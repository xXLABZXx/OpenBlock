// OpenBlock services module implementation: Players, Lighting, Teams,
// Camera, Workspace, SoundService, TweenService, DataStoreService, Debris,
// BindableEvent and BindableFunction. Clean-room code.
#include "services.h"

#include "bindings.h" // Host, oboxPushInstance
#include "mathx.h"    // easeValue, cfLerp, cfFromPos, cfIdentity

#include <lua.h>
#include <lualib.h>

#include <algorithm>
#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------
Player::Player() : Instance("Player")
{
    defProp("UserId", "number", 0.0);
    defProp("DisplayName", "string", std::string(""));
    defProp("Character", "Instance", static_cast<Instance*>(nullptr));
}

Players::Players() : ServiceInstance("Players")
{
}

Lighting::Lighting() : ServiceInstance("Lighting")
{
    defProp("Ambient", "Color3", Color3{0.5, 0.5, 0.5});
    defProp("OutdoorAmbient", "Color3", Color3{0.5, 0.5, 0.5});
    defProp("Brightness", "number", 2.0);
    defProp("ClockTime", "number", 14.0);
    defProp("TimeOfDay", "string", std::string("14:00:00"));
    defProp("GeographicLatitude", "number", 0.0);
    defProp("FogStart", "number", 0.0);
    defProp("FogEnd", "number", 100000.0);
    defProp("FogColor", "Color3", Color3{0.75, 0.75, 0.75});
}

Team::Team() : Instance("Team")
{
    defProp("TeamColor", "Color3", Color3{1.0, 1.0, 1.0});
    defProp("AutoAssignable", "bool", true);
}

Teams::Teams() : ServiceInstance("Teams")
{
}

Camera::Camera() : Instance("Camera")
{
    defProp("CFrame", "CFrame", cfFromPos(0.0, 12.0, 24.0));
    defProp("FieldOfView", "number", 70.0);
    defProp("CameraType", "string", std::string("Custom"));
    defProp("Focus", "CFrame", cfIdentity());
}

Workspace::Workspace() : ServiceInstance("Workspace")
{
    defProp("CurrentCamera", "Instance", static_cast<Instance*>(nullptr));
    defProp("Gravity", "number", 196.2);
}

SoundService::SoundService() : ServiceInstance("SoundService")
{
}

Sound::Sound() : Instance("Sound")
{
    defProp("SoundId", "string", std::string(""));
    defProp("Volume", "number", 0.5);
    defProp("PlaybackSpeed", "number", 1.0);
    defProp("Looped", "bool", false);
    defProp("Playing", "bool", false);
    defProp("TimePosition", "number", 0.0);
}

TweenService::TweenService() : ServiceInstance("TweenService")
{
}

Tween::Tween() : Instance("Tween")
{
    defProp("PlaybackState", "string", std::string("Delayed"));
}

DataStoreService::DataStoreService() : ServiceInstance("DataStoreService")
{
}

DataStore::DataStore() : Instance("DataStore")
{
}

Debris::Debris() : ServiceInstance("Debris")
{
}

BindableEvent::BindableEvent() : Instance("BindableEvent")
{
}

BindableFunction::BindableFunction() : Instance("BindableFunction")
{
}

// ---------------------------------------------------------------------------
// Signal lookup
// ---------------------------------------------------------------------------
Signal* Players::findSignal(const std::string& name)
{
    if (name == "PlayerAdded")
        return &playerAdded;
    if (name == "PlayerRemoving")
        return &playerRemoving;
    return ServiceInstance::findSignal(name);
}

Signal* Tween::findSignal(const std::string& name)
{
    if (name == "Completed")
        return &completed;
    return Instance::findSignal(name);
}

Signal* BindableEvent::findSignal(const std::string& name)
{
    if (name == "Event")
        return &event;
    return Instance::findSignal(name);
}

// ---------------------------------------------------------------------------
// Changed firing for C++-side property writes (mirrors instanceNewIndex).
// ---------------------------------------------------------------------------
void oboxFirePropChanged(Host* host, Instance* inst, const std::string& name)
{
    lua_State* L = host->L;
    lua_pushstring(L, name.c_str());
    inst->changed.fire(*host->sched, L, 1);
    lua_pop(L, 1);
    inst->getPropertyChangedSignal(name)->fire(*host->sched, L, 0);
}

