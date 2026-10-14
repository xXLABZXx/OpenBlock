// OpenBlox Luau bindings implementation. Clean-room code.
//
// Maps the prototype DataModel onto Luau userdata with metatables:
//   game            -> DataModel userdata ("OpenBlox.DataModel")
//   Instance.new()  -> Instance userdata ("OpenBlox.Instance")
//   signal          -> Signal userdata ("OpenBlox.Signal")
//   conn            -> Connection userdata ("OpenBlox.Connection")
//   Vector3.new()   -> Vector3 userdata ("OpenBlox.Vector3")
#include "bindings.h"
#include "mathx.h"
#include "services.h"
#include "physics.h"

#include <lua.h>
#include <lualib.h>
#include <luacode.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Host lookup
// ---------------------------------------------------------------------------
static Host* getHost(lua_State* L)
{
    return oboxGetHost(L);
}

Host* oboxGetHost(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "OpenBlox.Host");
    Host* h = (Host*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return h;
}

// ---------------------------------------------------------------------------
// Instance userdata
// ---------------------------------------------------------------------------
struct UInstance
{
    Instance* ptr;
};

static bool isInstanceUd(lua_State* L, int idx);

static UInstance* checkInstance(lua_State* L, int idx)
{
    // Accept both the Instance and DataModel metatables: luaL_checkudata
    // raises on mismatch instead of returning null, so the old fallback
    // after it was dead code (tostring(game) / game == game errored).
    if (isInstanceUd(L, idx))
        return (UInstance*)lua_touserdata(L, idx);
    // Standard type-error message for the common case.
    return (UInstance*)luaL_checkudata(L, idx, "OpenBlox.Instance");
}

static bool isInstanceUd(lua_State* L, int idx)
{
    if (lua_type(L, idx) != LUA_TUSERDATA)
        return false;
    if (!lua_getmetatable(L, idx))
        return false;
    luaL_getmetatable(L, "OpenBlox.Instance");
    bool a = lua_rawequal(L, -1, -2) != 0;
    lua_pop(L, 1);
    luaL_getmetatable(L, "OpenBlox.DataModel");
    bool b = lua_rawequal(L, -1, -2) != 0;
    lua_pop(L, 2);
    return a || b;
}

static void pushInstance(lua_State* L, Instance* inst)
{
    if (!inst)
    {
        lua_pushnil(L);
        return;
    }
    // Identity cache: one userdata per Instance, so `part == workspace.Part`,
    // table keys, and event payloads all agree on object identity.
    Host* h = oboxGetHost(L);
    auto it = h->instanceRefs.find(inst);
    if (it != h->instanceRefs.end())
    {
        lua_getref(L, it->second);
        return;
    }
    UInstance* u = (UInstance*)lua_newuserdata(L, sizeof(UInstance));
    u->ptr = inst;
    luaL_getmetatable(L, inst->className() == "DataModel" ? "OpenBlox.DataModel"
                                                         : "OpenBlox.Instance");
    lua_setmetatable(L, -2);
    // Luau's lua_ref does NOT pop the value (unlike PUC luaL_ref), so no
    // duplication is needed: the userdata stays on top for the caller.
    int ref = lua_ref(L, -1);
    h->instanceRefs[inst] = ref;
}

// Shared instance helpers used by the other binding modules.
Instance* oboxCheckInstance(lua_State* L, int idx)
{
    return checkInstance(L, idx)->ptr;
}

bool oboxIsInstance(lua_State* L, int idx)
{
    return isInstanceUd(L, idx);
}

void oboxPushInstance(lua_State* L, Instance* inst)
{
    pushInstance(L, inst);
}

// ---------------------------------------------------------------------------
// Signal userdata + connections
// ---------------------------------------------------------------------------
struct USignal
{
    Signal* ptr;
};

struct UConn
{
    Signal* sig;
    int id;
};

static void pushSignal(lua_State* L, Signal* sig)
{
    USignal* u = (USignal*)lua_newuserdata(L, sizeof(USignal));
    u->ptr = sig;
    luaL_getmetatable(L, "OpenBlox.Signal");
    lua_setmetatable(L, -2);
}

static int oboxSignalConnect(lua_State* L)
{
    USignal* s = (USignal*)luaL_checkudata(L, 1, "OpenBlox.Signal");
    luaL_checktype(L, 2, LUA_TFUNCTION);
    Host* h = getHost(L);
    (void)h;

    lua_pushvalue(L, 2);
    int ref = lua_ref(L, -1);
    lua_pop(L, 1);
    int id = s->ptr->connect(h->L, L, ref);
    if (id == 0)
    {
        lua_unref(L, ref);
        luaL_errorL(L, "signal handler limit reached");
    }

    UConn* c = (UConn*)lua_newuserdata(L, sizeof(UConn));
    c->sig = s->ptr;
    c->id = id;
    luaL_getmetatable(L, "OpenBlox.Connection");
    lua_setmetatable(L, -2);
    return 1;
}

static int oboxSignalOnce(lua_State* L)
{
    USignal* s = (USignal*)luaL_checkudata(L, 1, "OpenBlox.Signal");
    luaL_checktype(L, 2, LUA_TFUNCTION);
    Host* h = getHost(L);
    lua_pushvalue(L, 2);
    int ref = lua_ref(L, -1);
    lua_pop(L, 1);
    int id = s->ptr->connect(h->L, L, ref, true);
    if (id == 0)
    {
        lua_unref(L, ref);
        luaL_errorL(L, "signal handler limit reached");
    }

    UConn* c = (UConn*)lua_newuserdata(L, sizeof(UConn));
    c->sig = s->ptr;
    c->id = id;
    luaL_getmetatable(L, "OpenBlox.Connection");
    lua_setmetatable(L, -2);
    return 1;
}

static int oboxSignalEq(lua_State* L)
{
    USignal* a = (USignal*)luaL_checkudata(L, 1, "OpenBlox.Signal");
    USignal* b = (USignal*)luaL_checkudata(L, 2, "OpenBlox.Signal");
    lua_pushboolean(L, a->ptr == b->ptr);
    return 1;
}

static int oboxSignalWait(lua_State* L)
{
    USignal* s = (USignal*)luaL_checkudata(L, 1, "OpenBlox.Signal");
    Host* h = getHost(L);
    s->ptr->addWaiter(L);
    // Park (don't sleep): the thread stays suspended with no wake time until
    // the signal fires. Parking makes the yield scheduler-visible, so the
    // "yielded outside of task.wait()" check passes, the registry anchor is
    // retained, and task.cancel can drop the waiter.
    h->sched->park(L);
    return lua_yield(L, 0);
}

static int oboxConnDisconnect(lua_State* L)
{
    UConn* c = (UConn*)luaL_checkudata(L, 1, "OpenBlox.Connection");
    // Prototype note: the registry ref held by the handler is released on
    // process exit; see docs/COMPATIBILITY.md "known prototype limitations".
    c->sig->disconnect(c->id);
    return 0;
}

// ---------------------------------------------------------------------------
// Vector3 userdata
// ---------------------------------------------------------------------------
struct UVec3
{
    double x, y, z;
};

