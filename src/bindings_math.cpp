// OpenBlock Luau bindings for the math datatypes: CFrame, UDim, UDim2,
// BrickColor, and Enum. Clean-room code for the OpenBlock voxel game engine.
#include "mathx.h"

#include "bindings.h"

#include <lua.h>
#include <lualib.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <variant>

// ---------------------------------------------------------------------------
// Vector3 bridging through the shared property helpers (the Vector3
// userdata lives in bindings.cpp, so we read/write it via oboxToPropValue /
// oboxPushPropValue instead of duplicating the userdata code).
// ---------------------------------------------------------------------------
static Vec3 toVec3(lua_State* L, int idx)
{
    PropValue pv;
    std::string err;
    if (!oboxToPropValue(L, idx, "Vector3", pv, err))
        luaL_errorL(L, "Vector3 expected");
    return std::get<Vec3>(pv);
}

static void pushVec3v(lua_State* L, Vec3 v)
{
    PropValue pv = v;
    oboxPushPropValue(L, pv);
}

// ---------------------------------------------------------------------------
// CFrame userdata ("OpenBlox.CFrame")
// ---------------------------------------------------------------------------
UCFrame* oboxCheckCFrame(lua_State* L, int idx)
{
    return (UCFrame*)luaL_checkudata(L, idx, "OpenBlox.CFrame");
}

void oboxPushCFrame(lua_State* L, const CFrameV& c)
{
    UCFrame* u = (UCFrame*)lua_newuserdata(L, sizeof(UCFrame));
    u->c = c;
    luaL_getmetatable(L, "OpenBlox.CFrame");
    lua_setmetatable(L, -2);
}

bool oboxIsCFrame(lua_State* L, int idx)
{
    if (lua_type(L, idx) != LUA_TUSERDATA || !lua_getmetatable(L, idx))
        return false;
    luaL_getmetatable(L, "OpenBlox.CFrame");
    bool eq = lua_rawequal(L, -1, -2) != 0;
    lua_pop(L, 2);
    return eq;
}

static int cframeNew(lua_State* L)
{
    int n = lua_gettop(L);
    if (n == 0)
    {
        oboxPushCFrame(L, cfIdentity());
        return 1;
    }
    if (n == 3)
    {
        oboxPushCFrame(L,
                       cfFromPos(luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3)));
        return 1;
    }
    if (n == 1)
    {
        Vec3 p = toVec3(L, 1);
        oboxPushCFrame(L, cfFromPos(p.x, p.y, p.z));
        return 1;
    }
    if (n == 2)
    {
        Vec3 p = toVec3(L, 1);
        Vec3 t = toVec3(L, 2);
        oboxPushCFrame(L, cfLookAt(p.x, p.y, p.z, t.x, t.y, t.z));
        return 1;
    }
    luaL_errorL(L, "CFrame.new expects 0, 1, 2 or 3 arguments");
}

static int cframeAngles(lua_State* L)
{
    oboxPushCFrame(L, cfFromEuler(luaL_checknumber(L, 1), luaL_checknumber(L, 2),
                                 luaL_checknumber(L, 3)));
    return 1;
}

static int cframeFromEulerAnglesXYZ(lua_State* L)
{
    return cframeAngles(L);
}

static int cframeInverse(lua_State* L)
{
    oboxPushCFrame(L, cfInverse(oboxCheckCFrame(L, 1)->c));
    return 1;
}

static int cframeLerp(lua_State* L)
{
    UCFrame* a = oboxCheckCFrame(L, 1);
    UCFrame* b = oboxCheckCFrame(L, 2);
    double alpha = luaL_checknumber(L, 3);
    oboxPushCFrame(L, cfLerp(a->c, b->c, alpha));
    return 1;
}

static int cframeToWorldSpace(lua_State* L)
{
    UCFrame* a = oboxCheckCFrame(L, 1);
    UCFrame* b = oboxCheckCFrame(L, 2);
    oboxPushCFrame(L, cfMul(a->c, b->c));
    return 1;
}

static int cframeToObjectSpace(lua_State* L)
{
    UCFrame* a = oboxCheckCFrame(L, 1);
    UCFrame* b = oboxCheckCFrame(L, 2);
    oboxPushCFrame(L, cfMul(cfInverse(a->c), b->c));
    return 1;
}

