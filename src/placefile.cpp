// OpenBlock place files: JSON save/load for the DataModel tree.
//
// The writer walks the DataModel tree and emits a JSON document; the loader
// parses it back with a small recursive-descent JSON parser and rebuilds the
// instances. The parser is written to be robust on the writer's own output
// (objects, arrays, strings with escapes incl. \uXXXX, numbers, true/false,
// null) and on hand-edited variants of it. Clean-room code.

#include "placefile.h"

#include "mathx.h" // brickColorByNumber (fills BrickColor name/color on load)
#include "rbx.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{

// ---------------------------------------------------------------------------
// JSON writer
// ---------------------------------------------------------------------------
void writeIndent(std::ostream& os, int level)
{
    for (int i = 0; i < level; ++i)
        os << "  ";
}

void writeJsonString(std::ostream& os, const std::string& s)
{
    os << '"';
    for (unsigned char ch : s)
    {
        switch (ch)
        {
        case '"':
            os << "\\\"";
            break;
        case '\\':
            os << "\\\\";
            break;
        case '\n':
            os << "\\n";
            break;
        case '\r':
            os << "\\r";
            break;
        case '\t':
            os << "\\t";
            break;
        default:
            if (ch < 0x20)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
                os << buf;
            }
            else
            {
                os << (char)ch;
            }
        }
    }
    os << '"';
}

void writeJsonNumber(std::ostream& os, double v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    os << buf;
}

void writeVec(std::ostream& os, const double* c, int n)
{
    os << '[';
    for (int i = 0; i < n; ++i)
    {
        if (i)
            os << ", ";
        writeJsonNumber(os, c[i]);
    }
    os << ']';
}

// Serializes one property value. Returns false for values with no JSON
// representation (empty, or Instance references); the caller skips those.
bool writePropValue(std::ostream& os, const Instance::PropDef& def, const PropValue& v)
{
    if (std::holds_alternative<std::monostate>(v))
        return false;
    const std::string& t = def.type;
    if (t == "string")
    {
        writeJsonString(os, std::get<std::string>(v));
        return true;
    }
    if (t == "number")
    {
        writeJsonNumber(os, std::get<double>(v));
        return true;
    }
    if (t == "bool")
    {
        os << (std::get<bool>(v) ? "true" : "false");
        return true;
    }
    if (t == "Vector3")
    {
        const Vec3& p = std::get<Vec3>(v);
        const double c[3] = {p.x, p.y, p.z};
        writeVec(os, c, 3);
        return true;
    }
    if (t == "Color3")
    {
        const Color3& p = std::get<Color3>(v);
        const double c[3] = {p.r, p.g, p.b};
        writeVec(os, c, 3);
        return true;
    }
    if (t == "CFrameV" || t == "CFrame") // the engine spells this type "CFrame"
    {
        const CFrameV& cf = std::get<CFrameV>(v);
        writeVec(os, cf.m, 12);
        return true;
    }
    if (t == "UDim2V")
    {
        const UDim2V& u = std::get<UDim2V>(v);
        const double c[4] = {u.xScale, u.xOffset, u.yScale, u.yOffset};
        writeVec(os, c, 4);
        return true;
    }
    if (t == "BrickColorV")
    {
        const BrickColorV& b = std::get<BrickColorV>(v);
        os << "{\"number\": " << b.number << '}';
        return true;
    }
    // "Instance" and anything unknown: not serializable.
    return false;
}

void writeNode(std::ostream& os, Instance* inst, int level)
{
    writeIndent(os, level);
    os << "{\n";
    writeIndent(os, level + 1);
    os << "\"class\": ";
    writeJsonString(os, inst->className());
    os << ",\n";
    writeIndent(os, level + 1);
    os << "\"name\": ";
    writeJsonString(os, inst->name());
    os << ",\n";
    writeIndent(os, level + 1);
    os << "\"props\": {";
    // Sorted keys for deterministic output.
    std::vector<std::string> keys;
    for (const auto& kv : inst->propDefs())
        keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    bool first = true;
    for (const std::string& k : keys)
    {
        if (k == "ClassName")
            continue; // read-only
        const Instance::PropDef& def = inst->propDefs().at(k);
        if (def.readOnly)
            continue;
        if (def.type == "Instance")
            continue; // object references are not serialized
        PropValue v;
        if (!inst->getProp(k, v))
            continue;
        std::ostringstream tmp;
        if (!writePropValue(tmp, def, v))
            continue;
        os << (first ? "\n" : ",\n");
        first = false;
        writeIndent(os, level + 2);
        writeJsonString(os, k);
        os << ": " << tmp.str();
    }
    if (!first)
    {
        os << '\n';
        writeIndent(os, level + 1);
    }
    os << "},\n";
    writeIndent(os, level + 1);
    os << "\"children\": [";
    const std::vector<Instance*>& kids = inst->children();
    if (!kids.empty())
    {
        os << '\n';
        for (size_t i = 0; i < kids.size(); ++i)
        {
            writeNode(os, kids[i], level + 2);
            os << (i + 1 < kids.size() ? ",\n" : "\n");
        }
        writeIndent(os, level + 1);
    }
    os << "]\n";
    writeIndent(os, level);
    os << '}';
}