// ---------------------------------------------------------------------------
// Players polling
// ---------------------------------------------------------------------------
void Players::poll(Host* host)
{
    lua_State* L = host->L;
    std::set<Instance*> current;
    for (Instance* c : children())
    {
        if (c->className() == "Player")
            current.insert(c);
    }
    for (Instance* p : current)
    {
        if (m_known.find(p) == m_known.end())
        {
            m_known.insert(p);
            oboxPushInstance(L, p);
            playerAdded.fire(*host->sched, L, 1);
            lua_pop(L, 1);
        }
    }
    std::vector<Instance*> gone;
    for (Instance* p : m_known)
    {
        if (current.find(p) == current.end())
            gone.push_back(p);
    }
    for (Instance* p : gone)
    {
        m_known.erase(p);
        oboxPushInstance(L, p);
        playerRemoving.fire(*host->sched, L, 1);
        lua_pop(L, 1);
    }
}

// ---------------------------------------------------------------------------
// Tween control
// ---------------------------------------------------------------------------
void Tween::setPlaybackState(Host* host, const std::string& s)
{
    PropValue cur;
    if (getProp("PlaybackState", cur) && std::holds_alternative<std::string>(cur) &&
        std::get<std::string>(cur) == s)
        return; // no churn: don't refire Changed every frame
    setProp("PlaybackState", s);
    oboxFirePropChanged(host, this, "PlaybackState");
}

void Tween::play(Host* host)
{
    if (!target || target->isDestroyed())
        return;
    bool resuming = paused && playing && !finished;
    paused = false;
    if (!resuming)
    {
        elapsed = 0.0;
        finished = false;
        startVals.clear();
        for (const auto& kv : goals)
        {
            PropValue v;
            if (target->getProp(kv.first, v))
                startVals[kv.first] = v;
        }
    }
    playing = true;
    auto& at = host->activeTweens;
    if (std::find(at.begin(), at.end(), static_cast<Instance*>(this)) == at.end())
        at.push_back(this);
    setPlaybackState(host, delayTime > 0.0 ? "Delayed" : "Playing");
}

void Tween::cancel(Host* host)
{
    playing = false;
    paused = false;
    finished = true;
    auto& at = host->activeTweens;
    at.erase(std::remove(at.begin(), at.end(), static_cast<Instance*>(this)), at.end());
    setPlaybackState(host, "Cancelled");
}

void Tween::pause(Host* host)
{
    if (!playing || finished)
        return;
    paused = true;
    setPlaybackState(host, "Paused");
}

