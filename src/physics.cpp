// OpenBlock physics + humanoid implementation. Clean-room code.
//
// Simulation model (documented limitations):
// - Gravity integrates unanchored Part velocity/position each frame.
// - Collision is AABB-only (no rotation) and resolves unanchored, CanCollide
//   parts against anchored parts only; part-vs-part dynamic collision is not
//   simulated.
// - When the Part "Velocity" property is unavailable, per-part velocities are
//   kept in a small fallback table so gravity still works.
#include "physics.h"

#include <lua.h>

#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// Humanoid
// ---------------------------------------------------------------------------
Humanoid::Humanoid() : Instance("Humanoid")
{
    defProp("Health", "number", 100.0);
    defProp("MaxHealth", "number", 100.0);
    defProp("WalkSpeed", "number", 16.0);
    defProp("JumpPower", "number", 50.0);
    defProp("Jump", "bool", false);
    defProp("Sit", "bool", false);
    defProp("DisplayName", "string", std::string(""));
}

Signal* Humanoid::findSignal(const std::string& name)
{
    if (name == "Died")
        return &died;
    if (name == "MoveToFinished")
        return &moveToFinished;
    return Instance::findSignal(name);
}

std::string Humanoid::setProp(const std::string& key, const PropValue& v)
{
    if (key == "Health")
    {
        const double* d = std::get_if<double>(&v);
        if (!d)
            return "Type mismatch setting Health (expected number)";
        double maxHealth = 100.0;
        PropValue mv;
        if (getProp("MaxHealth", mv))
            if (const double* m = std::get_if<double>(&mv))
                maxHealth = *m;
        double clamped = *d;
        if (clamped < 0.0)
            clamped = 0.0;
        if (clamped > maxHealth)
            clamped = maxHealth;
        return Instance::setProp(key, clamped);
    }
    return Instance::setProp(key, v);
}

// ---------------------------------------------------------------------------
// Small property helpers
// ---------------------------------------------------------------------------
static bool getNumProp(Instance* p, const char* key, double& out)
{
    PropValue pv;
    if (!p->getProp(key, pv))
        return false;
    if (const double* d = std::get_if<double>(&pv))
    {
        out = *d;
        return true;
    }
    return false;
}

static bool getBoolProp(Instance* p, const char* key, bool& out)
{
    PropValue pv;
    if (!p->getProp(key, pv))
        return false;
    if (const bool* b = std::get_if<bool>(&pv))
    {
        out = *b;
        return true;
    }
    return false;
}

static bool getVec3Prop(Instance* p, const char* key, Vec3& out)
{
    PropValue pv;
    if (!p->getProp(key, pv))
        return false;
    if (const Vec3* v = std::get_if<Vec3>(&pv))
    {
        out = *v;
        return true;
    }
    return false;
}

// Fallback per-part velocities, used only when the Part "Velocity" property
// is not defined. Entries are pruned every step to parts currently simulated.
static std::unordered_map<Instance*, Vec3>& fallbackVel()
{
    static std::unordered_map<Instance*, Vec3> s;
    return s;
}

static Vec3 readVel(Instance* p)
{
    Vec3 v{0.0, 0.0, 0.0};
    if (getVec3Prop(p, "Velocity", v))
        return v;
    auto it = fallbackVel().find(p);
    if (it != fallbackVel().end())
        return it->second;
    return v;
}

// Fires Changed + the per-property signal, mirroring instanceNewIndex in
// bindings.cpp (Changed carries the property name; the per-property signal
// carries no arguments).
static void firePropChanged(Host* host, Instance* inst, const char* key)
{
    lua_State* L = host->L;
    lua_pushstring(L, key);
    inst->changed.fire(*host->sched, L, 1);
    lua_pop(L, 1);
    // Per-property signal (GetPropertyChangedSignal).
    inst->getPropertyChangedSignal(key)->fire(*host->sched, L, 0);
}

static void writeVel(Host* host, Instance* p, const Vec3& v, bool fire)
{
    Vec3 cur{0.0, 0.0, 0.0};
    if (getVec3Prop(p, "Velocity", cur))
    {
        p->setProp("Velocity", v);
        if (fire)
            firePropChanged(host, p, "Velocity");
    }
    else
    {
        fallbackVel()[p] = v;
    }
}

static void collectParts(Instance* root, std::vector<Instance*>& out)
{
    for (Instance* c : root->children())
    {
        if (c->isDestroyed())
            continue;
        if (c->isA("Part"))
            out.push_back(c);
        collectParts(c, out);
    }
}