static UVec3* checkVec3(lua_State* L, int idx)
{
    return (UVec3*)luaL_checkudata(L, idx, "OpenBlox.Vector3");
}

static void pushVec3(lua_State* L, double x, double y, double z)
{
    UVec3* v = (UVec3*)lua_newuserdata(L, sizeof(UVec3));
    v->x = x;
    v->y = y;
    v->z = z;
    luaL_getmetatable(L, "OpenBlox.Vector3");
    lua_setmetatable(L, -2);
}

static int vec3New(lua_State* L)
{
    double x = luaL_checknumber(L, 1);
    double y = luaL_checknumber(L, 2);
    double z = luaL_checknumber(L, 3);
    pushVec3(L, x, y, z);
    return 1;
}

static int vec3Index(lua_State* L)
{
    UVec3* v = checkVec3(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (strcmp(k, "x") == 0) { lua_pushnumber(L, v->x); return 1; }
    if (strcmp(k, "y") == 0) { lua_pushnumber(L, v->y); return 1; }
    if (strcmp(k, "z") == 0) { lua_pushnumber(L, v->z); return 1; }
    if (strcmp(k, "Magnitude") == 0)
    {
        lua_pushnumber(L, sqrt(v->x * v->x + v->y * v->y + v->z * v->z));
        return 1;
    }
    luaL_errorL(L, "%s is not a valid member of Vector3", k);
}

static int vec3Add(lua_State* L)
{
    UVec3* a = checkVec3(L, 1);
    UVec3* b = checkVec3(L, 2);
    pushVec3(L, a->x + b->x, a->y + b->y, a->z + b->z);
    return 1;
}

static int vec3Sub(lua_State* L)
{
    UVec3* a = checkVec3(L, 1);
    UVec3* b = checkVec3(L, 2);
    pushVec3(L, a->x - b->x, a->y - b->y, a->z - b->z);
    return 1;
}

static int vec3Mul(lua_State* L)
{
    if (lua_type(L, 1) == LUA_TUSERDATA)
    {
        UVec3* a = checkVec3(L, 1);
        double s = luaL_checknumber(L, 2);
        pushVec3(L, a->x * s, a->y * s, a->z * s);
    }
    else
    {
        double s = luaL_checknumber(L, 1);
        UVec3* a = checkVec3(L, 2);
        pushVec3(L, a->x * s, a->y * s, a->z * s);
    }
    return 1;
}

static int vec3Div(lua_State* L)
{
    UVec3* a = checkVec3(L, 1);
    double s = luaL_checknumber(L, 2);
    pushVec3(L, a->x / s, a->y / s, a->z / s);
    return 1;
}

static int vec3Unm(lua_State* L)
{
    UVec3* a = checkVec3(L, 1);
    pushVec3(L, -a->x, -a->y, -a->z);
    return 1;
}

static int vec3Eq(lua_State* L)
{
    UVec3* a = checkVec3(L, 1);
    UVec3* b = checkVec3(L, 2);
    lua_pushboolean(L, a->x == b->x && a->y == b->y && a->z == b->z);
    return 1;
}

static int vec3ToString(lua_State* L)
{
    UVec3* a = checkVec3(L, 1);
    char buf[96];
    snprintf(buf, sizeof(buf), "(%g, %g, %g)", a->x, a->y, a->z);
    lua_pushstring(L, buf);
    return 1;
}

// ---------------------------------------------------------------------------
// Color3 userdata
// ---------------------------------------------------------------------------
struct UC3
{
    double r, g, b;
};

static UC3* checkColor3(lua_State* L, int idx)
{
    return (UC3*)luaL_checkudata(L, idx, "OpenBlox.Color3");
}

static bool isColor3Ud(lua_State* L, int idx)
{
    if (lua_type(L, idx) != LUA_TUSERDATA || !lua_getmetatable(L, idx))
        return false;
    luaL_getmetatable(L, "OpenBlox.Color3");
    bool eq = lua_rawequal(L, -1, -2) != 0;
    lua_pop(L, 2);
    return eq;
}

static bool isVec3Ud(lua_State* L, int idx)
{
    if (lua_type(L, idx) != LUA_TUSERDATA || !lua_getmetatable(L, idx))
        return false;
    luaL_getmetatable(L, "OpenBlox.Vector3");
    bool eq = lua_rawequal(L, -1, -2) != 0;
    lua_pop(L, 2);
    return eq;
}

static void pushColor3(lua_State* L, double r, double g, double b)
{
    UC3* c = (UC3*)lua_newuserdata(L, sizeof(UC3));
    c->r = r;
    c->g = g;
    c->b = b;
    luaL_getmetatable(L, "OpenBlox.Color3");
    lua_setmetatable(L, -2);
}

static int color3New(lua_State* L)
{
    pushColor3(L, luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3));
    return 1;
}

static int color3FromRGB(lua_State* L)
{
    pushColor3(L, luaL_checknumber(L, 1) / 255.0, luaL_checknumber(L, 2) / 255.0,
               luaL_checknumber(L, 3) / 255.0);
    return 1;
}

