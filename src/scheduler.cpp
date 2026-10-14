// OpenBlox task scheduler implementation. Clean-room code.
#include "scheduler.h"

#include <lua.h>
#include <lualib.h>

#include <cmath>
#include <cstdio>

Scheduler::Scheduler(lua_State* mainThread) : m_main(mainThread)
{
}

// Delays must be finite and non-negative: a NaN/Inf wake time would never be
// reached, pinning the coroutine in the queue forever.
static double saneDelay(double delay)
{
    if (!std::isfinite(delay) || delay < 0.0)
        return 0.0;
    return delay;
}

void Scheduler::defer(lua_State* from, int nargs, double delay)
{
    // Stack on `from`: [func, arg1..argN]
    lua_State* co = lua_newthread(from); // pushes the thread object on `from`
    lua_xmove(from, co, nargs + 1);      // move func + args to co (order kept)
    lua_pop(from, 1);                    // pop the thread object
    deferThread(co, nargs, delay);
}

void Scheduler::deferThread(lua_State* co, int nargs, double delay)
{
    anchor(co);
    m_deferred.push_back(Work{co, nargs, m_now + saneDelay(delay)});
}

void Scheduler::anchor(lua_State* co)
{
    if (m_anchors.find(co) != m_anchors.end())
        return;
    lua_pushthread(co);
    int ref = lua_ref(co, -1);
    lua_pop(co, 1);
    m_anchors[co] = ref;
}

void Scheduler::release(lua_State* co)
{
    auto it = m_anchors.find(co);
    if (it != m_anchors.end())
    {
        lua_unref(m_main, it->second);
        m_anchors.erase(it);
    }
}

void Scheduler::wait(lua_State* co, double seconds)
{
    double s = saneDelay(seconds);
    m_sleeping.push_back(Work{co, 0, m_now + s});
    m_sleepSet.insert(co);
}

void Scheduler::park(lua_State* co)
{
    m_sleepSet.insert(co);
}

void Scheduler::unpark(lua_State* co)
{
    m_sleepSet.erase(co);
}

bool Scheduler::isParked(lua_State* co) const
{
    return m_sleepSet.find(co) != m_sleepSet.end();
}

bool Scheduler::dropThread(lua_State* co)
{
    bool found = false;
    for (size_t i = 0; i < m_deferred.size();)
    {
        if (m_deferred[i].co == co)
        {
            m_deferred.erase(m_deferred.begin() + (long)i);
            found = true;
        }
        else
        {
            ++i;
        }
    }
    for (size_t i = 0; i < m_sleeping.size();)
    {
        if (m_sleeping[i].co == co)
        {
            m_sleeping.erase(m_sleeping.begin() + (long)i);
            found = true;
        }
        else
        {
            ++i;
        }
    }
    if (m_sleepSet.erase(co) != 0)
        found = true; // parked (e.g. blocked in Signal:Wait)
    return found;
}

bool Scheduler::cancel(lua_State* co)
{
    bool found = dropThread(co);
    if (found)
        release(co);
    return found;
}

void Scheduler::step(double dt)
{
    m_now += dt;

    // Resume deferred calls that are due.
    for (size_t i = 0; i < m_deferred.size();)
    {
        if (m_deferred[i].wake <= m_now)
        {
            Work w = m_deferred[i];
            m_deferred.erase(m_deferred.begin() + (long)i);
            resumeWork(w, w.nargs);
        }
        else
        {
            ++i;
        }
    }

    // Wake sleeping threads.
    for (size_t i = 0; i < m_sleeping.size();)
    {
        if (m_sleeping[i].wake <= m_now)
        {
            Work w = m_sleeping[i];
            m_sleeping.erase(m_sleeping.begin() + (long)i);
            m_sleepSet.erase(w.co);
            resumeWork(w, 0);
        }
        else
        {
            ++i;
        }
    }
}

void Scheduler::resumeWork(Work w, int nargs)
{
    // Arm the per-resume execution budget (set by main.cpp's interrupt hook).
    extern double g_oboxDeadline;
    extern double oboxWallNow();
    g_oboxDeadline = oboxWallNow() + 2.0; // 2s wall-clock per resume, prototype value

    double cpuStart = oboxWallNow();
    int status = lua_resume(w.co, m_main, nargs);
    m_cpuUsed += oboxWallNow() - cpuStart;
    g_oboxDeadline = 1e30;

    if (status == LUA_OK)
    {
        release(w.co); // thread finished
        return;
    }
    if (status == LUA_YIELD)
    {
        // Only legal if the thread suspended itself via wait()/park().
        if (m_sleepSet.find(w.co) == m_sleepSet.end())
        {
            std::printf("[OpenBlox] error: thread yielded outside of task.wait()\n");
            release(w.co);
        }
        return;
    }
    reportError(w.co);
    // Remove from every queue: a dead thread must never be resumed again.
    dropThread(w.co);
    release(w.co);
}

void Scheduler::reportError(lua_State* co)
{
    const char* scriptName = (const char*)lua_getthreaddata(co);
    const char* msg = lua_tostring(co, -1);
    std::printf("[OpenBlox] error in script '%s': %s\n", scriptName ? scriptName : "?",
                msg ? msg : "(no message)");
    luaL_traceback(m_main, co, nullptr, 1);
    const char* tb = lua_tostring(m_main, -1);
    if (tb)
        std::printf("%s\n", tb);
    lua_pop(m_main, 1);
    lua_pop(co, 1); // pop the error message
}
