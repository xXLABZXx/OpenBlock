// OpenBlox task scheduler: deferred calls, task.wait/task.delay, Heartbeat.
// Roblox runs event handlers and task.* callbacks on a scheduler; this is a
// minimal cooperative version of that idea on top of Luau coroutines.
#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

struct lua_State;

class Scheduler
{
public:
    explicit Scheduler(lua_State* mainThread);

    double now() const { return m_now; }
    lua_State* mainThread() const { return m_main; }

    // Stack on `from`: [func, arg1..argN]. Moves them onto a fresh thread and
    // schedules it to run after `delay` simulated seconds.
    void defer(lua_State* from, int nargs, double delay);

    // Schedules an already-prepared thread whose stack is [func, arg1..argN].
    void deferThread(lua_State* co, int nargs, double delay);

    // Suspends the calling coroutine until now()+seconds. Must be followed by
    // lua_yield by the caller. Only valid on non-main threads.
    void wait(lua_State* co, double seconds);

    // Advances the clock by dt and resumes everything that is due.
    void step(double dt);

    // Removes a thread from all scheduler queues (task.cancel). Releases its
    // registry anchor. Returns true if the thread was scheduled.
    bool cancel(lua_State* co);

    // Parks a thread: it stays suspended with no wake time until something
    // resumes it explicitly (used by Signal:Wait). Must be followed by
    // lua_yield by the caller.
    void park(lua_State* co);
    // Removes a parked thread from the parked set (it is about to be resumed
    // by other means, e.g. a signal firing with arguments).
    void unpark(lua_State* co);

    bool idle() const { return m_deferred.empty() && m_sleeping.empty(); }

private:
    struct Work
    {
        lua_State* co = nullptr;
        int nargs = 0;
        double wake = 0.0;
    };

    void resumeWork(Work w, int nargs);
    void reportError(lua_State* co);

    lua_State* m_main;
    double m_now = 0.0;
    std::vector<Work> m_deferred;
    std::vector<Work> m_sleeping; // threads suspended via wait()
    std::unordered_set<lua_State*> m_sleepSet;
    // Registry anchors keeping scheduled threads alive across GC.
    std::unordered_map<lua_State*, int> m_anchors;

    void anchor(lua_State* co);
    void release(lua_State* co);
};
