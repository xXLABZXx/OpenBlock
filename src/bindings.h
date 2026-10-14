// OpenBlox Luau bindings: exposes game, Instance, Vector3, task, events.
// Clean-room implementation against the public Luau C API.
#pragma once

#include <list>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rbx.h"
#include "scheduler.h"

struct lua_State;

struct DebrisItem
{
    Instance* inst = nullptr;
    double expireAt = 0.0;
};

// Cached per-part touch state for the dirty-skip fast path in touchCheck:
// when the collected part set and all positions/sizes are identical to last
// frame, pair processing is skipped entirely.
struct PartCache
{
    Part* part = nullptr;
    Vec3 pos;
    Vec3 size;
};

struct Host
{
    lua_State* L = nullptr;
    DataModel* game = nullptr;
    Scheduler* sched = nullptr;
    std::list<std::string> names; // stable storage for per-thread script names
    std::set<std::pair<Instance*, Instance*>> touching; // active AABB touch pairs
    std::vector<Instance*> activeTweens; // Tweens with Play() called, not yet finished
    std::vector<DebrisItem> debris;     // Debris.AddItem entries awaiting expiry
    // One cached Luau userdata per Instance (registry refs). pushInstance
    // reuses the cached userdata so object identity holds across API calls:
    // `part == workspace.Part`, table keys, and event args all agree.
    std::unordered_map<Instance*, int> instanceRefs;
    // Wall-clock deadline for oboxRunFrames: once oboxWallNow() passes this,
    // the frame loop and drain loop stop. Set by the CLI from --seconds so
    // `while true do task.wait(0) end` cannot spin past the requested run.
    double runDeadline = 1e30;
    // Touched edge-trigger priming: the first touchCheck call populates
    // `touching` with the currently-overlapping pairs WITHOUT firing events,
    // so pre-existing overlaps at startup don't report spurious Touched.
    bool touchPrimed = false;
    // Dirty-skip cache for touchCheck (see PartCache above).
    std::vector<PartCache> lastTouchParts;
    bool lastTouchValid = false;
    // Set by touchCheck when any collected part is unanchored (physics may
    // move it); consulted by the oboxRunFrames early-exit check.
    bool sawUnanchored = false;
};

// Property value helpers shared by binding modules (defined in bindings.cpp).
void oboxPushPropValue(lua_State* L, const PropValue& v);
// Returns true on success; on failure returns false and sets err.
bool oboxToPropValue(lua_State* L, int idx, const std::string& type, PropValue& out,
                     std::string& err);

// Registers every OpenBlox global (game, Instance, Vector3, task, tick,
// wait, print, warn, require) on the state. Must be called after the safe
// standard libraries are opened.
void oboxRegisterApi(lua_State* L, Host* host);

// Retrieves the Host stored in the registry by oboxRegisterApi.
Host* oboxGetHost(lua_State* L);

// Instance userdata helpers shared by binding modules (defined in bindings.cpp).
// checkInstance raises a Luau type error when idx is not an Instance.
Instance* oboxCheckInstance(lua_State* L, int idx);
bool oboxIsInstance(lua_State* L, int idx);
void oboxPushInstance(lua_State* L, Instance* inst);

// Compiles and schedules a script file to run on its own thread.
bool oboxRunFile(Host* host, const std::string& path);

// Runs the Heartbeat loop for `frames` frames at `dt`, then drains any
// remaining scheduled work.
void oboxRunFrames(Host* host, int frames, double dt);

// Wall-clock time in seconds (steady clock), defined in main.cpp. Used for
// the run deadline that bounds oboxRunFrames.
double oboxWallNow();

// Per-module API registration and per-frame steps (implemented in the
// bindings_math / bindings_services / bindings_physics modules).
// oboxXxxIndex dispatches class-specific methods from instance __index;
// returns true when `key` was handled (value left on the stack).
bool oboxServicesIndex(lua_State* L, Instance* inst, const char* key);
bool oboxPhysicsIndex(lua_State* L, Instance* inst, const char* key);
void oboxServiceStep(Host* host, double dt);
void oboxPhysicsStep(Host* host, double dt);