// ---------------------------------------------------------------------------
// JSON parser (recursive descent; ~150 lines).
// ---------------------------------------------------------------------------
struct JVal
{
    enum class T
    {
        Null,
        Bool,
        Num,
        Str,
        Arr,
        Obj
    };
    T t = T::Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<JVal> a;
    std::map<std::string, JVal> o;

    const JVal* find(const std::string& key) const
    {
        if (t != T::Obj)
            return nullptr;
        auto it = o.find(key);
        return it == o.end() ? nullptr : &it->second;
    }
};

class JsonParser
{
public:
    JsonParser(const char* text, size_t len) : p(text), end(text + len) {}

    std::string error;

    bool parse(JVal& out)
    {
        skipWs();
        if (!parseValue(out))
            return false;
        skipWs();
        if (p != end)
            return fail("trailing characters after JSON document");
        return true;
    }

private:
    const char* p;
    const char* end;

    void skipWs()
    {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            ++p;
    }

    bool fail(const std::string& msg)
    {
        error = msg;
        return false;
    }

    bool parseValue(JVal& v)
    {
        if (p >= end)
            return fail("unexpected end of input");
        char c = *p;
        if (c == '{')
            return parseObject(v);
        if (c == '[')
            return parseArray(v);
        if (c == '"')
        {
            std::string s;
            if (!parseString(s))
                return false;
            v.t = JVal::T::Str;
            v.s = std::move(s);
            return true;
        }
        if (c == 't')
            return parseLit("true", JVal::T::Bool, v, true);
        if (c == 'f')
            return parseLit("false", JVal::T::Bool, v, false);
        if (c == 'n')
            return parseLit("null", JVal::T::Null, v, false);
        if (c == '-' || (c >= '0' && c <= '9'))
        {
            char* e = nullptr;
            double d = std::strtod(p, &e);
            if (e == p)
                return fail("invalid number");
            p = e;
            v.t = JVal::T::Num;
            v.n = d;
            return true;
        }
        return fail(std::string("unexpected character '") + c + "'");
    }

    bool parseLit(const char* lit, JVal::T t, JVal& v, bool bv)
    {
        size_t n = std::strlen(lit);
        if ((size_t)(end - p) < n || std::strncmp(p, lit, n) != 0)
            return fail(std::string("expected '") + lit + "'");
        p += n;
        v.t = t;
        v.b = bv;
        return true;
    }

    bool parseObject(JVal& v)
    {
        ++p; // '{'
        v.t = JVal::T::Obj;
        skipWs();
        if (p < end && *p == '}')
        {
            ++p;
            return true;
        }
        while (true)
        {
            skipWs();
            if (p >= end || *p != '"')
                return fail("expected string key in object");
            std::string key;
            if (!parseString(key))
                return false;
            skipWs();
            if (p >= end || *p != ':')
                return fail("expected ':' after object key");
            ++p;
            skipWs();
            JVal val;
            if (!parseValue(val))
                return false;
            v.o[std::move(key)] = std::move(val);
            skipWs();
            if (p >= end)
                return fail("unterminated object");
            if (*p == ',')
            {
                ++p;
                continue;
            }
            if (*p == '}')
            {
                ++p;
                return true;
            }
            return fail("expected ',' or '}' in object");
        }
    }

    bool parseArray(JVal& v)
    {
        ++p; // '['
        v.t = JVal::T::Arr;
        skipWs();
        if (p < end && *p == ']')
        {
            ++p;
            return true;
        }
        while (true)
        {
            skipWs();
            JVal val;
            if (!parseValue(val))
                return false;
            v.a.push_back(std::move(val));
            skipWs();
            if (p >= end)
                return fail("unterminated array");
            if (*p == ',')
            {
                ++p;
                continue;
            }
            if (*p == ']')
            {
                ++p;
                return true;
            }
            return fail("expected ',' or ']' in array");
        }
    }