// ---------------------------------------------------------------------------
// oboxPhysicsStep
// ---------------------------------------------------------------------------
void oboxPhysicsStep(Host* host, double dt)
{
    if (!host || !host->game || !host->sched || !host->L || dt <= 0.0)
        return;
    Instance* ws = host->game->getService("Workspace");
    if (!ws)
        return;

    // (a) Collect Parts under Workspace (recursive; skip destroyed).
    std::vector<Instance*> parts;
    collectParts(ws, parts);

    // Prune fallback velocities for parts that are no longer simulated.
    {
        auto& fv = fallbackVel();
        if (!fv.empty())
        {
            std::unordered_set<Instance*> live(parts.begin(), parts.end());
            for (auto it = fv.begin(); it != fv.end();)
                it = live.count(it->first) ? std::next(it) : fv.erase(it);
        }
    }

    // (b) Gravity (Workspace.Gravity, default 196.2).
    double gravity = 196.2;
    getNumProp(ws, "Gravity", gravity);

    std::vector<Instance*> anchored;
    std::vector<Instance*> dynamic;
    for (Instance* p : parts)
    {
        bool a = false;
        getBoolProp(p, "Anchored", a);
        (a ? anchored : dynamic).push_back(p);
    }

    // (c) Integrate unanchored parts.
    for (Instance* p : dynamic)
    {
        Vec3 pos{0.0, 0.0, 0.0};
        getVec3Prop(p, "Position", pos);
        Vec3 vel = readVel(p);
        vel.y -= gravity * dt;
        pos.x += vel.x * dt;
        pos.y += vel.y * dt;
        pos.z += vel.z * dt;
        p->setProp("Position", pos);
        firePropChanged(host, p, "Position");
        writeVel(host, p, vel, true);
    }

    // (d) AABB collision: unanchored + CanCollide parts against anchored
    // parts. Resolve along the minimum-penetration axis; a +Y resolution
    // means the part landed, so its downward velocity is cleared.
    for (Instance* p : dynamic)
    {
        bool canCollide = true;
        getBoolProp(p, "CanCollide", canCollide);
        if (!canCollide)
            continue;
        Vec3 pos{0.0, 0.0, 0.0}, size{0.0, 0.0, 0.0};
        getVec3Prop(p, "Position", pos);
        getVec3Prop(p, "Size", size);
        Vec3 vel = readVel(p);
        bool moved = false;
        bool landed = false;
        for (Instance* a : anchored)
        {
            Vec3 apos{0.0, 0.0, 0.0}, asize{0.0, 0.0, 0.0};
            getVec3Prop(a, "Position", apos);
            getVec3Prop(a, "Size", asize);
            double dx = pos.x - apos.x;
            double dy = pos.y - apos.y;
            double dz = pos.z - apos.z;
            double ox = (size.x + asize.x) * 0.5 - std::fabs(dx);
            double oy = (size.y + asize.y) * 0.5 - std::fabs(dy);
            double oz = (size.z + asize.z) * 0.5 - std::fabs(dz);
            if (ox <= 0.0 || oy <= 0.0 || oz <= 0.0)
                continue;
            moved = true;
            if (ox <= oy && ox <= oz)
                pos.x += (dx >= 0.0 ? ox : -ox);
            else if (oy <= oz)
            {
                pos.y += (dy >= 0.0 ? oy : -oy);
                if (dy >= 0.0)
                    landed = true;
            }
            else
                pos.z += (dz >= 0.0 ? oz : -oz);
        }
        if (moved)
        {
            p->setProp("Position", pos);
            firePropChanged(host, p, "Position");
        }
        if (landed)
        {
            vel.y = 0.0;
            writeVel(host, p, vel, true);
        }
    }

    // (e) Humanoids: death, jump, and MoveTo driving the HumanoidRootPart.
    // Runs after integration/collision so control velocities apply cleanly.
    std::vector<Instance*> all;
    host->game->getDescendants(all);
    for (Instance* i : all)
    {
        if (i->isDestroyed() || i->className() != "Humanoid")
            continue;
        Humanoid* h = static_cast<Humanoid*>(i);

        double health = 100.0, walkSpeed = 16.0, jumpPower = 50.0;
        bool jump = false;
        getNumProp(h, "Health", health);
        getNumProp(h, "WalkSpeed", walkSpeed);
        getNumProp(h, "JumpPower", jumpPower);
        getBoolProp(h, "Jump", jump);

        if (health <= 0.0 && !h->m_deadFired)
        {
            h->m_deadFired = true;
            h->died.fire(*host->sched, host->L, 0);
        }

        Instance* parent = h->parent();
        if (!parent)
            continue;
        Instance* root = parent->findFirstChild("HumanoidRootPart");
        if (!root || root->isDestroyed() || !root->isA("Part"))
            continue;

        if (jump)
        {
            Vec3 rv = readVel(root);
            rv.y = jumpPower;
            writeVel(host, root, rv, true);
            h->setProp("Jump", false);
            firePropChanged(host, h, "Jump");
        }

        if (h->m_hasMoveTarget)
        {
            Vec3 rp{0.0, 0.0, 0.0};
            getVec3Prop(root, "Position", rp);
            double dx = h->m_moveTarget.x - rp.x;
            double dz = h->m_moveTarget.z - rp.z;
            double dist = std::sqrt(dx * dx + dz * dz);
            Vec3 rv = readVel(root);
            if (dist > 0.5 && dist > 1e-9)
            {
                rv.x = dx / dist * walkSpeed;
                rv.z = dz / dist * walkSpeed;
                writeVel(host, root, rv, true);
            }
            else
            {
                h->m_hasMoveTarget = false;
                rv.x = 0.0;
                rv.z = 0.0;
                writeVel(host, root, rv, true);
                h->moveToFinished.fire(*host->sched, host->L, 0);
            }
        }
    }
}
