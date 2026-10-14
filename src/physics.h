// OpenBlock physics + humanoid module: gravity integration, AABB collision,
// and the Humanoid character controller. Clean-room implementation for the
// OpenBlock open-source voxel game engine.
#pragma once

#include <string>

#include "bindings.h" // Host (lua_State*, DataModel*, Scheduler*)
#include "rbx.h"      // Instance, Signal, Vec3

// ---------------------------------------------------------------------------
// Humanoid: character controller state for an OpenBlock avatar rig.
// Lives under a Model; BuildRig populates the Model with body parts and the
// physics step drives the HumanoidRootPart for MoveTo / Jump.
// ---------------------------------------------------------------------------
class Humanoid : public Instance
{
public:
    Humanoid();

    Signal died;           // fires (no args) when Health reaches 0
    Signal moveToFinished; // fires (no args) when a MoveTo target is reached

    Signal* findSignal(const std::string& name) override;
    // Clamps Health into [0, MaxHealth]; everything else behaves like Instance.
    std::string setProp(const std::string& key, const PropValue& v) override;

    // C++-side state (kept public so the physics step and bindings share it).
    bool m_deadFired = false;    // Died already fired for the current death
    bool m_hasMoveTarget = false;
    Vec3 m_moveTarget{0.0, 0.0, 0.0};
};

// Registers Humanoid methods on the Luau state (currently no globals needed;
// kept for uniformity with the other binding modules).
void oboxRegisterPhysics(lua_State* L);

// Instance __index hook: pushes Humanoid methods (TakeDamage, MoveTo,
// BuildRig). Returns true when the key was handled, false otherwise.
bool oboxPhysicsIndex(lua_State* L, Instance* inst, const char* key);

// Advances the physics simulation by dt seconds: integrates unanchored parts
// under Workspace, resolves AABB collisions against anchored parts, then runs
// Humanoid movement / jump / death logic. Called once per frame by the host
// frame loop, before touch detection.
void oboxPhysicsStep(Host* host, double dt);
