// OpenBlox Luau bindings: exposes game, Instance, Vector3, task, events.
// Clean-room implementation against the public Luau C API.
#pragma once

#include <list>
#include <set>
#include <string>
#include <utility>

#include "rbx.h"
#include "scheduler.h"

struct lua_State;

struct Host
{
    lua_State* L = nullptr;
    DataModel* game = nullptr;
    Scheduler* sched = nullptr;
    std::list<std::string> names; // stable storage for per-thread script names
    std::set<std::pair<Instance*, Instance*>> touching; // active AABB touch pairs
};

// Registers every OpenBlox global (game, Instance, Vector3, task, tick,
// wait, print, warn, require) on the state. Must be called after the safe
// standard libraries are opened.
void oboxRegisterApi(lua_State* L, Host* host);

// Retrieves the Host stored in the registry by oboxRegisterApi.
Host* oboxGetHost(lua_State* L);

// Compiles and schedules a script file to run on its own thread.
bool oboxRunFile(Host* host, const std::string& path);

// Runs the Heartbeat loop for `frames` frames at `dt`, then drains any
// remaining scheduled work.
void oboxRunFrames(Host* host, int frames, double dt);