static int cframeGetComponents(lua_State* L)
{
    UCFrame* c = oboxCheckCFrame(L, 1);
    for (int i = 0; i < 12; ++i)
        lua_pushnumber(L, c->c.m[i]);
    return 12;
}

static int cframePointToWorldSpace(lua_State* L)
{
    UCFrame* c = oboxCheckCFrame(L, 1);
    pushVec3v(L, cfPointToWorld(c->c, toVec3(L, 2)));
    return 1;
}

static int cframePointToObjectSpace(lua_State* L)
{
    UCFrame* c = oboxCheckCFrame(L, 1);
    pushVec3v(L, cfPointToObject(c->c, toVec3(L, 2)));
    return 1;
}

static int cframeVectorToWorldSpace(lua_State* L)
{
    UCFrame* c = oboxCheckCFrame(L, 1);
    pushVec3v(L, cfVectorToWorld(c->c, toVec3(L, 2)));
    return 1;
}

static int cframeVectorToObjectSpace(lua_State* L)
{
    UCFrame* c = oboxCheckCFrame(L, 1);
    pushVec3v(L, cfVectorToObject(c->c, toVec3(L, 2)));
    return 1;
}

static int cframeIndex(lua_State* L)
{
    UCFrame* c = oboxCheckCFrame(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (strcmp(k, "Position") == 0)
    {
        pushVec3v(L, cfPosition(c->c));
        return 1;
    }
    if (strcmp(k, "LookVector") == 0)
    {
        pushVec3v(L, cfLook(c->c));
        return 1;
    }
    if (strcmp(k, "RightVector") == 0)
    {
        pushVec3v(L, cfRight(c->c));
        return 1;
    }
    if (strcmp(k, "UpVector") == 0)
    {
        pushVec3v(L, cfUp(c->c));
        return 1;
    }
    if (strcmp(k, "X") == 0)
    {
        lua_pushnumber(L, c->c.m[9]);
        return 1;
    }
    if (strcmp(k, "Y") == 0)
    {
        lua_pushnumber(L, c->c.m[10]);
        return 1;
    }
    if (strcmp(k, "Z") == 0)
    {
        lua_pushnumber(L, c->c.m[11]);
        return 1;
    }
    if (strcmp(k, "Inverse") == 0)
    {
        lua_pushcfunction(L, cframeInverse, "Inverse");
        return 1;
    }
    if (strcmp(k, "Lerp") == 0)
    {
        lua_pushcfunction(L, cframeLerp, "Lerp");
        return 1;
    }
    if (strcmp(k, "ToWorldSpace") == 0)
    {
        lua_pushcfunction(L, cframeToWorldSpace, "ToWorldSpace");
        return 1;
    }
    if (strcmp(k, "ToObjectSpace") == 0)
    {
        lua_pushcfunction(L, cframeToObjectSpace, "ToObjectSpace");
        return 1;
    }
    if (strcmp(k, "GetComponents") == 0)
    {
        lua_pushcfunction(L, cframeGetComponents, "GetComponents");
        return 1;
    }
    if (strcmp(k, "PointToWorldSpace") == 0)
    {
        lua_pushcfunction(L, cframePointToWorldSpace, "PointToWorldSpace");
        return 1;
    }
    if (strcmp(k, "PointToObjectSpace") == 0)
    {
        lua_pushcfunction(L, cframePointToObjectSpace, "PointToObjectSpace");
        return 1;
    }
    if (strcmp(k, "VectorToWorldSpace") == 0)
    {
        lua_pushcfunction(L, cframeVectorToWorldSpace, "VectorToWorldSpace");
        return 1;
    }
    if (strcmp(k, "VectorToObjectSpace") == 0)
    {
        lua_pushcfunction(L, cframeVectorToObjectSpace, "VectorToObjectSpace");
        return 1;
    }
    luaL_errorL(L, "%s is not a valid member of CFrame", k);
}

static int cframeMul(lua_State* L)
{
    if (oboxIsCFrame(L, 1) && oboxIsCFrame(L, 2))
    {
        oboxPushCFrame(L, cfMul(oboxCheckCFrame(L, 1)->c, oboxCheckCFrame(L, 2)->c));
        return 1;
    }
    if (oboxIsCFrame(L, 1))
    {
        UCFrame* c = oboxCheckCFrame(L, 1);
        pushVec3v(L, cfPointToWorld(c->c, toVec3(L, 2)));
        return 1;
    }
    luaL_errorL(L, "attempt to multiply a CFrame with an incompatible value");
}

static int cframeAdd(lua_State* L)
{
    UCFrame* c = oboxCheckCFrame(L, 1);
    Vec3 v = toVec3(L, 2);
    CFrameV r = c->c;
    r.m[9] += v.x;
    r.m[10] += v.y;
    r.m[11] += v.z;
    oboxPushCFrame(L, r);
    return 1;
}

static int cframeSub(lua_State* L)
{
    if (oboxIsCFrame(L, 2))
    {
        Vec3 a = cfPosition(oboxCheckCFrame(L, 1)->c);
        Vec3 b = cfPosition(oboxCheckCFrame(L, 2)->c);
        pushVec3v(L, Vec3{a.x - b.x, a.y - b.y, a.z - b.z});
        return 1;
    }
    UCFrame* c = oboxCheckCFrame(L, 1);
    Vec3 v = toVec3(L, 2);
    CFrameV r = c->c;
    r.m[9] -= v.x;
    r.m[10] -= v.y;
    r.m[11] -= v.z;
    oboxPushCFrame(L, r);
    return 1;
}

static int cframeEq(lua_State* L)
{
    UCFrame* a = oboxCheckCFrame(L, 1);
    UCFrame* b = oboxCheckCFrame(L, 2);
    bool eq = true;
    for (int i = 0; i < 12; ++i)
        if (a->c.m[i] != b->c.m[i])
        {
            eq = false;
            break;
        }
    lua_pushboolean(L, eq);
    return 1;
}

static int cframeToString(lua_State* L)
{
    UCFrame* c = oboxCheckCFrame(L, 1);
    char buf[256];
    snprintf(buf, sizeof(buf), "%g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g", c->c.m[0],
             c->c.m[1], c->c.m[2], c->c.m[3], c->c.m[4], c->c.m[5], c->c.m[6], c->c.m[7],
             c->c.m[8], c->c.m[9], c->c.m[10], c->c.m[11]);
    lua_pushstring(L, buf);
    return 1;
}

// ---------------------------------------------------------------------------
// UDim userdata ("OpenBlox.UDim")
// ---------------------------------------------------------------------------
struct UUDim
{
    double scale = 0.0;
    double offset = 0.0;
};

static UUDim* checkUDim(lua_State* L, int idx)
{
    return (UUDim*)luaL_checkudata(L, idx, "OpenBlox.UDim");
}

static void pushUDim(lua_State* L, double scale, double offset)
{
    UUDim* u = (UUDim*)lua_newuserdata(L, sizeof(UUDim));
    u->scale = scale;
    u->offset = offset;
    luaL_getmetatable(L, "OpenBlox.UDim");
    lua_setmetatable(L, -2);
}

static int udimNew(lua_State* L)
{
    pushUDim(L, luaL_checknumber(L, 1), luaL_checknumber(L, 2));
    return 1;
}

static int udimIndex(lua_State* L)
{
    UUDim* u = checkUDim(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (strcmp(k, "Scale") == 0)
    {
        lua_pushnumber(L, u->scale);
        return 1;
    }
    if (strcmp(k, "Offset") == 0)
    {
        lua_pushnumber(L, u->offset);
        return 1;
    }
    luaL_errorL(L, "%s is not a valid member of UDim", k);
}

static int udimAdd(lua_State* L)
{
    UUDim* a = checkUDim(L, 1);
    UUDim* b = checkUDim(L, 2);
    pushUDim(L, a->scale + b->scale, a->offset + b->offset);
    return 1;
}

static int udimSub(lua_State* L)
{
    UUDim* a = checkUDim(L, 1);
    UUDim* b = checkUDim(L, 2);
    pushUDim(L, a->scale - b->scale, a->offset - b->offset);
    return 1;
}

static int udimEq(lua_State* L)
{
    UUDim* a = checkUDim(L, 1);
    UUDim* b = checkUDim(L, 2);
    lua_pushboolean(L, a->scale == b->scale && a->offset == b->offset);
    return 1;
}

static int udimToString(lua_State* L)
{
    UUDim* u = checkUDim(L, 1);
    char buf[96];
    snprintf(buf, sizeof(buf), "%g, %g", u->scale, u->offset);
    lua_pushstring(L, buf);
    return 1;
}

// ---------------------------------------------------------------------------
// UDim2 userdata ("OpenBlox.UDim2")
// ---------------------------------------------------------------------------
UUDim2* oboxCheckUDim2(lua_State* L, int idx)
{
    return (UUDim2*)luaL_checkudata(L, idx, "OpenBlox.UDim2");
}

void oboxPushUDim2(lua_State* L, const UDim2V& v)
{
    UUDim2* u = (UUDim2*)lua_newuserdata(L, sizeof(UUDim2));
    u->v = v;
    luaL_getmetatable(L, "OpenBlox.UDim2");
    lua_setmetatable(L, -2);
}

bool oboxIsUDim2(lua_State* L, int idx)
{
    if (lua_type(L, idx) != LUA_TUSERDATA || !lua_getmetatable(L, idx))
        return false;
    luaL_getmetatable(L, "OpenBlox.UDim2");
    bool eq = lua_rawequal(L, -1, -2) != 0;
    lua_pop(L, 2);
    return eq;
}

static int udim2New(lua_State* L)
{
    UDim2V v;
    v.xScale = luaL_checknumber(L, 1);
    v.xOffset = luaL_checknumber(L, 2);
    v.yScale = luaL_checknumber(L, 3);
    v.yOffset = luaL_checknumber(L, 4);
    oboxPushUDim2(L, v);
    return 1;
}

static int udim2FromScale(lua_State* L)
{
    UDim2V v;
    v.xScale = luaL_checknumber(L, 1);
    v.yScale = luaL_checknumber(L, 2);
    oboxPushUDim2(L, v);
    return 1;
}

static int udim2FromOffset(lua_State* L)
{
    UDim2V v;
    v.xOffset = luaL_checknumber(L, 1);
    v.yOffset = luaL_checknumber(L, 2);
    oboxPushUDim2(L, v);
    return 1;
}

static int udim2Index(lua_State* L)
{
    UUDim2* u = oboxCheckUDim2(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (strcmp(k, "XScale") == 0)
    {
        lua_pushnumber(L, u->v.xScale);
        return 1;
    }
    if (strcmp(k, "XOffset") == 0)
    {
        lua_pushnumber(L, u->v.xOffset);
        return 1;
    }
    if (strcmp(k, "YScale") == 0)
    {
        lua_pushnumber(L, u->v.yScale);
        return 1;
    }
    if (strcmp(k, "YOffset") == 0)
    {
        lua_pushnumber(L, u->v.yOffset);
        return 1;
    }
    if (strcmp(k, "Width") == 0)
    {
        lua_pushnumber(L, u->v.xOffset);
        return 1;
    }
    if (strcmp(k, "Height") == 0)
    {
        lua_pushnumber(L, u->v.yOffset);
        return 1;
    }
    luaL_errorL(L, "%s is not a valid member of UDim2", k);
}

static int udim2Add(lua_State* L)
{
    UUDim2* a = oboxCheckUDim2(L, 1);
    UUDim2* b = oboxCheckUDim2(L, 2);
    UDim2V r{a->v.xScale + b->v.xScale, a->v.xOffset + b->v.xOffset, a->v.yScale + b->v.yScale,
              a->v.yOffset + b->v.yOffset};
    oboxPushUDim2(L, r);
    return 1;
}

static int udim2Sub(lua_State* L)
{
    UUDim2* a = oboxCheckUDim2(L, 1);
    UUDim2* b = oboxCheckUDim2(L, 2);
    UDim2V r{a->v.xScale - b->v.xScale, a->v.xOffset - b->v.xOffset, a->v.yScale - b->v.yScale,
              a->v.yOffset - b->v.yOffset};
    oboxPushUDim2(L, r);
    return 1;
}

static int udim2Eq(lua_State* L)
{
    UUDim2* a = oboxCheckUDim2(L, 1);
    UUDim2* b = oboxCheckUDim2(L, 2);
    lua_pushboolean(L, a->v.xScale == b->v.xScale && a->v.xOffset == b->v.xOffset &&
                                a->v.yScale == b->v.yScale && a->v.yOffset == b->v.yOffset);
    return 1;
}

static int udim2ToString(lua_State* L)
{
    UUDim2* u = oboxCheckUDim2(L, 1);
    char buf[128];
    snprintf(buf, sizeof(buf), "%g, %g, %g, %g", u->v.xScale, u->v.xOffset, u->v.yScale,
             u->v.yOffset);
    lua_pushstring(L, buf);
    return 1;
}

// ---------------------------------------------------------------------------
// BrickColor userdata ("OpenBlox.BrickColor")
// ---------------------------------------------------------------------------
UBrickColor* oboxCheckBrickColor(lua_State* L, int idx)
{
    return (UBrickColor*)luaL_checkudata(L, idx, "OpenBlox.BrickColor");
}

void oboxPushBrickColor(lua_State* L, const BrickColorV& v)
{
    UBrickColor* u = (UBrickColor*)lua_newuserdata(L, sizeof(UBrickColor));
    u->paletteIndex = brickColorPaletteIndex(&v);
    luaL_getmetatable(L, "OpenBlox.BrickColor");
    lua_setmetatable(L, -2);
}

// Looks up the BrickColorV for a userdata's stored palette index.
static const BrickColorV& brickColorOf(const UBrickColor* u)
{
    size_t count = 0;
    const BrickColorV* pal = brickColorPalette(count);
    int i = u->paletteIndex;
    if (i < 0 || (size_t)i >= count)
        i = 0;
    return pal[i];
}

bool oboxIsBrickColor(lua_State* L, int idx)
{
    if (lua_type(L, idx) != LUA_TUSERDATA || !lua_getmetatable(L, idx))
        return false;
    luaL_getmetatable(L, "OpenBlox.BrickColor");
    bool eq = lua_rawequal(L, -1, -2) != 0;
    lua_pop(L, 2);
    return eq;
}

static int brickColorNew(lua_State* L)
{
    const BrickColorV* found = nullptr;
    int t = lua_type(L, 1);
    if (t == LUA_TSTRING)
    {
        found = brickColorByName(lua_tostring(L, 1));
        if (!found)
            luaL_errorL(L, "\"%s\" is not a valid BrickColor name", lua_tostring(L, 1));
    }
    else if (t == LUA_TNUMBER)
    {
        found = brickColorByNumber((int)lua_tonumber(L, 1));
        if (!found)
            luaL_errorL(L, "%d is not a valid BrickColor number", (int)lua_tonumber(L, 1));
    }
    else
    {
        // A Color3 snaps to the nearest palette color.
        PropValue pv;
        std::string err;
        if (!oboxToPropValue(L, 1, "Color3", pv, err))
            luaL_errorL(L, "BrickColor.new expects a name, number or Color3");
        Color3 c = std::get<Color3>(pv);
        size_t count = 0;
        const BrickColorV* pal = brickColorPalette(count);
        double best = 1e300;
        for (size_t i = 0; i < count; ++i)
        {
            double dr = pal[i].color.r - c.r;
            double dg = pal[i].color.g - c.g;
            double db = pal[i].color.b - c.b;
            double d = dr * dr + dg * dg + db * db;
            if (d < best)
            {
                best = d;
                found = &pal[i];
            }
        }
    }
    oboxPushBrickColor(L, *found);
    return 1;
}

static int brickColorIndex(lua_State* L)
{
    UBrickColor* u = oboxCheckBrickColor(L, 1);
    const BrickColorV& v = brickColorOf(u);
    const char* k = luaL_checkstring(L, 2);
    if (strcmp(k, "Name") == 0)
    {
        lua_pushstring(L, v.name.c_str());
        return 1;
    }
    if (strcmp(k, "Number") == 0)
    {
        lua_pushnumber(L, v.number);
        return 1;
    }
    if (strcmp(k, "Color") == 0)
    {
        PropValue pv = v.color;
        oboxPushPropValue(L, pv);
        return 1;
    }
    luaL_errorL(L, "%s is not a valid member of BrickColor", k);
}

static int brickColorEq(lua_State* L)
{
    UBrickColor* a = oboxCheckBrickColor(L, 1);
    UBrickColor* b = oboxCheckBrickColor(L, 2);
    lua_pushboolean(L, brickColorOf(a).number == brickColorOf(b).number);
    return 1;
}

static int brickColorToString(lua_State* L)
{
    UBrickColor* u = oboxCheckBrickColor(L, 1);
    lua_pushstring(L, brickColorOf(u).name.c_str());
    return 1;
}

// ---------------------------------------------------------------------------
// Enum + EnumItem ("OpenBlox.EnumItem")
// ---------------------------------------------------------------------------
struct UEnumItem
{
    char enumName[64];
    char itemName[64];
    int value = 0;
};

static UEnumItem* checkEnumItem(lua_State* L, int idx)
{
    return (UEnumItem*)luaL_checkudata(L, idx, "OpenBlox.EnumItem");
}

static int enumItemIndex(lua_State* L)
{
    UEnumItem* u = checkEnumItem(L, 1);
    const char* k = luaL_checkstring(L, 2);
    if (strcmp(k, "Name") == 0)
    {
        lua_pushstring(L, u->itemName);
        return 1;
    }
    if (strcmp(k, "Value") == 0)
    {
        lua_pushnumber(L, u->value);
        return 1;
    }
    luaL_errorL(L, "%s is not a valid member of EnumItem", k);
}

static int enumItemEq(lua_State* L)
{
    UEnumItem* a = checkEnumItem(L, 1);
    UEnumItem* b = checkEnumItem(L, 2);
    lua_pushboolean(L, strcmp(a->enumName, b->enumName) == 0 && a->value == b->value);
    return 1;
}

static int enumItemToString(lua_State* L)
{
    UEnumItem* u = checkEnumItem(L, 1);
    char buf[160];
    snprintf(buf, sizeof(buf), "Enum.%s.%s", u->enumName, u->itemName);
    lua_pushstring(L, buf);
    return 1;
}

// __index for an Enum table. Upvalue 1 is the name->EnumItem table, upvalue 2
// is the enum name for error messages.
static int enumIndex(lua_State* L)
{
    const char* k = luaL_checkstring(L, 2);
    lua_pushvalue(L, 2);
    lua_gettable(L, lua_upvalueindex(1));
    if (lua_isnil(L, -1))
        luaL_errorL(L, "%s is not a valid member of Enum.%s", k,
                    lua_tostring(L, lua_upvalueindex(2)));
    return 1;
}

static void pushEnumTable(lua_State* L, const char* enumName, const char* const* names, int count)
{
    lua_newtable(L); // enum table
    lua_newtable(L); // metatable
    lua_newtable(L); // items: name -> EnumItem userdata
    for (int i = 0; i < count; ++i)
    {
        UEnumItem* u = (UEnumItem*)lua_newuserdata(L, sizeof(UEnumItem));
        snprintf(u->enumName, sizeof(u->enumName), "%s", enumName);
        snprintf(u->itemName, sizeof(u->itemName), "%s", names[i]);
        u->value = i;
        luaL_getmetatable(L, "OpenBlox.EnumItem");
        lua_setmetatable(L, -2);
        lua_setfield(L, -2, names[i]);
    }
    lua_pushstring(L, enumName);
    lua_pushcclosure(L, enumIndex, "Enum.__index", 2);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
}

static const char* const kMaterialNames[] = {
    "Plastic",  "SmoothPlastic", "Neon",   "Wood",     "Marble", "Metal", "DiamondPlate", "Foil",
    "Grass",    "Slate",         "Concrete", "Granite", "Sand",   "Fabric", "Glass",        "Ice",
};
// Order matches easeValue() style ids.
static const char* const kEasingStyleNames[] = {
    "Linear", "Sine",  "Quad",   "Cubic",  "Quart",  "Quint",
    "Exponential", "Circular", "Back", "Bounce", "Elastic",
};
// Order matches easeValue() direction ids.
static const char* const kEasingDirectionNames[] = {"In", "Out", "InOut"};
static const char* const kHumanoidStateTypeNames[] = {
    "Running", "Jumping", "Falling", "Seated", "Dead", "Climbing", "Swimming",
};
static const char* const kSortOrderNames[] = {"Name", "Custom"};
static const char* const kFontNames[] = {
    "Legacy", "Arial", "Gotham", "Bodoni", "Cartoon", "Code", "Highway", "Fantasy",
};
static const char* const kPlaybackStateNames[] = {
    "Playing", "Paused", "Stopped", "Completed", "Cancelled",
};

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
void oboxRegisterMath(lua_State* L)
{
    // --- metatables ---
    {
        const luaL_Reg cframeMt[] = {
            {"__index", cframeIndex},
            {"__mul", cframeMul},
            {"__add", cframeAdd},
            {"__sub", cframeSub},
            {"__eq", cframeEq},
            {"__tostring", cframeToString},
            {nullptr, nullptr},
        };
        luaL_newmetatable(L, "OpenBlox.CFrame");
        luaL_register(L, nullptr, cframeMt);
        lua_setreadonly(L, -1, true); // freeze: scripts must not hijack dispatch
        lua_pop(L, 1);
    }
    {
        const luaL_Reg udimMt[] = {
            {"__index", udimIndex},
            {"__add", udimAdd},
            {"__sub", udimSub},
            {"__eq", udimEq},
            {"__tostring", udimToString},
            {nullptr, nullptr},
        };
        luaL_newmetatable(L, "OpenBlox.UDim");
        luaL_register(L, nullptr, udimMt);
        lua_setreadonly(L, -1, true); // freeze: scripts must not hijack dispatch
        lua_pop(L, 1);
    }
    {
        const luaL_Reg udim2Mt[] = {
            {"__index", udim2Index},
            {"__add", udim2Add},
            {"__sub", udim2Sub},
            {"__eq", udim2Eq},
            {"__tostring", udim2ToString},
            {nullptr, nullptr},
        };
        luaL_newmetatable(L, "OpenBlox.UDim2");
        luaL_register(L, nullptr, udim2Mt);
        lua_setreadonly(L, -1, true); // freeze: scripts must not hijack dispatch
        lua_pop(L, 1);
    }
    {
        const luaL_Reg brickColorMt[] = {
            {"__index", brickColorIndex},
            {"__eq", brickColorEq},
            {"__tostring", brickColorToString},
            {nullptr, nullptr},
        };
        luaL_newmetatable(L, "OpenBlox.BrickColor");
        luaL_register(L, nullptr, brickColorMt);
        lua_setreadonly(L, -1, true); // freeze: scripts must not hijack dispatch
        lua_pop(L, 1);
    }
    {
        const luaL_Reg enumItemMt[] = {
            {"__index", enumItemIndex},
            {"__eq", enumItemEq},
            {"__tostring", enumItemToString},
            {nullptr, nullptr},
        };
        luaL_newmetatable(L, "OpenBlox.EnumItem");
        luaL_register(L, nullptr, enumItemMt);
        lua_setreadonly(L, -1, true); // freeze: scripts must not hijack dispatch
        lua_pop(L, 1);
    }
    // --- globals ---
    {
        const luaL_Reg cframeLib[] = {
            {"new", cframeNew},
            {"Angles", cframeAngles},
            {"fromEulerAnglesXYZ", cframeFromEulerAnglesXYZ},
            {nullptr, nullptr},
        };
        lua_newtable(L);
        luaL_register(L, nullptr, cframeLib);
        lua_setglobal(L, "CFrame");
    }
    {
        const luaL_Reg udimLib[] = {
            {"new", udimNew},
            {nullptr, nullptr},
        };
        lua_newtable(L);
        luaL_register(L, nullptr, udimLib);
        lua_setglobal(L, "UDim");
    }
    {
        const luaL_Reg udim2Lib[] = {
            {"new", udim2New},
            {"fromScale", udim2FromScale},
            {"fromOffset", udim2FromOffset},
            {nullptr, nullptr},
        };
        lua_newtable(L);
        luaL_register(L, nullptr, udim2Lib);
        lua_setglobal(L, "UDim2");
    }
    {
        const luaL_Reg brickColorLib[] = {
            {"new", brickColorNew},
            {nullptr, nullptr},
        };
        lua_newtable(L);
        luaL_register(L, nullptr, brickColorLib);
        lua_setglobal(L, "BrickColor");
    }
    {
        lua_newtable(L); // Enum
        pushEnumTable(L, "Material", kMaterialNames, 16);
        lua_setfield(L, -2, "Material");
        pushEnumTable(L, "EasingStyle", kEasingStyleNames, 11);
        lua_setfield(L, -2, "EasingStyle");
        pushEnumTable(L, "EasingDirection", kEasingDirectionNames, 3);
        lua_setfield(L, -2, "EasingDirection");
        pushEnumTable(L, "HumanoidStateType", kHumanoidStateTypeNames, 7);
        lua_setfield(L, -2, "HumanoidStateType");
        pushEnumTable(L, "SortOrder", kSortOrderNames, 2);
        lua_setfield(L, -2, "SortOrder");
        pushEnumTable(L, "Font", kFontNames, 8);
        lua_setfield(L, -2, "Font");
        pushEnumTable(L, "PlaybackState", kPlaybackStateNames, 5);
        lua_setfield(L, -2, "PlaybackState");
        lua_setglobal(L, "Enum");
    }
}