    bool parseString(std::string& out)
    {
        ++p; // '"'
        while (p < end)
        {
            char c = *p++;
            if (c == '"')
                return true;
            if (c == '\\')
            {
                if (p >= end)
                    return fail("unterminated escape");
                char e = *p++;
                switch (e)
                {
                case '"':
                    out += '"';
                    break;
                case '\\':
                    out += '\\';
                    break;
                case '/':
                    out += '/';
                    break;
                case 'b':
                    out += '\b';
                    break;
                case 'f':
                    out += '\f';
                    break;
                case 'n':
                    out += '\n';
                    break;
                case 'r':
                    out += '\r';
                    break;
                case 't':
                    out += '\t';
                    break;
                case 'u':
                {
                    unsigned cp = 0;
                    if (!parseHex4(cp))
                        return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF && end - p >= 2 && p[0] == '\\' && p[1] == 'u')
                    {
                        p += 2;
                        unsigned lo = 0;
                        if (!parseHex4(lo))
                            return false;
                        if (lo < 0xDC00 || lo > 0xDFFF)
                            return fail("invalid low surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default:
                    return fail("invalid escape sequence");
                }
            }
            else if ((unsigned char)c < 0x20)
            {
                return fail("unescaped control character in string");
            }
            else
            {
                out += c;
            }
        }
        return fail("unterminated string");
    }

    bool parseHex4(unsigned& cp)
    {
        if (end - p < 4)
            return fail("truncated \\u escape");
        cp = 0;
        for (int i = 0; i < 4; ++i)
        {
            char c = p[i];
            cp <<= 4;
            if (c >= '0' && c <= '9')
                cp |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f')
                cp |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                cp |= (unsigned)(c - 'A' + 10);
            else
                return fail("invalid hex digit in \\u escape");
        }
        p += 4;
        return true;
    }

    static void appendUtf8(std::string& out, unsigned cp)
    {
        if (cp < 0x80)
        {
            out += (char)cp;
        }
        else if (cp < 0x800)
        {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
        else
        {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
};

// ---------------------------------------------------------------------------
// JSON value -> PropValue, guided by the property definition's type.
// ---------------------------------------------------------------------------
bool jsonToProp(const std::string& key, const Instance::PropDef& def, const JVal& jv,
                PropValue& out, std::string& why)
{
    const std::string& t = def.type;
    if (t == "string")
    {
        if (jv.t != JVal::T::Str)
        {
            why = "expected a JSON string";
            return false;
        }
        out = jv.s;
        return true;
    }
    if (t == "number")
    {
        if (jv.t != JVal::T::Num)
        {
            why = "expected a JSON number";
            return false;
        }
        out = jv.n;
        return true;
    }
    if (t == "bool")
    {
        if (jv.t != JVal::T::Bool)
        {
            why = "expected true/false";
            return false;
        }
        out = jv.b;
        return true;
    }
    if (t == "Vector3" || t == "Color3")
    {
        if (jv.t != JVal::T::Arr || jv.a.size() != 3)
        {
            why = "expected [x, y, z]";
            return false;
        }
        double c[3];
        for (int i = 0; i < 3; ++i)
        {
            if (jv.a[i].t != JVal::T::Num)
            {
                why = "expected numbers in [x, y, z]";
                return false;
            }
            c[i] = jv.a[i].n;
        }
        if (t == "Vector3")
            out = Vec3{c[0], c[1], c[2]};
        else
            out = Color3{c[0], c[1], c[2]};
        return true;
    }
    if (t == "CFrameV" || t == "CFrame") // the engine spells this type "CFrame"
    {
        if (jv.t != JVal::T::Arr || jv.a.size() != 12)
        {
            why = "expected 12 numbers";
            return false;
        }
        CFrameV cf;
        for (int i = 0; i < 12; ++i)
        {
            if (jv.a[i].t != JVal::T::Num)
            {
                why = "expected numbers in CFrame array";
                return false;
            }
            cf.m[i] = jv.a[i].n;
        }
        out = cf;
        return true;
    }
    if (t == "UDim2V")
    {
        if (jv.t != JVal::T::Arr || jv.a.size() != 4)
        {
            why = "expected 4 numbers";
            return false;
        }
        UDim2V u;
        double* f[4] = {&u.xScale, &u.xOffset, &u.yScale, &u.yOffset};
        for (int i = 0; i < 4; ++i)
        {
            if (jv.a[i].t != JVal::T::Num)
            {
                why = "expected numbers in UDim2 array";
                return false;
            }
            *f[i] = jv.a[i].n;
        }
        out = u;
        return true;
    }
    if (t == "BrickColorV")
    {
        const JVal* num = jv.find("number");
        if (jv.t != JVal::T::Obj || !num || num->t != JVal::T::Num)
        {
            why = "expected {\"number\": n}";
            return false;
        }
        BrickColorV bc;
        bc.number = (int)num->n;
        if (const BrickColorV* pal = brickColorByNumber(bc.number))
        {
            bc.name = pal->name;
            bc.color = pal->color;
        }
        out = bc;
        return true;
    }
    why = "unsupported property type '" + t + "'";
    return false;
}

// Applies one pass of properties. When cframeOnly is false every property
// except "CFrame" is set; when true only "CFrame" is set (so it runs last and
// wins over Position, matching Part's synced semantics).
void applyProps(Instance* target, const JVal& props, bool cframeOnly)
{
    for (const auto& kv : props.o)
    {
        const std::string& key = kv.first;
        if ((key == "CFrame") != cframeOnly)
            continue;
        if (key == "Name")
            continue; // already applied via setName
        const auto& defs = target->propDefs();
        auto it = defs.find(key);
        if (it == defs.end())
        {
            std::printf("[OpenBlox] place: unknown property '%s' on %s \"%s\" (skipped)\n",
                        key.c_str(), target->className().c_str(), target->name().c_str());
            continue;
        }
        const Instance::PropDef& def = it->second;
        if (def.readOnly)
            continue;
        if (def.type == "Instance")
        {
            std::printf("[OpenBlox] place: property '%s' is an object reference "
                        "(not serialized; skipped)\n",
                        key.c_str());
            continue;
        }
        PropValue v;
        std::string why;
        if (!jsonToProp(key, def, kv.second, v, why))
        {
            std::printf("[OpenBlox] place: bad value for '%s' on %s \"%s\": %s (skipped)\n",
                        key.c_str(), target->className().c_str(), target->name().c_str(),
                        why.c_str());
            continue;
        }
        std::string err = target->setProp(key, v);
        if (!err.empty())
            std::printf("[OpenBlox] place: could not set '%s' on %s \"%s\": %s\n", key.c_str(),
                        target->className().c_str(), target->name().c_str(), err.c_str());
    }
}

void loadNode(DataModel* game, Instance* parent, const JVal& node, bool topLevel)
{
    if (node.t != JVal::T::Obj)
    {
        std::printf("[OpenBlox] place: skipping malformed node (not an object)\n");
        return;
    }
    const JVal* cls = node.find("class");
    const JVal* nm = node.find("name");
    if (!cls || cls->t != JVal::T::Str || !nm || nm->t != JVal::T::Str)
    {
        std::printf("[OpenBlox] place: skipping node without class/name\n");
        return;
    }
    Instance* target = nullptr;
    if (topLevel)
    {
        // A top-level node that matches an existing service merges into it
        // instead of creating a duplicate.
        Instance* svc = game->getService(nm->s);
        if (svc && svc->className() == cls->s)
            target = svc;
    }
    else
    {
        // Merge into an existing same-class/same-name child instead of
        // duplicating it (e.g. the default Camera under Workspace), so that
        // save -> load -> save round-trips are stable.
        for (Instance* c : parent->children())
        {
            if (!c->isDestroyed() && c->className() == cls->s && c->name() == nm->s)
            {
                target = c;
                break;
            }
        }
    }
    if (!target)
    {
        target = game->createInstance(cls->s);
        if (!target)
        {
            std::printf("[OpenBlox] place: unknown class '%s' (skipped)\n", cls->s.c_str());
            return;
        }
        target->setName(nm->s);
        target->setParent(parent);
    }
    if (const JVal* props = node.find("props"); props && props->t == JVal::T::Obj)
    {
        applyProps(target, *props, false); // everything but CFrame
        applyProps(target, *props, true);  // CFrame last (wins over Position)
    }
    if (const JVal* kids = node.find("children"); kids && kids->t == JVal::T::Arr)
    {
        for (const JVal& k : kids->a)
            loadNode(game, target, k, false);
    }
}

} // namespace

bool oboxSavePlace(DataModel* game, const std::string& path)
{
    if (!game)
        return false;
    std::ofstream out(path, std::ios::trunc);
    if (!out)
        return false;
    out << "{\n  \"children\": [";
    const std::vector<Instance*>& kids = game->children();
    if (!kids.empty())
    {
        out << '\n';
        for (size_t i = 0; i < kids.size(); ++i)
        {
            writeNode(out, kids[i], 2);
            out << (i + 1 < kids.size() ? ",\n" : "\n");
        }
        out << "  ";
    }
    out << "]\n}\n";
    out.flush();
    return (bool)out;
}

bool oboxLoadPlace(DataModel* game, const std::string& path)
{
    if (!game)
        return false;
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    JVal root;
    JsonParser parser(text.data(), text.size());
    if (!parser.parse(root))
    {
        std::printf("[OpenBlox] place: JSON parse error in %s: %s\n", path.c_str(),
                    parser.error.c_str());
        return false;
    }
    const JVal* arr = nullptr;
    if (root.t == JVal::T::Obj)
        arr = root.find("children");
    else if (root.t == JVal::T::Arr)
        arr = &root; // also accept a bare array of nodes
    if (!arr || arr->t != JVal::T::Arr)
    {
        std::printf("[OpenBlox] place: %s has no top-level children array\n", path.c_str());
        return false;
    }
    for (const JVal& n : arr->a)
        loadNode(game, game, n, true);
    return true;
}
