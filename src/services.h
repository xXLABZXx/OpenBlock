// OpenBlock services module: Players, Lighting, Teams, Camera, Workspace,
// SoundService, TweenService, DataStoreService, Debris, BindableEvent and
// BindableFunction for the OpenBlock open-source voxel game engine.
// Clean-room implementation; no third-party engine code is used.
#pragma once

#include <map>
#include <set>
#include <string>
#include <variant>

#include "rbx.h"

struct Host; // complete type in bindings.h
struct lua_State;

// ---------------------------------------------------------------------------
// Player / Players
// ---------------------------------------------------------------------------
class Player : public Instance
{
public:
    Player();
};

class Players : public ServiceInstance
{
public:
    Players();

    Signal playerAdded;
    Signal playerRemoving;

    Signal* findSignal(const std::string& name) override;

    // Compares current Player children against the last-seen set and fires
    // PlayerAdded / PlayerRemoving for the difference. Called once per frame
    // by oboxServiceStep.
    void poll(Host* host);

private:
    std::set<Instance*> m_known;
};

// ---------------------------------------------------------------------------
// Lighting
// ---------------------------------------------------------------------------
class Lighting : public ServiceInstance
{
public:
    Lighting();
};

// ---------------------------------------------------------------------------
// Teams / Team
// ---------------------------------------------------------------------------
class Team : public Instance
{
public:
    Team();
};

class Teams : public ServiceInstance
{
public:
    Teams();
};

// ---------------------------------------------------------------------------
// Camera / Workspace
// ---------------------------------------------------------------------------
class Camera : public Instance
{
public:
    Camera();
};

class Workspace : public ServiceInstance
{
public:
    Workspace();
};

// ---------------------------------------------------------------------------
// SoundService / Sound
// ---------------------------------------------------------------------------
class SoundService : public ServiceInstance
{
public:
    SoundService();
};

class Sound : public Instance
{
public:
    Sound();
};

// ---------------------------------------------------------------------------
// TweenService / Tween
// ---------------------------------------------------------------------------
class TweenService : public ServiceInstance
{
public:
    TweenService();
};

class Tween : public Instance
{
public:
    Tween();

    Signal completed;
    Signal* findSignal(const std::string& name) override;

    // Tween state (filled by TweenService:Create, driven by oboxServiceStep).
    Instance* target = nullptr;
    double duration = 1.0;
    double delayTime = 0.0;
    double elapsed = 0.0;
    int style = 1;      // easing style id (see easeValue in mathx.h)
    int direction = 1;  // easing direction id
    int repeatCount = 0; // -1 = repeat forever
    bool reverses = false;
    bool playing = false;
    bool paused = false;
    bool finished = false;
    std::map<std::string, PropValue> goals;
    std::map<std::string, PropValue> startVals;

    // Captures start values, marks playing and enqueues on host->activeTweens
    // (no duplicates). Calling Play on a paused tween resumes it from the
    // current position, keeping the original start values.
    void play(Host* host);
    // Removes from host->activeTweens; PlaybackState becomes "Cancelled".
    void cancel(Host* host);
    // Freezes progress; PlaybackState becomes "Paused".
    void pause(Host* host);

    // Updates the PlaybackState property (firing Changed), skipping the
    // write when the value is unchanged.
    void setPlaybackState(Host* host, const std::string& s);
};

// ---------------------------------------------------------------------------
// DataStoreService / DataStore
//
// Prototype DataStores are synchronous and in-memory (local-first): there is
// no network backend, so GetAsync/SetAsync/etc. apply immediately and values
// live only for the process lifetime. Supported value types are number,
// string and boolean.
// ---------------------------------------------------------------------------
using StoreVal = std::variant<double, std::string, bool>;

class DataStore : public Instance
{
public:
    DataStore();

    bool get(const std::string& key, StoreVal& out) const;
    void set(const std::string& key, const StoreVal& v);
    bool remove(const std::string& key); // true when a value existed

private:
    std::map<std::string, StoreVal> m_store;
};

class DataStoreService : public ServiceInstance
{
public:
    DataStoreService();

    // Returns the cached store for (name, scope), creating it through the
    // DataModel on first use. Returns nullptr when the DataStore class is
    // not registered with the DataModel factory.
    DataStore* getDataStore(const std::string& name, const std::string& scope, DataModel* game);

private:
    std::map<std::string, DataStore*> m_stores; // owned by the DataModel
};

// ---------------------------------------------------------------------------
// Debris
// ---------------------------------------------------------------------------
class Debris : public ServiceInstance
{
public:
    Debris();
};

// ---------------------------------------------------------------------------
// BindableEvent / BindableFunction
// ---------------------------------------------------------------------------
class BindableEvent : public Instance
{
public:
    BindableEvent();

    Signal event; // exposed to Luau as the "Event" signal
    Signal* findSignal(const std::string& name) override;
};

class BindableFunction : public Instance
{
public:
    BindableFunction();

    bool hasCallback() const { return m_funcRef != 0; }
    // Stores the owning thread + registry ref of the Luau function (0 clears).
    void setCallback(lua_State* thread, int funcRef);
    lua_State* callbackThread() const { return m_thread; }
    int callbackRef() const { return m_funcRef; }

private:
    lua_State* m_thread = nullptr;
    int m_funcRef = 0; // 0 == LUA_REFNIL == no callback
};

// ---------------------------------------------------------------------------
// TweenInfo userdata (created by bindings_services.cpp).
// ---------------------------------------------------------------------------
struct UTweenInfo
{
    double time = 1.0;
    int style = 1;      // easing style id, 1 = Sine
    int direction = 1;  // easing direction id, 1 = Out
    int repeatCount = 0;
    bool reverses = false;
    double delayTime = 0.0;
};
UTweenInfo* oboxCheckTweenInfo(lua_State* L, int idx);

// ---------------------------------------------------------------------------
// Module entry points (implemented in services.cpp / bindings_services.cpp).
// ---------------------------------------------------------------------------

// Registers the TweenInfo global on L.
void oboxRegisterServices(lua_State* L);

// __index dispatch for service methods. Pushes the method closure and returns
// true when `key` is a method of `inst`'s class; returns false otherwise.
bool oboxServicesIndex(lua_State* L, Instance* inst, const char* key);

// Advances Players polling, active tweens and Debris expiry. Called once per
// frame from the main loop.
void oboxServiceStep(Host* host, double dt);

// Fires Changed (with the property name) plus the per-property signal for a
// property assigned from C++, mirroring instanceNewIndex in bindings.cpp.
void oboxFirePropChanged(Host* host, Instance* inst, const std::string& name);
