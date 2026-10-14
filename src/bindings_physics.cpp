// OpenBlock physics bindings: Humanoid methods (TakeDamage, MoveTo, BuildRig).
// Clean-room implementation against the public Luau C API.
#include "physics.h"

#include "bindings.h"

#include <lua.h>
#include <lualib.h>
#include <luacode.h>

#include <cstring>
#include <string>

// Builds a simple character rig under the Humanoid's parent Model.
// Note: Model has no Position property, so the rig is built around the (0,0,0)
// origin. Six parts are created; the invisible HumanoidRootPart stands in the
// torso slot (same size/position a visible torso would use).
static void buildRig(lua_State* L, Host* host, Humanoid* hum)
{
    Instance* parent = hum->parent();
    if (!parent || !parent->isA("Model"))
        luaL_errorL(L, "BuildRig requires the Humanoid to be parented to a Model");

    struct RigPart
    {
        const char* name;
        Vec3 size;
        Vec3 pos;
        bool canCollide;
        double transparency;
    };
    // Positions are relative to the Model origin (0, 0, 0).
    static const RigPart rig[] = {
        {"HumanoidRootPart", {2.0, 2.0, 1.0}, {0.0, 4.0, 0.0}, true, 1.0},
        {"Head", {1.2, 1.2, 1.2}, {0.0, 5.62, 0.0}, false, 0.0},
        {"Left Arm", {1.0, 2.0, 1.0}, {-1.5, 4.0, 0.0}, false, 0.0},
        {"Right Arm", {1.0, 2.0, 1.0}, {1.5, 4.0, 0.0}, false, 0.0},
        {"Left Leg", {1.0, 2.0, 1.0}, {-0.5, 2.0, 0.0}, false, 0.0},
        {"Right Leg", {1.0, 2.0, 1.0}, {0.5, 2.0, 0.0}, false, 0.0},
    };

    for (const RigPart& rp : rig)
    {
        Instance* p = host->game->createInstance("Part");
        if (!p)
            luaL_errorL(L, "BuildRig failed: could not create Part");
        p->setName(rp.name);
        std::string err = p->setProp("Size", rp.size);
        if (err.empty())
            err = p->setProp("Position", rp.pos);
        if (err.empty())
            err = p->setProp("Transparency", rp.transparency);
        if (err.empty())
            err = p->setProp("CanCollide", rp.canCollide);
        if (err.empty())
            err = p->setProp("Anchored", false);
        if (!err.empty())
            luaL_errorL(L, "BuildRig failed: %s", err.c_str());
        p->setParent(parent);
        // Mirror the ChildAdded event that assigning Parent fires.
        oboxPushInstance(L, p);
        parent->childAdded.fire(*host->sched, L, 1);
        lua_pop(L, 1);
    }
}

static int humTakeDamage(lua_State* L)
{
    Humanoid* h = static_cast<Humanoid*>(oboxCheckInstance(L, 1));
    double amount = luaL_checknumber(L, 2);
    PropValue cur;
    double health = 0.0;
    if (h->getProp("Health", cur))
        if (const double* d = std::get_if<double>(&cur))
            health = *d;
    std::string err = h->setProp("Health", health - amount); // clamps to [0, MaxHealth]
    if (!err.empty())
        luaL_errorL(L, "%s", err.c_str());
    // Changed fires deferred, like a property assignment.
    Host* host = oboxGetHost(L);
    lua_pushstring(L, "Health");
    h->changed.fire(*host->sched, L, 1);
    lua_pop(L, 1);
    h->getPropertyChangedSignal("Health")->fire(*host->sched, L, 0);
    return 0;
}

static int humMoveTo(lua_State* L)
{
    Humanoid* h = static_cast<Humanoid*>(oboxCheckInstance(L, 1));
    PropValue pv;
    std::string err;
    if (!oboxToPropValue(L, 2, "Vector3", pv, err))
        luaL_errorL(L, "Invalid MoveTo target: %s", err.c_str());
    const Vec3* v = std::get_if<Vec3>(&pv);
    if (!v)
        luaL_errorL(L, "Invalid MoveTo target");
    h->m_moveTarget = *v;
    h->m_hasMoveTarget = true;
    return 0;
}

static int humBuildRig(lua_State* L)
{
    Humanoid* h = static_cast<Humanoid*>(oboxCheckInstance(L, 1));
    buildRig(L, oboxGetHost(L), h);
    return 0;
}

bool oboxPhysicsIndex(lua_State* L, Instance* inst, const char* key)
{
    if (inst->className() != "Humanoid")
        return false;
    if (std::strcmp(key, "TakeDamage") == 0)
    {
        lua_pushcfunction(L, humTakeDamage, "TakeDamage");
        return true;
    }
    if (std::strcmp(key, "MoveTo") == 0)
    {
        lua_pushcfunction(L, humMoveTo, "MoveTo");
        return true;
    }
    if (std::strcmp(key, "BuildRig") == 0)
    {
        lua_pushcfunction(L, humBuildRig, "BuildRig");
        return true;
    }
    return false;
}

void oboxRegisterPhysics(lua_State* L)
{
    // No globals are needed: Humanoid instances are created through
    // Instance.new("Humanoid") and their methods resolve via oboxPhysicsIndex.
    (void)L;
}
