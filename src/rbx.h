// OpenBlox core DataModel: Instance hierarchy, properties, signals.
// Clean-room implementation. No Roblox code or assets are used.
#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct lua_State; // Luau thread/state (forward declared; lua.h included in .cpp)
class Scheduler; // forward declared; scheduler.h included in .cpp

struct Vec3
{
    double x = 0.0, y = 0.0, z = 0.0;
};

struct Color3
{
    double r = 0.0, g = 0.0, b = 0.0;
};

class Instance; // forward

// Property values supported by the prototype property system.
using PropValue = std::variant<std::monostate, double, std::string, bool, Vec3, Color3, Instance*>;

// Attribute values (Roblox attributes support a subset of types).
using AttrValue = std::variant<std::monostate, double, std::string, bool, Vec3, Color3>;

// ---------------------------------------------------------------------------
// Signal: a Roblox-style RBXScriptSignal. Handlers are Luau functions owned by
// the scheduler; firing is always deferred (never reentrant), matching
// Roblox semantics where event handlers run on the task scheduler.
// ---------------------------------------------------------------------------
class Signal
{
public:
    struct Handler
    {
        lua_State* thread = nullptr; // Luau thread that owns the function
        int ref = 0;                 // registry ref to the Luau function
        bool connected = true;
        bool once = false;
    };

    // Connects a function; returns a 1-based connection id.
    int connect(lua_State* thread, int funcRef, bool once = false);
    void disconnect(int id);
    bool isConnected(int id) const;
    int connectedCount() const;

    // Suspends `co` until the next fire(); the fired args become Wait()'s results.
    void addWaiter(lua_State* co);

    // Enqueues one deferred scheduler call per connected handler and resumes
    // waiters. The event arguments must already be pushed on top of `from`'s stack.
    void fire(Scheduler& sched, lua_State* from, int nargs);

private:
    std::vector<Handler> m_handlers;
    std::vector<lua_State*> m_waiters;
};

// ---------------------------------------------------------------------------
// Instance: the base of the DataModel object hierarchy.
// ---------------------------------------------------------------------------
class Instance
{
public:
    explicit Instance(std::string className);
    virtual ~Instance() = default;

    const std::string& className() const { return m_className; }
    const std::string& name() const { return m_name; }
    void setName(const std::string& n)
    {
        m_name = n;
        m_values["Name"] = n;
    }

    Instance* parent() const { return m_parent; }
    void setParent(Instance* p); // links/unlinks; does not transfer ownership
    const std::vector<Instance*>& children() const { return m_children; }
    Instance* findFirstChild(const std::string& n, bool recursive = false) const;
    Instance* findFirstChildOfClass(const std::string& cn) const;
    Instance* findFirstChildWhichIsA(const std::string& cn, bool recursive = false) const;
    void getDescendants(std::vector<Instance*>& out) const;
    void clearAllChildren();
    std::string getFullName() const;

    bool isA(const std::string& cn) const;
    bool isDestroyed() const { return m_destroyed; }
    void destroy(); // unlinks from parent, marks destroyed (memory kept by DataModel)

    struct PropDef
    {
        std::string type; // "string", "number", "bool", "Vector3", "Instance"
        bool readOnly = false;
        PropValue def = std::monostate{};
    };

    const std::unordered_map<std::string, PropDef>& propDefs() const { return m_defs; }
    bool getProp(const std::string& key, PropValue& out) const;
    // Returns "" on success, or an error message. Fires nothing; the binding
    // layer fires Changed so the core stays scheduler-free.
    virtual std::string setProp(const std::string& key, const PropValue& v);

    Signal changed;      // fires with the property name
    Signal childAdded;   // fires with the child Instance
    Signal childRemoved; // fires with the child Instance

    // Per-property change signals (GetPropertyChangedSignal).
    Signal* getPropertyChangedSignal(const std::string& prop);

    // Attributes (untyped-ish key/value store, Roblox-compatible subset).
    void setAttribute(const std::string& key, const AttrValue& v);
    bool getAttribute(const std::string& key, AttrValue& out) const;
    const std::unordered_map<std::string, AttrValue>& attributes() const { return m_attributes; }

    // Tags.
    void addTag(const std::string& tag);
    void removeTag(const std::string& tag);
    bool hasTag(const std::string& tag) const;
    std::vector<std::string> getTags() const;

    // Named-signal lookup for the binding layer (subclasses extend this).
    virtual Signal* findSignal(const std::string& name);

protected:
    void defProp(const std::string& pname, const std::string& type, PropValue def, bool readOnly = false);

private:
    std::string m_className;
    std::string m_name;
    Instance* m_parent = nullptr;
    std::vector<Instance*> m_children;
    std::unordered_map<std::string, PropDef> m_defs;
    std::unordered_map<std::string, PropValue> m_values;
    std::unordered_map<std::string, Signal> m_propSignals;
    std::unordered_map<std::string, AttrValue> m_attributes;
    std::vector<std::string> m_tags;
    bool m_destroyed = false;
};

// ---------------------------------------------------------------------------
// Concrete classes used by the prototype.
// ---------------------------------------------------------------------------
class Part : public Instance
{
public:
    Part();
    Signal touched;    // fires with the other Part on new AABB overlap
    Signal touchEnded; // fires with the other Part when overlap ends

    Signal* findSignal(const std::string& name) override;
};

class Model : public Instance
{
public:
    Model();
};

class Folder : public Instance
{
public:
    Folder();
};

// Base for Script / ModuleScript: carries Luau source text.
class LuaSourceContainer : public Instance
{
public:
    explicit LuaSourceContainer(const std::string& className);
    const std::string& source() const { return m_source; }
    void setSource(const std::string& s)
    {
        m_source = s;
        Instance::setProp("Source", s);
    }
    std::string setProp(const std::string& key, const PropValue& v) override;

private:
    std::string m_source;
};

class Script : public LuaSourceContainer
{
public:
    Script();
};

class ModuleScript : public LuaSourceContainer
{
public:
    ModuleScript();
};

// Services (plain containers with well-known class names).
class ServiceInstance : public Instance
{
public:
    explicit ServiceInstance(const std::string& className);
};

class RunService : public ServiceInstance
{
public:
    RunService();
    Signal heartbeat; // fires with dt each frame

    Signal* findSignal(const std::string& name) override;
};

// ---------------------------------------------------------------------------
// DataModel: the root `game` object. Owns every Instance ever created so Luau
// userdata can never dangle.
// ---------------------------------------------------------------------------
class DataModel : public Instance
{
public:
    DataModel();
    ~DataModel() override = default;

    // Creates an instance of a known class; owned by the DataModel for life.
    // Returns nullptr for unknown classes.
    Instance* createInstance(const std::string& className);

    Instance* getService(const std::string& name) const;

private:
    std::vector<std::unique_ptr<Instance>> m_owned;
    std::unordered_map<std::string, Instance*> m_services;
};
