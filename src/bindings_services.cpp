// OpenBlock services Luau bindings: method dispatch for the services module
// (Players, Teams, Sound, TweenService, Tween, DataStoreService, DataStore,
// Debris, BindableEvent, BindableFunction) plus the TweenInfo userdata.
// Clean-room implementation against the public Luau C API.
#include "services.h"

#include "bindings.h" // Host, oboxGetHost, oboxCheckInstance, oboxIsInstance,
                      // oboxPushInstance, oboxToPropValue
#include "mathx.h"    // oboxCheckCFrame, oboxCheckUDim2, oboxCheckBrickColor

#include <lua.h>
#include <lualib.h>

#include <cstring>
#include <new>
#include <string>

// ---------------------------------------------------------------------------
// Players
// ---------------------------------------------------------------------------
static int playersGetPlayers(lua_State* L)
{
    Instance* inst = oboxCheckInstance(L, 1);
    lua_newtable(L);
    int i = 1;
    for (Instance* c : inst->children())
    {
        if (c->className() == "Player")
        {
            oboxPushInstance(L, c);
            lua_rawseti(L, -2, i++);
        }
    }
    return 1;
}

static int playersGetPlayerByUserId(lua_State* L)
{
    Instance* inst = oboxCheckInstance(L, 1);
    double id = luaL_checknumber(L, 2);
    for (Instance* c : inst->children())
    {
        if (c->className() != "Player")
            continue;
        PropValue v;
        if (c->getProp("UserId", v) && std::holds_alternative<double>(v) &&
            std::get<double>(v) == id)
        {
            oboxPushInstance(L, c);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

// ---------------------------------------------------------------------------
// Teams
// ---------------------------------------------------------------------------
static int teamsGetTeams(lua_State* L)
{
    Instance* inst = oboxCheckInstance(L, 1);
    lua_newtable(L);
    int i = 1;
    for (Instance* c : inst->children())
    {
        if (c->className() == "Team")
        {
            oboxPushInstance(L, c);
            lua_rawseti(L, -2, i++);
        }
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Sound
// ---------------------------------------------------------------------------
static void setSoundPlaying(lua_State* L, Sound* s, bool playing, bool resetPosition)
{
    Host* h = oboxGetHost(L);
    s->setProp("Playing", playing);
    oboxFirePropChanged(h, s, "Playing");
    if (resetPosition)
    {
        s->setProp("TimePosition", 0.0);
        oboxFirePropChanged(h, s, "TimePosition");
    }
}

static int soundPlay(lua_State* L)
{
    setSoundPlaying(L, static_cast<Sound*>(oboxCheckInstance(L, 1)), true, false);
    return 0;
}

static int soundStop(lua_State* L)
{
    setSoundPlaying(L, static_cast<Sound*>(oboxCheckInstance(L, 1)), false, true);
    return 0;
}

static int soundPause(lua_State* L)
{
    setSoundPlaying(L, static_cast<Sound*>(oboxCheckInstance(L, 1)), false, false);
    return 0;
}

static int soundResume(lua_State* L)
{
    setSoundPlaying(L, static_cast<Sound*>(oboxCheckInstance(L, 1)), true, false);
    return 0;
}

// ---------------------------------------------------------------------------
// TweenInfo userdata
// ---------------------------------------------------------------------------
UTweenInfo* oboxCheckTweenInfo(lua_State* L, int idx)
{
    return (UTweenInfo*)luaL_checkudata(L, idx, "OpenBlox.TweenInfo");
}

// Accepts a plain number id, or (once an Enum system exists) an EnumItem
// userdata whose metatable is "OpenBlox.EnumItem" with a numeric Value field.
static int checkEasingId(lua_State* L, int idx, int def, int lo, int hi, const char* what)
{
    int t = lua_type(L, idx);
    if (t == LUA_TNONE || t == LUA_TNIL)
        return def;
    int v = def;
    if (t == LUA_TNUMBER)
    {
        v = (int)lua_tonumber(L, idx);
    }
    else if (t == LUA_TUSERDATA)
    {
        if (!lua_getmetatable(L, idx))
            luaL_errorL(L, "%s: expected a number or EnumItem", what);
        luaL_getmetatable(L, "OpenBlox.EnumItem");
        bool isEnumItem = lua_rawequal(L, -1, -2) != 0;
        lua_pop(L, 2);
        if (!isEnumItem)
            luaL_errorL(L, "%s: expected a number or EnumItem", what);
        lua_getfield(L, idx, "Value");
        v = (int)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    else
    {
        luaL_errorL(L, "%s: expected a number or EnumItem", what);
    }
    if (v < lo || v > hi)
        luaL_errorL(L, "%s: id %d out of range [%d, %d]", what, v, lo, hi);
    return v;
}

static int tweenInfoNew(lua_State* L)
{
    // Validate all arguments BEFORE pushing the result userdata: the
    // userdata would otherwise shift the optional-argument indices.
    double time = luaL_checknumber(L, 1);
    if (time < 0.0)
        luaL_errorL(L, "TweenInfo.new: time must be >= 0");
    int style = checkEasingId(L, 2, 1, 0, 10, "easingStyle");
    int direction = checkEasingId(L, 3, 1, 0, 2, "easingDirection");
    int repeatCount = 0;
    if (lua_type(L, 4) != LUA_TNONE && lua_type(L, 4) != LUA_TNIL)
    {
        int rc = (int)luaL_checknumber(L, 4);
        if (rc < -1)
            luaL_errorL(L, "TweenInfo.new: repeatCount must be >= -1");
        repeatCount = rc;
    }
    bool reverses = lua_toboolean(L, 5) != 0;
    double delayTime = 0.0;
    if (lua_type(L, 6) != LUA_TNONE && lua_type(L, 6) != LUA_TNIL)
    {
        double d = luaL_checknumber(L, 6);
        if (d < 0.0)
            luaL_errorL(L, "TweenInfo.new: delayTime must be >= 0");
        delayTime = d;
    }

    UTweenInfo* u = (UTweenInfo*)lua_newuserdata(L, sizeof(UTweenInfo));
    new (u) UTweenInfo();
    luaL_getmetatable(L, "OpenBlox.TweenInfo");
    lua_setmetatable(L, -2);
    u->time = time;
    u->style = style;
    u->direction = direction;
    u->repeatCount = repeatCount;
    u->reverses = reverses;
    u->delayTime = delayTime;
    return 1;
}

// ---------------------------------------------------------------------------
// TweenService:Create
// ---------------------------------------------------------------------------

// Converts one goal-table value to the target property's type. The CFrame /
// UDim2 / BrickColor / Instance branches are handled here because the shared
// oboxToPropValue helper only covers string/number/bool/Vector3/Color3.
static bool convertGoalValue(lua_State* L, int idx, const std::string& type, PropValue& out,
                             std::string& err)
{
    if (type == "CFrame")
    {
        out = oboxCheckCFrame(L, idx)->c; // raises on mismatch
        return true;
    }
    if (type == "UDim2")
    {
        out = oboxCheckUDim2(L, idx)->v; // raises on mismatch
        return true;
    }
    if (type == "BrickColor")
    {
        UBrickColor* ubc = oboxCheckBrickColor(L, idx); // raises on mismatch
        size_t bcc = 0;
        const BrickColorV* bcp = brickColorPalette(bcc);
        int bci = ubc->paletteIndex;
        out = bcp[(bci < 0 || (size_t)bci >= bcc) ? 0 : bci];
        return true;
    }
    if (type == "Instance")
    {
        if (!oboxIsInstance(L, idx))
        {
            err = "Instance expected";
            return false;
        }
        out = oboxCheckInstance(L, idx);
        return true;
    }
    return oboxToPropValue(L, idx, type, out, err);
}

static int tweenServiceCreate(lua_State* L)
{
    // Called as TweenService:Create(...) — arg 1 is self.
    Instance* target = oboxCheckInstance(L, 2);
    UTweenInfo* info = oboxCheckTweenInfo(L, 3);
    luaL_checktype(L, 4, LUA_TTABLE);
    Host* h = oboxGetHost(L);

    Tween* tw = static_cast<Tween*>(h->game->createInstance("Tween"));
    if (!tw)
        luaL_errorL(L, "TweenService:Create: the Tween class is not registered");
    tw->target = target;
    tw->duration = info->time;
    tw->style = info->style;
    tw->direction = info->direction;
    tw->repeatCount = info->repeatCount;
    tw->reverses = info->reverses;
    tw->delayTime = info->delayTime;

    lua_pushnil(L);
    while (lua_next(L, 4) != 0)
    {
        // key at -2, value at -1
        if (lua_type(L, -2) != LUA_TSTRING)
            luaL_errorL(L, "TweenService:Create: goal keys must be property names");
        std::string name = lua_tostring(L, -2);
        auto dit = target->propDefs().find(name);
        if (dit == target->propDefs().end())
            luaL_errorL(L, "%s is not a valid member of %s \"%s\"", name.c_str(),
                        target->className().c_str(), target->name().c_str());
        if (dit->second.readOnly)
            luaL_errorL(L, "The %s property of %s is read-only", name.c_str(),
                        target->className().c_str());
        PropValue v;
        std::string err;
        if (!convertGoalValue(L, -1, dit->second.type, v, err))
            luaL_errorL(L, "Invalid goal value for %s: %s", name.c_str(), err.c_str());
        tw->goals[name] = v;
        lua_pop(L, 1); // pop value, keep key for lua_next
    }

    oboxPushInstance(L, tw);
    return 1;
}

// ---------------------------------------------------------------------------
// Tween methods
// ---------------------------------------------------------------------------
static int tweenPlay(lua_State* L)
{
    static_cast<Tween*>(oboxCheckInstance(L, 1))->play(oboxGetHost(L));
    return 0;
}

static int tweenCancel(lua_State* L)
{
    static_cast<Tween*>(oboxCheckInstance(L, 1))->cancel(oboxGetHost(L));
    return 0;
}

static int tweenPause(lua_State* L)
{
    static_cast<Tween*>(oboxCheckInstance(L, 1))->pause(oboxGetHost(L));
    return 0;
}

// ---------------------------------------------------------------------------
// DataStoreService / DataStore
// ---------------------------------------------------------------------------
static int dssGetDataStore(lua_State* L)
{
    DataStoreService* svc = static_cast<DataStoreService*>(oboxCheckInstance(L, 1));
    const char* name = luaL_checkstring(L, 2);
    const char* scope = luaL_optstring(L, 3, "");
    Host* h = oboxGetHost(L);
    DataStore* ds = svc->getDataStore(name, scope ? scope : "", h->game);
    if (!ds)
        luaL_errorL(L, "DataStoreService:GetDataStore: the DataStore class is not registered");
    oboxPushInstance(L, ds);
    return 1;
}

static bool toStoreVal(lua_State* L, int idx, StoreVal& out, std::string& err)
{
    switch (lua_type(L, idx))
    {
    case LUA_TNUMBER:
        out = lua_tonumber(L, idx);
        return true;
    case LUA_TSTRING:
        out = std::string(lua_tostring(L, idx));
        return true;
    case LUA_TBOOLEAN:
        out = (bool)lua_toboolean(L, idx);
        return true;
    default:
        err = "DataStore values must be a number, string or boolean";
        return false;
    }
}

static void pushStoreVal(lua_State* L, const StoreVal& v)
{
    if (std::holds_alternative<double>(v))
        lua_pushnumber(L, std::get<double>(v));
    else if (std::holds_alternative<std::string>(v))
        lua_pushstring(L, std::get<std::string>(v).c_str());
    else
        lua_pushboolean(L, std::get<bool>(v));
}

static int dsGetAsync(lua_State* L)
{
    DataStore* ds = static_cast<DataStore*>(oboxCheckInstance(L, 1));
    const char* key = luaL_checkstring(L, 2);
    StoreVal v;
    if (ds->get(key, v))
        pushStoreVal(L, v);
    else
        lua_pushnil(L);
    return 1;
}

static int dsSetAsync(lua_State* L)
{
    DataStore* ds = static_cast<DataStore*>(oboxCheckInstance(L, 1));
    const char* key = luaL_checkstring(L, 2);
    StoreVal v;
    std::string err;
    if (!toStoreVal(L, 3, v, err))
        luaL_errorL(L, "%s", err.c_str());
    ds->set(key, v);
    return 0;
}

static int dsIncrementAsync(lua_State* L)
{
    DataStore* ds = static_cast<DataStore*>(oboxCheckInstance(L, 1));
    const char* key = luaL_checkstring(L, 2);
    double delta = luaL_optnumber(L, 3, 1.0);
    double cur = 0.0;
    StoreVal v;
    if (ds->get(key, v))
    {
        if (!std::holds_alternative<double>(v))
            luaL_errorL(L, "IncrementAsync: the stored value for \"%s\" is not a number", key);
        cur = std::get<double>(v);
    }
    double next = cur + delta;
    ds->set(key, next);
    lua_pushnumber(L, next);
    return 1;
}

static int dsUpdateAsync(lua_State* L)
{
    DataStore* ds = static_cast<DataStore*>(oboxCheckInstance(L, 1));
    const char* key = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    // Call transform(currentValue) synchronously. The transform must not
    // yield; a yield raises a Luau error out of lua_call.
    lua_pushvalue(L, 3);
    StoreVal cur;
    if (ds->get(key, cur))
        pushStoreVal(L, cur);
    else
        lua_pushnil(L);
    lua_call(L, 1, 1); // errors propagate to the caller
    if (lua_isnil(L, -1))
        return 1; // a nil result leaves the stored value untouched
    StoreVal v;
    std::string err;
    if (!toStoreVal(L, -1, v, err))
        luaL_errorL(L, "%s", err.c_str());
    ds->set(key, v);
    return 1; // the transformed value stays on the stack
}

static int dsRemoveAsync(lua_State* L)
{
    DataStore* ds = static_cast<DataStore*>(oboxCheckInstance(L, 1));
    const char* key = luaL_checkstring(L, 2);
    StoreVal v;
    if (ds->get(key, v))
    {
        ds->remove(key);
        pushStoreVal(L, v);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Debris
// ---------------------------------------------------------------------------
static int debrisAddItem(lua_State* L)
{
    Instance* inst = oboxCheckInstance(L, 2);
    double lifetime = luaL_checknumber(L, 3);
    if (lifetime < 0.0)
        luaL_errorL(L, "Debris:AddItem: lifetime must be >= 0");
    Host* h = oboxGetHost(L);
    h->debris.push_back(DebrisItem{inst, h->sched->now() + lifetime});
    return 0;
}

// ---------------------------------------------------------------------------
// BindableEvent / BindableFunction
// ---------------------------------------------------------------------------
static int beFire(lua_State* L)
{
    BindableEvent* be = static_cast<BindableEvent*>(oboxCheckInstance(L, 1));
    Host* h = oboxGetHost(L);
    int nargs = lua_gettop(L) - 1; // args sit above self on the stack
    be->event.fire(*h->sched, L, nargs);
    lua_pop(L, nargs);
    return 0;
}

static int bfSetOnInvoke(lua_State* L)
{
    BindableFunction* bf = static_cast<BindableFunction*>(oboxCheckInstance(L, 1));
    if (lua_isnil(L, 2))
    {
        bf->setCallback(nullptr, 0);
        return 0;
    }
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int ref = lua_ref(L, -1);
    lua_pop(L, 1);
    bf->setCallback(L, ref);
    return 0;
}

static int bfInvoke(lua_State* L)
{
    BindableFunction* bf = static_cast<BindableFunction*>(oboxCheckInstance(L, 1));
    if (!bf->hasCallback())
        luaL_errorL(L, "BindableFunction:Invoke: no function is bound (call SetOnInvoke first)");
    int nargs = lua_gettop(L) - 1;
    // Run the bound function on a fresh thread, resumed immediately, so the
    // call looks synchronous to the invoker. A yield inside the callback is
    // rejected (see services.h).
    lua_State* co = lua_newthread(L); // pushes the thread object on L
    lua_getref(co, bf->callbackRef());
    for (int i = 2; i < 2 + nargs; ++i)
    {
        lua_pushvalue(L, i);
        lua_xmove(L, co, 1);
    }
    int status = lua_resume(co, L, nargs);
    lua_pop(L, 1); // pop the thread object
    if (status == LUA_YIELD)
        luaL_errorL(L, "BindableFunction:Invoke: the bound function yielded (not supported)");
    if (status != LUA_OK)
    {
        lua_xmove(co, L, 1); // error object
        lua_error(L);        // noreturn: re-raise in the caller
    }
    int nres = lua_gettop(co);
    lua_xmove(co, L, nres);
    return nres;
}

// ---------------------------------------------------------------------------
// Module entry points
// ---------------------------------------------------------------------------
void oboxRegisterServices(lua_State* L)
{
    luaL_newmetatable(L, "OpenBlox.TweenInfo");
    lua_setreadonly(L, -1, true);
    lua_pop(L, 1);

    lua_newtable(L);
    lua_pushcfunction(L, tweenInfoNew, "new");
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "TweenInfo");
}

bool oboxServicesIndex(lua_State* L, Instance* inst, const char* key)
{
    const std::string& cn = inst->className();

    if (cn == "Players")
    {
        if (strcmp(key, "GetPlayers") == 0)
        {
            lua_pushcfunction(L, playersGetPlayers, "GetPlayers");
            return true;
        }
        if (strcmp(key, "GetPlayerByUserId") == 0)
        {
            lua_pushcfunction(L, playersGetPlayerByUserId, "GetPlayerByUserId");
            return true;
        }
        return false;
    }
    if (cn == "Teams")
    {
        if (strcmp(key, "GetTeams") == 0)
        {
            lua_pushcfunction(L, teamsGetTeams, "GetTeams");
            return true;
        }
        return false;
    }
    if (cn == "Sound")
    {
        if (strcmp(key, "Play") == 0)
        {
            lua_pushcfunction(L, soundPlay, "Play");
            return true;
        }
        if (strcmp(key, "Stop") == 0)
        {
            lua_pushcfunction(L, soundStop, "Stop");
            return true;
        }
        if (strcmp(key, "Pause") == 0)
        {
            lua_pushcfunction(L, soundPause, "Pause");
            return true;
        }
        if (strcmp(key, "Resume") == 0)
        {
            lua_pushcfunction(L, soundResume, "Resume");
            return true;
        }
        return false;
    }
    if (cn == "TweenService")
    {
        if (strcmp(key, "Create") == 0)
        {
            lua_pushcfunction(L, tweenServiceCreate, "Create");
            return true;
        }
        return false;
    }
    if (cn == "Tween")
    {
        if (strcmp(key, "Play") == 0)
        {
            lua_pushcfunction(L, tweenPlay, "Play");
            return true;
        }
        if (strcmp(key, "Cancel") == 0)
        {
            lua_pushcfunction(L, tweenCancel, "Cancel");
            return true;
        }
        if (strcmp(key, "Pause") == 0)
        {
            lua_pushcfunction(L, tweenPause, "Pause");
            return true;
        }
        return false;
    }
    if (cn == "DataStoreService")
    {
        if (strcmp(key, "GetDataStore") == 0)
        {
            lua_pushcfunction(L, dssGetDataStore, "GetDataStore");
            return true;
        }
        return false;
    }
    if (cn == "DataStore")
    {
        if (strcmp(key, "GetAsync") == 0)
        {
            lua_pushcfunction(L, dsGetAsync, "GetAsync");
            return true;
        }
        if (strcmp(key, "SetAsync") == 0)
        {
            lua_pushcfunction(L, dsSetAsync, "SetAsync");
            return true;
        }
        if (strcmp(key, "IncrementAsync") == 0)
        {
            lua_pushcfunction(L, dsIncrementAsync, "IncrementAsync");
            return true;
        }
        if (strcmp(key, "UpdateAsync") == 0)
        {
            lua_pushcfunction(L, dsUpdateAsync, "UpdateAsync");
            return true;
        }
        if (strcmp(key, "RemoveAsync") == 0)
        {
            lua_pushcfunction(L, dsRemoveAsync, "RemoveAsync");
            return true;
        }
        return false;
    }
    if (cn == "Debris")
    {
        if (strcmp(key, "AddItem") == 0)
        {
            lua_pushcfunction(L, debrisAddItem, "AddItem");
            return true;
        }
        return false;
    }
    if (cn == "BindableEvent")
    {
        if (strcmp(key, "Fire") == 0)
        {
            lua_pushcfunction(L, beFire, "Fire");
            return true;
        }
        return false;
    }
    if (cn == "BindableFunction")
    {
        if (strcmp(key, "SetOnInvoke") == 0)
        {
            lua_pushcfunction(L, bfSetOnInvoke, "SetOnInvoke");
            return true;
        }
        if (strcmp(key, "Invoke") == 0)
        {
            lua_pushcfunction(L, bfInvoke, "Invoke");
            return true;
        }
        return false;
    }
    return false;
}