static int color3Index(lua_State* L)
{
    UC3* c = checkColor3(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (strcmp(k, "r") == 0 || strcmp(k, "R") == 0) { lua_pushnumber(L, c->r); return 1; }
    if (strcmp(k, "g") == 0 || strcmp(k, "G") == 0) { lua_pushnumber(L, c->g); return 1; }
    if (strcmp(k, "b") == 0 || strcmp(k, "B") == 0) { lua_pushnumber(L, c->b); return 1; }
    luaL_errorL(L, "%s is not a valid member of Color3", k);
}

static int color3Eq(lua_State* L)
{
    UC3* a = checkColor3(L, 1);
    UC3* b = checkColor3(L, 2);
    lua_pushboolean(L, a->r == b->r && a->g == b->g && a->b == b->b);
    return 1;
}

static int color3ToString(lua_State* L)
{
    UC3* c = checkColor3(L, 1);
    char buf[96];
    snprintf(buf, sizeof(buf), "(%g, %g, %g)", c->r, c->g, c->b);
    lua_pushstring(L, buf);
    return 1;
}

// ---------------------------------------------------------------------------
// Property bridging
// ---------------------------------------------------------------------------
static void pushColor3(lua_State* L, double r, double g, double b); // fwd

static void pushPropValue(lua_State* L, const PropValue& v)
{
    if (std::holds_alternative<double>(v))
        lua_pushnumber(L, std::get<double>(v));
    else if (std::holds_alternative<std::string>(v))
        lua_pushstring(L, std::get<std::string>(v).c_str());
    else if (std::holds_alternative<bool>(v))
        lua_pushboolean(L, std::get<bool>(v));
    else if (std::holds_alternative<Vec3>(v))
    {
        Vec3 p = std::get<Vec3>(v);
        pushVec3(L, p.x, p.y, p.z);
    }
    else if (std::holds_alternative<Color3>(v))
    {
        Color3 c = std::get<Color3>(v);
        pushColor3(L, c.r, c.g, c.b);
    }
    else if (std::holds_alternative<Instance*>(v))
        pushInstance(L, std::get<Instance*>(v));
    else if (std::holds_alternative<CFrameV>(v))
        oboxPushCFrame(L, std::get<CFrameV>(v));
    else if (std::holds_alternative<UDim2V>(v))
        oboxPushUDim2(L, std::get<UDim2V>(v));
    else if (std::holds_alternative<BrickColorV>(v))
        oboxPushBrickColor(L, std::get<BrickColorV>(v));
    else
        lua_pushnil(L);
}

static void pushAttrValue(lua_State* L, const AttrValue& v)
{
    if (std::holds_alternative<double>(v))
        lua_pushnumber(L, std::get<double>(v));
    else if (std::holds_alternative<std::string>(v))
        lua_pushstring(L, std::get<std::string>(v).c_str());
    else if (std::holds_alternative<bool>(v))
        lua_pushboolean(L, std::get<bool>(v));
    else if (std::holds_alternative<Vec3>(v))
    {
        Vec3 p = std::get<Vec3>(v);
        pushVec3(L, p.x, p.y, p.z);
    }
    else if (std::holds_alternative<Color3>(v))
    {
        Color3 c = std::get<Color3>(v);
        pushColor3(L, c.r, c.g, c.b);
    }
    else
        lua_pushnil(L);
}

// Converts the Lua value at idx to the property type. Returns "" on success.
static std::string toPropValue(lua_State* L, int idx, const std::string& type, PropValue& out)
{
    if (type == "string")
    {
        if (!lua_isstring(L, idx))
            return "string expected";
        out = std::string(lua_tostring(L, idx));
        return "";
    }
    if (type == "number")
    {
        if (!lua_isnumber(L, idx))
            return "number expected";
        out = lua_tonumber(L, idx);
        return "";
    }
    if (type == "bool")
    {
        if (lua_type(L, idx) != LUA_TBOOLEAN)
            return "boolean expected";
        out = (bool)lua_toboolean(L, idx);
        return "";
    }
    if (type == "Vector3")
    {
        UVec3* v = (UVec3*)luaL_checkudata(L, idx, "OpenBlox.Vector3");
        out = Vec3{v->x, v->y, v->z};
        return "";
    }
    if (type == "Color3")
    {
        UC3* c = (UC3*)luaL_checkudata(L, idx, "OpenBlox.Color3");
        out = Color3{c->r, c->g, c->b};
        return "";
    }
    if (type == "CFrame")
    {
        if (!oboxIsCFrame(L, idx))
            return "CFrame expected";
        out = oboxCheckCFrame(L, idx)->c;
        return "";
    }
    if (type == "UDim2")
    {
        if (!oboxIsUDim2(L, idx))
            return "UDim2 expected";
        out = oboxCheckUDim2(L, idx)->v;
        return "";
    }
    if (type == "BrickColor")
    {
        if (!oboxIsBrickColor(L, idx))
            return "BrickColor expected";
        UBrickColor* ubc = oboxCheckBrickColor(L, idx);
        size_t bcc = 0;
        const BrickColorV* bcp = brickColorPalette(bcc);
        int bci = ubc->paletteIndex;
        out = bcp[(bci < 0 || (size_t)bci >= bcc) ? 0 : bci];
        return "";
    }
    if (type == "Instance")
    {
        if (lua_isnil(L, idx))
        {
            out = static_cast<Instance*>(nullptr);
            return "";
        }
        if (!isInstanceUd(L, idx))
            return "Instance expected";
        out = checkInstance(L, idx)->ptr;
        return "";
    }
    return "unsupported property type";
}

// Converts a Lua value to an attribute value (best-effort type mapping).
static bool toAttrValue(lua_State* L, int idx, AttrValue& out){
    int t = lua_type(L, idx);
    if (t == LUA_TNUMBER) { out = lua_tonumber(L, idx); return true; }
    if (t == LUA_TSTRING) { out = std::string(lua_tostring(L, idx)); return true; }
    if (t == LUA_TBOOLEAN) { out = (bool)lua_toboolean(L, idx); return true; }
    if (t == LUA_TUSERDATA)
    {
        if (isVec3Ud(L, idx))
        {
            UVec3* v = checkVec3(L, idx);
            out = Vec3{v->x, v->y, v->z};
            return true;
        }
        if (isColor3Ud(L, idx))
        {
            UC3* c = checkColor3(L, idx);
            out = Color3{c->r, c->g, c->b};
            return true;
        }
        // CFrame, UDim2 and BrickColor have no AttrValue representation (the
        // AttrValue variant only covers the core scalar/vector types), so they
        // are rejected as attributes rather than silently dropped.
        if (oboxIsCFrame(L, idx) || oboxIsUDim2(L, idx) || oboxIsBrickColor(L, idx))
            return false;
    }
    return false;
}

// Shared property-value helpers used by the other binding modules
// (declared in bindings.h).
void oboxPushPropValue(lua_State* L, const PropValue& v)
{
    pushPropValue(L, v);
}

bool oboxToPropValue(lua_State* L, int idx, const std::string& type, PropValue& out,
                     std::string& err)
{
    err = toPropValue(L, idx, type, out);
    return err.empty();
}

// ---------------------------------------------------------------------------
// Instance methods exposed to Luau
// ---------------------------------------------------------------------------
static int instGetChildren(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    lua_newtable(L);
    int i = 1;
    for (Instance* c : u->ptr->children())
    {
        pushInstance(L, c);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

static int instFindFirstChild(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    const char* name = luaL_checkstring(L, 2);
    bool recursive = lua_toboolean(L, 3) != 0;
    pushInstance(L, u->ptr->findFirstChild(name, recursive));
    return 1;
}

// WaitForChild is implemented in Lua (see OB.WaitForChild in oboxRegisterApi)
// because C functions cannot yield in a loop and resume correctly.
static int instWaitForChild(lua_State* L)
{
    luaL_errorL(L, "WaitForChild: use the Lua implementation");
    return 0;
}

static int instDestroy(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    u->ptr->destroy();
    return 0;
}

static int instIsA(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    const char* cn = luaL_checkstring(L, 2);
    lua_pushboolean(L, u->ptr->isA(cn));
    return 1;
}

static int instGetFullName(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    lua_pushstring(L, u->ptr->getFullName().c_str());
    return 1;
}

static int instGetDescendants(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    std::vector<Instance*> out;
    u->ptr->getDescendants(out);
    lua_newtable(L);
    for (size_t i = 0; i < out.size(); ++i)
    {
        pushInstance(L, out[i]);
        lua_rawseti(L, -2, (int)i + 1);
    }
    return 1;
}

static int instClearAllChildren(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    Host* h = getHost(L);
    std::vector<Instance*> kids = u->ptr->children();
    for (Instance* c : kids)
    {
        c->setParent(nullptr);
        pushInstance(L, c);
        u->ptr->childRemoved.fire(*h->sched, L, 1);
        lua_pop(L, 1);
    }
    return 0;
}

static int instFindFirstChildOfClass(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    const char* cn = luaL_checkstring(L, 2);
    pushInstance(L, u->ptr->findFirstChildOfClass(cn));
    return 1;
}

static int instFindFirstChildWhichIsA(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    const char* cn = luaL_checkstring(L, 2);
    bool recursive = lua_toboolean(L, 3) != 0;
    pushInstance(L, u->ptr->findFirstChildWhichIsA(cn, recursive));
    return 1;
}

// Deep clone helper: same class, copied name/props/attributes/tags/children.
// `seen` guards against cyclic source trees (belt-and-braces alongside the
// setParent cycle rejection): a source already on the clone path is skipped.
static Instance* cloneTree(Host* h, Instance* src, std::unordered_set<Instance*>& seen)
{
    if (!seen.insert(src).second)
        return nullptr;
    Instance* dst = h->game->createInstance(src->className());
    if (!dst)
        return nullptr;
    dst->setName(src->name());
    for (auto& kv : src->propDefs())
    {
        if (kv.first == "Name" || kv.first == "ClassName")
            continue;
        PropValue v;
        if (src->getProp(kv.first, v))
            dst->setProp(kv.first, v);
    }
    for (auto& kv : src->attributes())
        dst->setAttribute(kv.first, kv.second);
    for (auto& t : src->getTags())
        dst->addTag(t);
    for (Instance* c : src->children())
    {
        Instance* cc = cloneTree(h, c, seen);
        if (cc)
            cc->setParent(dst);
    }
    return dst;
}

static int instClone(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    Host* h = getHost(L);
    std::unordered_set<Instance*> seen;
    Instance* c = cloneTree(h, u->ptr, seen);
    if (!c)
        luaL_errorL(L, "Clone failed for class %s", u->ptr->className().c_str());
    pushInstance(L, c);
    return 1;
}

static int instGetAttribute(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    const char* key = luaL_checkstring(L, 2);
    AttrValue v;
    if (u->ptr->getAttribute(key, v))
        pushAttrValue(L, v);
    else
        lua_pushnil(L);
    return 1;
}

static int instSetAttribute(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    const char* key = luaL_checkstring(L, 2);
    if (lua_isnil(L, 3))
    {
        u->ptr->setAttribute(key, std::monostate{});
        return 0;
    }
    AttrValue v;
    if (!toAttrValue(L, 3, v))
        luaL_errorL(L, "Unsupported attribute value type for '%s'", key);
    u->ptr->setAttribute(key, v);
    return 0;
}

static int instAddTag(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    u->ptr->addTag(luaL_checkstring(L, 2));
    return 0;
}

static int instRemoveTag(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    u->ptr->removeTag(luaL_checkstring(L, 2));
    return 0;
}

static int instHasTag(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    lua_pushboolean(L, u->ptr->hasTag(luaL_checkstring(L, 2)));
    return 1;
}

static int instGetTags(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    std::vector<std::string> tags = u->ptr->getTags();
    lua_newtable(L);
    for (size_t i = 0; i < tags.size(); ++i)
    {
        lua_pushstring(L, tags[i].c_str());
        lua_rawseti(L, -2, (int)i + 1);
    }
    return 1;
}

static int instGetPropertyChangedSignal(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    const char* prop = luaL_checkstring(L, 2);
    if (u->ptr->propDefs().find(prop) == u->ptr->propDefs().end())
        luaL_errorL(L, "%s is not a valid member of %s", prop, u->ptr->className().c_str());
    pushSignal(L, u->ptr->getPropertyChangedSignal(prop));
    return 1;
}

// Shared __index logic for Instance and DataModel userdata.
static int instanceIndexImpl(lua_State* L, Instance* inst)
{
    if (inst->isDestroyed())
        luaL_errorL(L, "Attempt to index destroyed instance");
    const char* key = luaL_checkstring(L, 2);

    // Signals: Changed/ChildAdded/ChildRemoved on Instance, Touched/TouchEnded
    // on Part, Heartbeat on RunService (see findSignal overrides in rbx.h).
    if (Signal* sig = inst->findSignal(key))
    {
        pushSignal(L, sig);
        return 1;
    }

    // Per-module method dispatch (services, physics). Each returns true when
    // it handled the key, leaving the method closure on the stack.
    if (oboxServicesIndex(L, inst, key))
        return 1;
    if (oboxPhysicsIndex(L, inst, key))
        return 1;

    // RunService methods (v0.1: headless server context).
    if (inst->className() == "RunService")
    {
        if (strcmp(key, "IsServer") == 0)
        {
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_pushboolean(L, 1);
                return 1;
            }, "IsServer");
            return 1;
        }
        if (strcmp(key, "IsClient") == 0 || strcmp(key, "IsStudio") == 0)
        {
            lua_pushcfunction(L, [](lua_State* L) -> int {
                lua_pushboolean(L, 0);
                return 1;
            }, "IsClient");
            return 1;
        }
    }

    // Methods
    if (strcmp(key, "GetChildren") == 0) { lua_pushcfunction(L, instGetChildren, "GetChildren"); return 1; }
    if (strcmp(key, "FindFirstChild") == 0) { lua_pushcfunction(L, instFindFirstChild, "FindFirstChild"); return 1; }
    if (strcmp(key, "FindFirstChildOfClass") == 0) { lua_pushcfunction(L, instFindFirstChildOfClass, "FindFirstChildOfClass"); return 1; }
    if (strcmp(key, "FindFirstChildWhichIsA") == 0) { lua_pushcfunction(L, instFindFirstChildWhichIsA, "FindFirstChildWhichIsA"); return 1; }
    if (strcmp(key, "WaitForChild") == 0) { lua_getfield(L, LUA_REGISTRYINDEX, "OB.WaitForChild"); return 1; }
    if (strcmp(key, "GetDescendants") == 0) { lua_pushcfunction(L, instGetDescendants, "GetDescendants"); return 1; }
    if (strcmp(key, "ClearAllChildren") == 0) { lua_pushcfunction(L, instClearAllChildren, "ClearAllChildren"); return 1; }
    if (strcmp(key, "Destroy") == 0) { lua_pushcfunction(L, instDestroy, "Destroy"); return 1; }
    if (strcmp(key, "Clone") == 0) { lua_pushcfunction(L, instClone, "Clone"); return 1; }
    if (strcmp(key, "IsA") == 0) { lua_pushcfunction(L, instIsA, "IsA"); return 1; }
    if (strcmp(key, "GetFullName") == 0) { lua_pushcfunction(L, instGetFullName, "GetFullName"); return 1; }
    if (strcmp(key, "GetAttribute") == 0) { lua_pushcfunction(L, instGetAttribute, "GetAttribute"); return 1; }
    if (strcmp(key, "SetAttribute") == 0) { lua_pushcfunction(L, instSetAttribute, "SetAttribute"); return 1; }
    if (strcmp(key, "AddTag") == 0) { lua_pushcfunction(L, instAddTag, "AddTag"); return 1; }
    if (strcmp(key, "RemoveTag") == 0) { lua_pushcfunction(L, instRemoveTag, "RemoveTag"); return 1; }
    if (strcmp(key, "HasTag") == 0) { lua_pushcfunction(L, instHasTag, "HasTag"); return 1; }
    if (strcmp(key, "GetTags") == 0) { lua_pushcfunction(L, instGetTags, "GetTags"); return 1; }
    if (strcmp(key, "GetPropertyChangedSignal") == 0) { lua_pushcfunction(L, instGetPropertyChangedSignal, "GetPropertyChangedSignal"); return 1; }

    // Parent is virtual (not in the property table).
    if (strcmp(key, "Parent") == 0) { pushInstance(L, inst->parent()); return 1; }

    PropValue v;
    if (inst->getProp(key, v))
    {
        pushPropValue(L, v);
        return 1;
    }
    luaL_errorL(L, "%s is not a valid member of %s \"%s\"", key, inst->className().c_str(),
                       inst->name().c_str());
}

static int instanceIndex(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    return instanceIndexImpl(L, u->ptr);
}

static int instanceNewIndex(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    Instance* inst = u->ptr;
    if (inst->isDestroyed())
        luaL_errorL(L, "Attempt to set property of destroyed instance");
    const char* key = luaL_checkstring(L, 2);
    Host* h = getHost(L);

    if (strcmp(key, "Parent") == 0)
    {
        Instance* oldp = inst->parent();
        Instance* newp = nullptr;
        if (!lua_isnil(L, 3))
        {
            if (!isInstanceUd(L, 3))
                luaL_errorL(L, "Parent must be an Instance or nil");
            newp = checkInstance(L, 3)->ptr;
        }
        if (!inst->setParent(newp))
            luaL_errorL(L, "Parenting cycle: cannot set Parent");
        if (oldp && oldp != newp)
        {
            pushInstance(L, inst);
            oldp->childRemoved.fire(*h->sched, L, 1);
            lua_pop(L, 1);
        }
        if (newp && oldp != newp)
        {
            pushInstance(L, inst);
            newp->childAdded.fire(*h->sched, L, 1);
            lua_pop(L, 1);
        }
        return 0;
    }

    auto it = inst->propDefs().find(key);
    if (it == inst->propDefs().end())
        luaL_errorL(L, "%s is not a valid member of %s \"%s\"", key, inst->className().c_str(),
                           inst->name().c_str());

    PropValue v;
    std::string convErr = toPropValue(L, 3, it->second.type, v);
    if (!convErr.empty())
        luaL_errorL(L, "Invalid value for %s: %s", key, convErr.c_str());

    std::string err = inst->setProp(key, v);
    if (!err.empty())
        luaL_errorL(L, "%s", err.c_str());

    // Changed fires deferred, like Roblox.
    lua_pushstring(L, key);
    inst->changed.fire(*h->sched, L, 1);
    lua_pop(L, 1);
    // Per-property signal (GetPropertyChangedSignal).
    inst->getPropertyChangedSignal(key)->fire(*h->sched, L, 0);
    return 0;
}

static int instanceToString(lua_State* L)
{
    UInstance* u = checkInstance(L, 1);
    lua_pushstring(L, u->ptr->name().c_str());
    return 1;
}

static int instanceEq(lua_State* L)
{
    UInstance* a = checkInstance(L, 1);
    UInstance* b = checkInstance(L, 2);
    lua_pushboolean(L, a->ptr == b->ptr);
    return 1;
}

// ---------------------------------------------------------------------------
// game (DataModel) __index: services first, then generic instance logic.
// ---------------------------------------------------------------------------
static int gameGetService(lua_State* L)
{
    UInstance* u = (UInstance*)luaL_checkudata(L, 1, "OpenBlox.DataModel");
    const char* name = luaL_checkstring(L, 2);
    Host* h = getHost(L);
    Instance* svc = h->game->getService(name);
    if (!svc)
        luaL_errorL(L, "\"%s\" is not a valid service", name);
    pushInstance(L, svc);
    return 1;
}

static int gameIndex(lua_State* L)
{
    UInstance* u = (UInstance*)luaL_checkudata(L, 1, "OpenBlox.DataModel");
    Host* h = getHost(L);
    const char* key = luaL_checkstring(L, 2);
    if (strcmp(key, "GetService") == 0)
    {
        lua_pushcfunction(L, gameGetService, "GetService");
        return 1;
    }
    if (Instance* svc = h->game->getService(key))
    {
        pushInstance(L, svc);
        return 1;
    }
    return instanceIndexImpl(L, u->ptr);
}

// ---------------------------------------------------------------------------
// Instance.new
// ---------------------------------------------------------------------------
static int oboxInstanceNew(lua_State* L)
{
    const char* className = luaL_checkstring(L, 1);
    Host* h = getHost(L);
    if (h->game->overInstanceCap())
        luaL_errorL(L, "Instance limit reached (%d); cannot create \"%s\"",
                    (int)DataModel::kMaxInstances, className);
    Instance* inst = h->game->createInstance(className);
    if (!inst)
        luaL_errorL(L, "Unable to create an Instance of type \"%s\"", className);
    if (!lua_isnoneornil(L, 2))
    {
        UInstance* p = checkInstance(L, 2);
        inst->setParent(p->ptr);
        pushInstance(L, inst);
        p->ptr->childAdded.fire(*h->sched, L, 1);
        lua_pop(L, 1);
    }
    pushInstance(L, inst);
    return 1;
}

// ---------------------------------------------------------------------------
// task library
// ---------------------------------------------------------------------------
static int taskSpawnDefer(lua_State* L)
{
    Host* h = getHost(L);
    luaL_checktype(L, 1, LUA_TFUNCTION);
    int nargs = lua_gettop(L) - 1;
    lua_State* co = lua_newthread(L); // [..., thread]; inherits caller's globals
    int coIdx = lua_gettop(L);
    for (int i = 1; i < coIdx; ++i)
        lua_pushvalue(L, i);
    lua_xmove(L, co, nargs + 1);
    h->sched->deferThread(co, nargs, 0.0);
    return 1; // the thread
}

static int taskDelay(lua_State* L)
{
    Host* h = getHost(L);
    double d = luaL_checknumber(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    int nargs = lua_gettop(L) - 2;
    lua_State* co = lua_newthread(L);
    int coIdx = lua_gettop(L);
    for (int i = 2; i < coIdx; ++i)
        lua_pushvalue(L, i);
    lua_xmove(L, co, nargs + 1);
    h->sched->deferThread(co, nargs, d < 0 ? 0 : d);
    return 1; // the thread
}

static int taskCancel(lua_State* L)
{
    Host* h = getHost(L);
    luaL_checktype(L, 1, LUA_TTHREAD);
    h->sched->cancel(lua_tothread(L, 1));
    return 0;
}

static int taskWait(lua_State* L)
{
    Host* h = getHost(L);
    double s = luaL_optnumber(L, 1, 0.0);
    if (L == h->sched->mainThread())
        luaL_errorL(L, "task.wait cannot yield the main thread");
    h->sched->wait(L, s);
    return lua_yield(L, 0);
}

// ---------------------------------------------------------------------------
// tick / wait / print / warn
// ---------------------------------------------------------------------------
static int oboxTick(lua_State* L)
{
    Host* h = getHost(L);
    lua_pushnumber(L, h->sched->now());
    return 1;
}

static int oboxPrint(lua_State* L, const char* tag)
{
    Host* h = getHost(L);
    std::printf("[%s t=%.2f] ", tag, h->sched->now());
    int n = lua_gettop(L);
    for (int i = 1; i <= n; ++i)
    {
        if (i > 1)
            std::printf("\t");
        size_t len = 0;
        const char* s = luaL_tolstring(L, i, &len);
        std::printf("%s", s ? s : "(null)");
        lua_pop(L, 1);
    }
    std::printf("\n");
    return 0;
}

static int oboxPrintFn(lua_State* L)
{
    return oboxPrint(L, "OpenBlox");
}

static int oboxWarnFn(lua_State* L)
{
    return oboxPrint(L, "warn");
}

// ---------------------------------------------------------------------------
// require(ModuleScript)
// ---------------------------------------------------------------------------
static int oboxRequire(lua_State* L)
{
    Host* h = getHost(L);
    if (!isInstanceUd(L, 1))
        luaL_errorL(L, "require() expects a ModuleScript");
    Instance* inst = checkInstance(L, 1)->ptr;
    if (!inst->isA("ModuleScript"))
        luaL_errorL(L, "require() expects a ModuleScript");

    lua_getfield(L, LUA_REGISTRYINDEX, "OpenBlox.ModuleCache");
    lua_pushlightuserdata(L, inst);
    lua_rawget(L, -2);
    if (lua_toboolean(L, -1) && !lua_isboolean(L, -1))
    {
        lua_remove(L, -2); // drop cache table, keep value
        return 1;
    }
    if (lua_isboolean(L, -1) && lua_toboolean(L, -1))
    {
        lua_pop(L, 2);
        luaL_errorL(L, "require() cycle detected for '%s'", inst->name().c_str());
    }
    lua_pop(L, 1); // pop nil

    // Sentinel for cycle detection.
    lua_pushlightuserdata(L, inst);
    lua_pushboolean(L, 1);
    lua_rawset(L, -3);
    lua_pop(L, 1); // pop cache table

    auto* mod = static_cast<ModuleScript*>(inst);
    const std::string& src = mod->source();

    size_t bcSize = 0;
    char* bc = luau_compile(src.c_str(), src.size(), nullptr, &bcSize);
    if (!bc)
        luaL_errorL(L, "require: out of memory compiling '%s'", inst->name().c_str());

    lua_State* co = lua_newthread(L);
    lua_pushthread(co);
    int threadRef = lua_ref(co, -1);
    lua_pop(co, 1);
    lua_pop(L, 1); // pop thread object
    luaL_sandboxthread(co); // per-module isolated environment, like oboxRunFile
    h->names.push_back(inst->getFullName());
    lua_setthreaddata(co, (void*)h->names.back().c_str());

    std::string chunk = "@" + inst->getFullName();
    int ld = luau_load(co, chunk.c_str(), bc, bcSize, 0);
    free(bc);
    if (ld != LUA_OK)
    {
        lua_xmove(co, L, 1);
        lua_unref(L, threadRef);
        lua_error(L);
    }

    extern double g_oboxDeadline;
    extern double oboxWallNow();
    g_oboxDeadline = oboxWallNow() + 2.0;
    int status = lua_resume(co, h->sched->mainThread(), 0);
    g_oboxDeadline = 1e30;

    if (status == LUA_YIELD)
    {
        lua_unref(L, threadRef);
        // Clear the cycle sentinel so a later require can retry (same as the
        // error path below).
        lua_getfield(L, LUA_REGISTRYINDEX, "OpenBlox.ModuleCache");
        lua_pushlightuserdata(L, inst);
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        luaL_errorL(L, "require: ModuleScript '%s' must not yield", inst->name().c_str());
    }
    if (status != LUA_OK)
    {
        lua_xmove(co, L, 1);
        lua_unref(L, threadRef);
        // Clear the cycle sentinel so a later require can retry.
        lua_getfield(L, LUA_REGISTRYINDEX, "OpenBlox.ModuleCache");
        lua_pushlightuserdata(L, inst);
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        lua_error(L);
    }
    if (lua_gettop(co) == 0)
        lua_pushnil(co);
    lua_xmove(co, L, 1);
    lua_unref(L, threadRef);

    // Cache the result.
    lua_getfield(L, LUA_REGISTRYINDEX, "OpenBlox.ModuleCache");
    lua_pushlightuserdata(L, inst);
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    return 1;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
void oboxRegisterApi(lua_State* L, Host* host)
{
    lua_pushlightuserdata(L, host);
    lua_setfield(L, LUA_REGISTRYINDEX, "OpenBlox.Host");

    lua_newtable(L); // module cache
    lua_setfield(L, LUA_REGISTRYINDEX, "OpenBlox.ModuleCache");

    // --- metatables ---
    {
        const luaL_Reg instanceMt[] = {
            {"__index", instanceIndex},
            {"__newindex", instanceNewIndex},
            {"__tostring", instanceToString},
            {"__eq", instanceEq},
            {nullptr, nullptr},
        };
        luaL_newmetatable(L, "OpenBlox.Instance");
        luaL_register(L, nullptr, instanceMt);
        lua_setreadonly(L, -1, true); // scripts must not hijack API dispatch
        lua_pop(L, 1);
    }
    {
        const luaL_Reg gameMt[] = {
            {"__index", gameIndex},
            {"__newindex", instanceNewIndex},
            {"__tostring", instanceToString},
            {"__eq", instanceEq},
            {nullptr, nullptr},
        };
        luaL_newmetatable(L, "OpenBlox.DataModel");
        luaL_register(L, nullptr, gameMt);
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }
    {
        const luaL_Reg signalMethods[] = {
            {"Connect", oboxSignalConnect},
            {"Once", oboxSignalOnce},
            {"Wait", oboxSignalWait},
            {nullptr, nullptr},
        };
        luaL_newmetatable(L, "OpenBlox.Signal");
        lua_newtable(L);
        luaL_register(L, nullptr, signalMethods);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, oboxSignalEq, "Signal.__eq");
        lua_setfield(L, -2, "__eq");
        lua_getfield(L, -1, "__index");
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }
    {
        // Connection: Disconnect() method + Connected property.
        luaL_newmetatable(L, "OpenBlox.Connection");
        lua_pushcfunction(L,
                          [](lua_State* L) -> int {
                              UConn* c = (UConn*)luaL_checkudata(L, 1, "OpenBlox.Connection");
                              const char* k = luaL_checkstring(L, 2);
                              if (strcmp(k, "Disconnect") == 0)
                              {
                                  lua_pushcfunction(L, oboxConnDisconnect, "Disconnect");
                                  return 1;
                              }
                              if (strcmp(k, "Connected") == 0)
                              {
                                  lua_pushboolean(L, c->sig->isConnected(c->id));
                                  return 1;
                              }
                              luaL_errorL(L, "%s is not a valid member of Connection", k);
                          },
                          "Connection.__index");
        lua_setfield(L, -2, "__index");
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }
    {
        const luaL_Reg vec3Mt[] = {
            {"__index", vec3Index},
            {"__add", vec3Add},
            {"__sub", vec3Sub},
            {"__mul", vec3Mul},
            {"__div", vec3Div},
            {"__unm", vec3Unm},
            {"__eq", vec3Eq},
            {"__tostring", vec3ToString},
            {nullptr, nullptr},
        };
        luaL_newmetatable(L, "OpenBlox.Vector3");
        luaL_register(L, nullptr, vec3Mt);
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }
    {
        const luaL_Reg color3Mt[] = {
            {"__index", color3Index},
            {"__eq", color3Eq},
            {"__tostring", color3ToString},
            {nullptr, nullptr},
        };
        luaL_newmetatable(L, "OpenBlox.Color3");
        luaL_register(L, nullptr, color3Mt);
        lua_setreadonly(L, -1, true);
        lua_pop(L, 1);
    }

    // --- game ---
    {
        UInstance* u = (UInstance*)lua_newuserdata(L, sizeof(UInstance));
        u->ptr = host->game;
        luaL_getmetatable(L, "OpenBlox.DataModel");
        lua_setmetatable(L, -2);
        lua_setglobal(L, "game");
    }

    // --- workspace ---
    pushInstance(L, host->game->getService("Workspace"));
    lua_setglobal(L, "workspace");

    // --- Instance ---
    {
        lua_newtable(L);
        lua_pushcfunction(L, oboxInstanceNew, "Instance.new");
        lua_setfield(L, -2, "new");
        lua_setglobal(L, "Instance");
    }

    // --- Vector3 ---
    {
        lua_newtable(L);
        lua_pushcfunction(L, vec3New, "Vector3.new");
        lua_setfield(L, -2, "new");
        pushVec3(L, 0, 0, 0);
        lua_setfield(L, -2, "zero");
        pushVec3(L, 1, 1, 1);
        lua_setfield(L, -2, "one");
        pushVec3(L, 0, 1, 0);
        lua_setfield(L, -2, "yAxis");
        lua_setglobal(L, "Vector3");
    }

    // --- Color3 ---
    {
        lua_newtable(L);
        lua_pushcfunction(L, color3New, "Color3.new");
        lua_setfield(L, -2, "new");
        lua_pushcfunction(L, color3FromRGB, "Color3.fromRGB");
        lua_setfield(L, -2, "fromRGB");
        lua_setglobal(L, "Color3");
    }

    // --- task ---
    {
        const luaL_Reg taskLib[] = {
            {"spawn", taskSpawnDefer},
            {"defer", taskSpawnDefer},
            {"delay", taskDelay},
            {"wait", taskWait},
            {"cancel", taskCancel},
            {nullptr, nullptr},
        };
        lua_newtable(L);
        luaL_register(L, nullptr, taskLib);
        lua_setglobal(L, "task");
    }

    // --- misc globals ---
    lua_pushcfunction(L, oboxTick, "tick");
    lua_setglobal(L, "tick");
    lua_pushcfunction(L, oboxTick, "elapsedTime");
    lua_setglobal(L, "elapsedTime");
    lua_pushcfunction(L, taskWait, "wait");
    lua_setglobal(L, "wait");
    lua_pushcfunction(L, oboxPrintFn, "print");
    lua_setglobal(L, "print");
    lua_pushcfunction(L, oboxWarnFn, "warn");
    lua_setglobal(L, "warn");
    lua_pushcfunction(L, oboxRequire, "require");
    lua_setglobal(L, "require");

    // Per-module globals (CFrame/UDim/UDim2/BrickColor/Enum, TweenInfo).
    oboxRegisterMath(L);
    oboxRegisterServices(L);
    oboxRegisterPhysics(L);

    // WaitForChild poll loop in Lua: C functions cannot yield in a loop and
    // resume correctly, but Lua functions can.
    const char* wfcSrc =
        "return function(self, name, timeout)\n"
        "    timeout = timeout or 5\n"
        "    local start = tick()\n"
        "    while true do\n"
        "        local found = self:FindFirstChild(name)\n"
        "        if found then return found end\n"
        "        if tick() - start >= timeout then\n"
        "            error(\"WaitForChild timed out waiting for '\" .. name .. \"'\")\n"
        "        end\n"
        "        task.wait(0.1)\n"
        "    end\n"
        "end\n";
    size_t wfcSize = 0;
    char* wfcBc = luau_compile(wfcSrc, std::strlen(wfcSrc), nullptr, &wfcSize);
    if (wfcBc)
    {
        if (luau_load(L, "@waitforchild", wfcBc, wfcSize, 0) == LUA_OK)
        {
            lua_call(L, 0, 1); // -> the function
            lua_setfield(L, LUA_REGISTRYINDEX, "OB.WaitForChild");
        }
        free(wfcBc);
    }
}

// ---------------------------------------------------------------------------
// Script loading + frame loop
// ---------------------------------------------------------------------------
static std::string readFile(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool oboxRunFile(Host* host, const std::string& path)
{
    lua_State* L = host->L;
    std::string src = readFile(path);
    if (src.empty())
    {
        std::printf("[OpenBlox] cannot read script file: %s\n", path.c_str());
        return false;
    }

    lua_CompileOptions opts = {};
    size_t bcSize = 0;
    char* bc = luau_compile(src.c_str(), src.size(), &opts, &bcSize);
    if (!bc)
    {
        std::printf("[OpenBlox] out of memory compiling %s\n", path.c_str());
        return false;
    }

    Instance* si = host->game->createInstance("Script");
    si->setName(path.substr(path.find_last_of("/\\") + 1));
    static_cast<LuaSourceContainer*>(si)->setSource(src);

    lua_State* co = lua_newthread(L);
    lua_pop(L, 1); // pop thread object; scheduler anchors it via deferThread
    luaL_sandboxthread(co); // per-script isolated environment, reads proxy to globals
    host->names.push_back(path);
    lua_setthreaddata(co, (void*)host->names.back().c_str());

    std::string chunk = "@" + path;
    int ld = luau_load(co, chunk.c_str(), bc, bcSize, 0);
    free(bc);
    if (ld != LUA_OK)
    {
        const char* msg = lua_tostring(co, -1);
        std::printf("[OpenBlox] syntax error in '%s': %s\n", path.c_str(), msg ? msg : "?");
        lua_pop(co, 1);
        return false;
    }

    pushInstance(co, si);
    lua_setglobal(co, "script"); // lands in the thread's own env table

    host->sched->deferThread(co, 0, 0.0);
    return true;
}

// AABB touch detection between Parts parented under Workspace.
// Fires Touched on new overlaps and TouchEnded when they separate.
//
// Volume-overlap semantics are preserved exactly: faces that merely touch
// (e.g. one part's top at exactly y=4.0 against another's bottom) do NOT
// count as overlapping.

// NOTE: kept for compatibility; touchCheck now uses the cached
// pair test below (identical semantics, single getProp per part).
static bool aabbOverlap(Part* a, Part* b)
{
    PropValue pa, sa, pb, sb;
    if (!a->getProp("Position", pa) || !a->getProp("Size", sa))
        return false;
    if (!b->getProp("Position", pb) || !b->getProp("Size", sb))
        return false;
    const Vec3* p1 = std::get_if<Vec3>(&pa);
    const Vec3* s1 = std::get_if<Vec3>(&sa);
    const Vec3* p2 = std::get_if<Vec3>(&pb);
    const Vec3* s2 = std::get_if<Vec3>(&sb);
    if (!p1 || !s1 || !p2 || !s2)
        return false;
    return std::fabs(p1->x - p2->x) <= (s1->x + s2->x) / 2.0 &&
           std::fabs(p1->y - p2->y) <= (s1->y + s2->y) / 2.0 &&
           std::fabs(p1->z - p2->z) <= (s1->z + s2->z) / 2.0;
}

// Cached pair test: same volume-overlap semantics as aabbOverlap but works
// off the per-part cache collected once per frame (no string lookups).
static bool aabbOverlapCached(const PartCache& a, const PartCache& b)
{
    return std::fabs(a.pos.x - b.pos.x) <= (a.size.x + b.size.x) / 2.0 &&
           std::fabs(a.pos.y - b.pos.y) <= (a.size.y + b.size.y) / 2.0 &&
           std::fabs(a.pos.z - b.pos.z) <= (a.size.z + b.size.z) / 2.0;
}

static void collectPartCache(Instance* root, std::vector<PartCache>& out, Host* host)
{
    for (Instance* c : root->children())
    {
        if (!c->isDestroyed() && c->isA("Part"))
        {
            Part* p = static_cast<Part*>(c);
            PropValue pv, sv, av;
            Vec3 pos{0, 0, 0}, size{0, 0, 0};
            if (p->getProp("Position", pv))
                if (const Vec3* v = std::get_if<Vec3>(&pv))
                    pos = *v;
            if (p->getProp("Size", sv))
                if (const Vec3* v = std::get_if<Vec3>(&sv))
                    size = *v;
            if (p->getProp("Anchored", av))
                if (const bool* a = std::get_if<bool>(&av))
                    if (!*a)
                        host->sawUnanchored = true;
            out.push_back(PartCache{p, pos, size});
        }
        collectPartCache(c, out, host);
    }
}

static void touchCheck(Host* host)
{
    lua_State* L = host->L;
    Instance* ws = host->game->getService("Workspace");

    // Collect parts ONCE per frame: a single getProp per part, no per-pair
    // string lookups. Also flags any unanchored part for the early-exit check.
    host->sawUnanchored = false;
    std::vector<PartCache> parts;
    collectPartCache(ws, parts, host);

    // Dirty check: when the part set and every position/size are identical to
    // last frame, the pair set cannot have changed — skip pair processing.
    bool dirty = true;
    if (host->lastTouchValid && parts.size() == host->lastTouchParts.size())
    {
        dirty = false;
        for (size_t i = 0; i < parts.size(); ++i)
        {
            const PartCache& a = parts[i];
            const PartCache& b = host->lastTouchParts[i];
            if (a.part != b.part || a.pos.x != b.pos.x || a.pos.y != b.pos.y ||
                a.pos.z != b.pos.z || a.size.x != b.size.x || a.size.y != b.size.y ||
                a.size.z != b.size.z)
            {
                dirty = true;
                break;
            }
        }
    }
    host->lastTouchParts = parts;
    host->lastTouchValid = true;
    if (!dirty)
        return;

    using TouchKey = std::pair<Instance*, Instance*>;
    std::set<TouchKey> now;

    // Sweep-and-prune on the X axis: sort by minX, keep an active list of
    // parts whose X interval reaches the current part, and only run the full
    // AABB test on pairs with overlapping X intervals.
    size_t n = parts.size();
    std::vector<size_t> order(n);
    for (size_t i = 0; i < n; ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return (parts[a].pos.x - parts[a].size.x / 2.0) <
               (parts[b].pos.x - parts[b].size.x / 2.0);
    });
    std::vector<size_t> active;
    active.reserve(n);
    for (size_t oi = 0; oi < n; ++oi)
    {
        size_t i = order[oi];
        double minX = parts[i].pos.x - parts[i].size.x / 2.0;
        size_t kept = 0;
        for (size_t ai = 0; ai < active.size(); ++ai)
        {
            size_t j = active[ai];
            double jMaxX = parts[j].pos.x + parts[j].size.x / 2.0;
            if (jMaxX < minX)
                continue; // X intervals no longer overlap
            active[kept++] = j;
            const PartCache& a = parts[i];
            const PartCache& b = parts[j];
            if (!aabbOverlapCached(a, b))
                continue;
            Part* pa = a.part;
            Part* pb = b.part;
            TouchKey k = (pa < pb) ? TouchKey(pa, pb) : TouchKey(pb, pa);
            now.insert(k);
        }
        active.resize(kept);
        active.push_back(i);
    }

    std::set<TouchKey>& touching = host->touching;
    if (!host->touchPrimed)
    {
        // First call: record the currently-overlapping pairs WITHOUT firing
        // any events, so pre-existing overlaps at startup don't produce a
        // spurious Touched on frame 1.
        touching = now;
        host->touchPrimed = true;
        return;
    }

    for (const TouchKey& k : now)
    {
        if (touching.find(k) != touching.end())
            continue;
        Part* a = static_cast<Part*>(k.first);
        Part* b = static_cast<Part*>(k.second);
        pushInstance(L, b);
        a->touched.fire(*host->sched, L, 1);
        lua_pop(L, 1);
        pushInstance(L, a);
        b->touched.fire(*host->sched, L, 1);
        lua_pop(L, 1);
    }
    for (const TouchKey& k : touching)
    {
        if (now.find(k) != now.end())
            continue;
        Part* a = static_cast<Part*>(k.first);
        Part* b = static_cast<Part*>(k.second);
        pushInstance(L, b);
        a->touchEnded.fire(*host->sched, L, 1);
        lua_pop(L, 1);
        pushInstance(L, a);
        b->touchEnded.fire(*host->sched, L, 1);
        lua_pop(L, 1);
    }
    touching = now;
}

void oboxRunFrames(Host* host, int frames, double dt)
{
    lua_State* L = host->L;
    RunService* rs = static_cast<RunService*>(host->game->getService("RunService"));

    for (int i = 0; i < frames; ++i)
    {
        // Wall-clock run deadline: bounds the frame loop even when scripts
        // never yield (e.g. `while true do task.wait(0) end` with --seconds).
        if (oboxWallNow() > host->runDeadline)
        {
            std::printf("[OpenBlox] wall-clock run deadline exceeded, stopping\n");
            break;
        }
        lua_pushnumber(L, dt);
        rs->heartbeat.fire(*host->sched, L, 1);
        lua_pop(L, 1);
        host->sched->step(dt);
        oboxPhysicsStep(host, dt);
        oboxServiceStep(host, dt);
        touchCheck(host);
        // Early exit: nothing left to simulate or listen for.
        if (host->sched->idle() && rs->heartbeat.connectedCount() == 0 &&
            !host->sawUnanchored)
            break;
    }
    // Drain leftover scheduled work (bounded).
    for (int i = 0; i < 1200 && !host->sched->idle(); ++i)
    {
        if (oboxWallNow() > host->runDeadline)
        {
            std::printf("[OpenBlox] wall-clock run deadline exceeded, stopping\n");
            break;
        }
        host->sched->step(dt);
        touchCheck(host); // final positions can still create/end touches
    }
}