// ---------------------------------------------------------------------------
// Tween stepping
// ---------------------------------------------------------------------------
namespace
{

void removeActiveTween(Host* host, Tween* t)
{
    auto& at = host->activeTweens;
    at.erase(std::remove(at.begin(), at.end(), static_cast<Instance*>(t)), at.end());
}

// Interpolates start -> goal by e for the tweenable property types.
// Sets ok=false for types that can only snap at the end.
PropValue lerpProp(const std::string& type, const PropValue& start, const PropValue& goal,
                   double e, bool& ok)
{
    ok = true;
    if (type == "number" && std::holds_alternative<double>(start) &&
        std::holds_alternative<double>(goal))
    {
        double a = std::get<double>(start), b = std::get<double>(goal);
        return a + (b - a) * e;
    }
    if (type == "Vector3" && std::holds_alternative<Vec3>(start) &&
        std::holds_alternative<Vec3>(goal))
    {
        Vec3 a = std::get<Vec3>(start), b = std::get<Vec3>(goal);
        return Vec3{a.x + (b.x - a.x) * e, a.y + (b.y - a.y) * e, a.z + (b.z - a.z) * e};
    }
    if (type == "Color3" && std::holds_alternative<Color3>(start) &&
        std::holds_alternative<Color3>(goal))
    {
        Color3 a = std::get<Color3>(start), b = std::get<Color3>(goal);
        return Color3{a.r + (b.r - a.r) * e, a.g + (b.g - a.g) * e, a.b + (b.b - a.b) * e};
    }
    if (type == "CFrame" && std::holds_alternative<CFrameV>(start) &&
        std::holds_alternative<CFrameV>(goal))
    {
        // Prototype approximation: component-wise lerp of the 3x3 rotation
        // plus position lerp (cfLerp). Fine for small rotations.
        return cfLerp(std::get<CFrameV>(start), std::get<CFrameV>(goal), e);
    }
    if (type == "UDim2" && std::holds_alternative<UDim2V>(start) &&
        std::holds_alternative<UDim2V>(goal))
    {
        UDim2V a = std::get<UDim2V>(start), b = std::get<UDim2V>(goal);
        return UDim2V{a.xScale + (b.xScale - a.xScale) * e, a.xOffset + (b.xOffset - a.xOffset) * e,
                      a.yScale + (b.yScale - a.yScale) * e, a.yOffset + (b.yOffset - a.yOffset) * e};
    }
    ok = false;
    return PropValue{};
}

void applyTweenGoals(Host* host, Tween* t, double e)
{
    Instance* target = t->target;
    if (!target || target->isDestroyed())
        return;
    for (const auto& kv : t->goals)
    {
        const std::string& name = kv.first;
        auto dit = target->propDefs().find(name);
        if (dit == target->propDefs().end())
            continue;
        PropValue start;
        auto sit = t->startVals.find(name);
        if (sit != t->startVals.end())
            start = sit->second;
        else if (!target->getProp(name, start))
            continue;
        bool ok = false;
        PropValue v = lerpProp(dit->second.type, start, kv.second, e, ok);
        if (!ok)
        {
            // string, bool, Instance, BrickColor: snap to the goal at the end.
            if (e < 1.0)
                continue;
            v = kv.second;
        }
        if (target->setProp(name, v).empty())
            oboxFirePropChanged(host, target, name);
    }
}

void finishTween(Host* host, Tween* t)
{
    removeActiveTween(host, t);
    t->playing = false;
    t->paused = false;
    t->finished = true;
    t->setPlaybackState(host, "Completed");
    t->completed.fire(*host->sched, host->L, 0);
}

void stepTween(Host* host, Tween* t, double dt)
{
    if (t->finished || t->isDestroyed())
    {
        removeActiveTween(host, t);
        return;
    }
    if (t->target && t->target->isDestroyed())
    {
        t->cancel(host); // animated object is gone: stop quietly
        return;
    }
    if (t->paused || !t->playing)
        return;

    t->elapsed += dt;
    double local = t->elapsed - t->delayTime;
    if (local < 0.0)
    {
        t->setPlaybackState(host, "Delayed");
        return;
    }
    t->setPlaybackState(host, "Playing");

    double dur = t->duration;
    if (dur <= 0.0)
    {
        applyTweenGoals(host, t, 1.0);
        if (t->repeatCount >= 0)
            finishTween(host, t);
        return;
    }
    int cycles = t->repeatCount < 0 ? -1 : t->repeatCount + 1; // -1: infinite
    double cycleDur = dur * (t->reverses ? 2.0 : 1.0);
    if (cycles > 0 && local >= cycles * cycleDur)
    {
        // Resting value after full cycles: the goal, or the start value when
        // the tween ping-pongs back on itself (reverses).
        applyTweenGoals(host, t, t->reverses ? 0.0 : 1.0);
        finishTween(host, t);
        return;
    }
    double tt = std::fmod(local, cycleDur) / dur;
    if (tt > 1.0)
        tt = t->reverses ? 2.0 - tt : 1.0; // ping-pong
    if (tt < 0.0)
        tt = 0.0;
    if (tt > 1.0)
        tt = 1.0;
    applyTweenGoals(host, t, easeValue(t->style, t->direction, tt));
}

} // namespace

// ---------------------------------------------------------------------------
// Per-frame service step
// ---------------------------------------------------------------------------
void oboxServiceStep(Host* host, double dt)
{
    if (Instance* svc = host->game->getService("Players"))
        static_cast<Players*>(svc)->poll(host);

    std::vector<Instance*> active = host->activeTweens; // copy: steps can finish
    for (Instance* i : active)
        stepTween(host, static_cast<Tween*>(i), dt);

    double now = host->sched->now();
    auto& debris = host->debris;
    for (auto it = debris.begin(); it != debris.end();)
    {
        if (it->expireAt <= now)
        {
            it->inst->destroy();
            it = debris.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// ---------------------------------------------------------------------------
// DataStore / DataStoreService
// ---------------------------------------------------------------------------
bool DataStore::get(const std::string& key, StoreVal& out) const
{
    auto it = m_store.find(key);
    if (it == m_store.end())
        return false;
    out = it->second;
    return true;
}

void DataStore::set(const std::string& key, const StoreVal& v)
{
    m_store[key] = v;
}

bool DataStore::remove(const std::string& key)
{
    return m_store.erase(key) > 0;
}

DataStore* DataStoreService::getDataStore(const std::string& name, const std::string& scope,
                                          DataModel* game)
{
    std::string key = name + '\x1f' + scope;
    auto it = m_stores.find(key);
    if (it != m_stores.end())
        return it->second;
    if (!game)
        return nullptr;
    DataStore* ds = static_cast<DataStore*>(game->createInstance("DataStore"));
    if (!ds)
        return nullptr;
    ds->setName(name);
    ds->setParent(this);
    m_stores[key] = ds;
    return ds;
}

// ---------------------------------------------------------------------------
// BindableFunction
// ---------------------------------------------------------------------------
void BindableFunction::setCallback(lua_State* thread, int funcRef)
{
    m_thread = thread;
    m_funcRef = funcRef;
}
