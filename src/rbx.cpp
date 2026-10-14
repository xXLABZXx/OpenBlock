// OpenBlox core DataModel implementation. Clean-room code.
#include "rbx.h"
#include "scheduler.h"
#include "services.h"
#include "physics.h"

#include <lua.h>
#include <lualib.h>
#include <unordered_set>

#include <algorithm>

// ---------------------------------------------------------------------------
// Class hierarchy for IsA (subset of the real Roblox hierarchy).
// ---------------------------------------------------------------------------
static const std::unordered_map<std::string, std::string>& classParents()
{
    static const std::unordered_map<std::string, std::string> parents = {
        {"Part", "BasePart"},
        {"BasePart", "PVInstance"},
        {"PVInstance", "Instance"},
        {"Model", "PVInstance"},
        {"SpawnLocation", "Part"},
        {"Script", "LuaSourceContainer"},
        {"ModuleScript", "LuaSourceContainer"},
        {"LuaSourceContainer", "Instance"},
        {"Workspace", "Instance"},
        {"Players", "Instance"},
        {"RunService", "Instance"},
        {"Lighting", "Instance"},
        {"ReplicatedStorage", "Instance"},
        {"ServerStorage", "Instance"},
        {"ServerScriptService", "Instance"},
        {"StarterPack", "Instance"},
        {"Folder", "Instance"},
        {"Player", "Instance"},
        {"Players", "Instance"},
        {"Lighting", "Instance"},
        {"Teams", "Instance"},
        {"Team", "Instance"},
        {"Camera", "Instance"},
        {"Workspace", "Instance"},
        {"SoundService", "Instance"},
        {"Sound", "Instance"},
        {"TweenService", "Instance"},
        {"Tween", "Instance"},
        {"DataStoreService", "Instance"},
        {"DataStore", "Instance"},
        {"Debris", "Instance"},
        {"BindableEvent", "Instance"},
        {"BindableFunction", "Instance"},
        {"StarterGui", "Instance"},
        {"ReplicatedFirst", "Instance"},
        {"StarterPlayer", "Instance"},
        {"DataModel", "Instance"},
        {"Humanoid", "Instance"},
        {"ValueBase", "Instance"},
        {"IntValue", "ValueBase"},
        {"NumberValue", "ValueBase"},
        {"StringValue", "ValueBase"},
        {"BoolValue", "ValueBase"},
        {"ClickDetector", "Instance"},
    };
    return parents;
}

// ---------------------------------------------------------------------------
// Signal
// ---------------------------------------------------------------------------
int Signal::connect(lua_State* mainL, lua_State* thread, int funcRef, bool once)
{
    if (!m_mainL)
        m_mainL = mainL;
    if (m_handlers.size() >= kMaxHandlers)
        return 0; // handler cap hit; caller raises a catchable error
    uint32_t id = m_nextId++;
    m_handlers.push_back(Handler{thread, funcRef, true, once, id});
    return (int)id;
}

Signal::~Signal()
{
    // Release any registry refs still held (teardown destroys the DataModel
    // before lua_close, so m_mainL is still valid here).
    if (m_mainL)
        for (auto& h : m_handlers)
            if (h.ref != 0)
                lua_unref(m_mainL, h.ref);
}

void Signal::addWaiter(lua_State* co)
{
    m_waiters.push_back(co);
}

void Signal::compact()
{
    m_handlers.erase(std::remove_if(m_handlers.begin(), m_handlers.end(),
                                    [](const Handler& h) { return !h.connected; }),
                     m_handlers.end());
}

void Signal::disconnect(int id)
{
    for (auto& h : m_handlers)
    {
        if (h.id == (uint32_t)id && h.connected)
        {
            if (m_mainL && h.ref != 0)
                lua_unref(m_mainL, h.ref);
            h.ref = 0;
            h.connected = false;
            break;
        }
    }
    // Never compact mid-fire(): the in-flight target list references handlers.
    if (m_firing == 0)
        compact();
}

bool Signal::isConnected(int id) const
{
    for (auto& h : m_handlers)
        if (h.id == (uint32_t)id)
            return h.connected;
    return false;
}

int Signal::connectedCount() const
{
    int n = 0;
    for (auto& h : m_handlers)
        if (h.connected)
            ++n;
    return n;
}

void Signal::fire(Scheduler& sched, lua_State* from, int nargs)
{
    // Reentrancy guard: an event handler that re-triggers the same signal
    // (e.g. a Changed handler that sets a property) would otherwise storm
    // forever. Cap fires per scheduler frame; exceeding it raises a catchable
    // Luau error attributed to the script that caused the storm.
    double now = sched.now();
    if (now != m_lastFireFrame)
    {
        m_lastFireFrame = now;
        m_firesThisFrame = 0;
    }
    if (++m_firesThisFrame > 1000)
    {
        // Reset before raising: fire() is also called from engine C++ code
        // (physics, touchCheck, Heartbeat) outside any protected Lua call,
        // where an exception cannot be caught. The storm's own thread still
        // dies with this catchable error; the reset lets legitimate later
        // fires in the same frame proceed normally.
        m_firesThisFrame = 0;
        luaL_errorL(from, "signal recursion limit exceeded (1000 fires in one frame)");
    }

    // Copy the handler list (with stable ids) so Connect/Disconnect during
    // firing is safe.
    struct Target
    {
        uint32_t id;
        Handler h;
    };
    std::vector<Target> targets;
    for (auto& hh : m_handlers)
        if (hh.connected)
            targets.push_back(Target{hh.id, hh});

    ++m_firing;
    for (auto& t : targets)
    {
        // New thread per handler; move function + args onto it.
        // NB: capture the arg range BEFORE lua_newthread pushes the thread
        // object, or we'd copy the thread itself as an argument.
        int argTop = lua_gettop(from);
        lua_State* co = lua_newthread(from); // pushes thread object on `from`
        lua_getref(co, t.h.ref);             // push the handler function on co
        if (nargs > 0)
        {
            // Copy args from `from`'s stack to co (xmove pops from source).
            // First duplicate them so `from` keeps its copies for the next handler.
            for (int i = argTop - nargs + 1; i <= argTop; ++i)
            {
                lua_pushvalue(from, i);
            }
            lua_xmove(from, co, nargs);
        }
        lua_pop(from, 1); // pop the thread object
        sched.deferThread(co, nargs, 0.0);

        if (t.h.once)
            disconnect(t.id);
    }

    // Resume threads suspended in Wait() with the fired args. Waiters that
    // were cancelled (no longer parked) are skipped; their raw pointers are
    // never dereferenced.
    for (lua_State* w : m_waiters)
    {
        if (!sched.isParked(w))
            continue;
        sched.unpark(w);
        if (nargs > 0)
        {
            int top = lua_gettop(from);
            for (int i = top - nargs + 1; i <= top; ++i)
                lua_pushvalue(from, i);
            lua_xmove(from, w, nargs);
        }
        sched.deferThread(w, nargs, 0.0);
    }
    m_waiters.clear();
    --m_firing;
    compact();
}

Signal* Instance::findSignal(const std::string& name)
{
    if (name == "Changed")
        return &changed;
    if (name == "ChildAdded")
        return &childAdded;
    if (name == "ChildRemoved")
        return &childRemoved;
    return nullptr;
}

Signal* Part::findSignal(const std::string& name)
{
    if (name == "Touched")
        return &touched;
    if (name == "TouchEnded")
        return &touchEnded;
    return Instance::findSignal(name);
}

Signal* RunService::findSignal(const std::string& name)
{
    if (name == "Heartbeat" || name == "Stepped" || name == "RenderStepped")
        return &heartbeat;
    return Instance::findSignal(name);
}

// Same member address on every access: the binding wraps &mouseClick in a
// fresh userdata each time, but all wrappers target this one Signal.
Signal* ClickDetector::findSignal(const std::string& name)
{
    if (name == "MouseClick")
        return &mouseClick;
    return Instance::findSignal(name);
}

// ---------------------------------------------------------------------------
// Instance
// ---------------------------------------------------------------------------
Instance::Instance(std::string className)
    : m_className(std::move(className)), m_name(m_className)
{
    defProp("Name", "string", std::string(m_className));
    defProp("ClassName", "string", std::string(m_className), /*readOnly*/ true);
    defProp("Archivable", "bool", true);
}

void Instance::defProp(const std::string& pname, const std::string& type, PropValue def, bool readOnly)
{
    m_defs[pname] = PropDef{type, readOnly, def};
    m_values[pname] = def;
}

bool Instance::setParent(Instance* p)
{
    if (m_parent == p)
        return true;
    if (p)
    {
        // Reject parenting cycles: p is (transitively) a descendant of this,
        // or this itself. Committing such a link would create a cyclic tree
        // and crash tree walkers.
        for (Instance* q = p; q; q = q->parent())
            if (q == this)
                return false;
    }
    if (m_parent)
    {
        auto& v = m_parent->m_children;
        v.erase(std::remove(v.begin(), v.end(), this), v.end());
    }
    m_parent = p;
    if (m_parent)
        m_parent->m_children.push_back(this);
    return true;
}

Instance* Instance::findFirstChild(const std::string& n, bool recursive) const
{
    for (Instance* c : m_children)
    {
        if (c->m_name == n)
            return c;
        if (recursive)
        {
            if (Instance* f = c->findFirstChild(n, true))
                return f;
        }
    }
    return nullptr;
}

Instance* Instance::findFirstChildOfClass(const std::string& cn) const
{
    for (Instance* c : m_children)
        if (c->m_className == cn)
            return c;
    return nullptr;
}

Instance* Instance::findFirstChildWhichIsA(const std::string& cn, bool recursive) const
{
    for (Instance* c : m_children)
    {
        if (c->isA(cn))
            return c;
        if (recursive)
        {
            if (Instance* f = c->findFirstChildWhichIsA(cn, true))
                return f;
        }
    }
    return nullptr;
}

void Instance::getDescendants(std::vector<Instance*>& out) const
{
    // Visited set: belt-and-braces against cyclic trees (setParent now
    // rejects cycles, but a corrupted tree must not hang the engine).
    std::unordered_set<const Instance*> seen;
    getDescendantsInto(out, seen);
}

void Instance::getDescendantsInto(std::vector<Instance*>& out,
                                  std::unordered_set<const Instance*>& seen) const
{
    for (Instance* c : m_children)
    {
        if (!seen.insert(c).second)
            continue;
        out.push_back(c);
        c->getDescendantsInto(out, seen);
    }
}

void Instance::clearAllChildren()
{
    // Copy: setParent mutates m_children.
    std::vector<Instance*> kids = m_children;
    for (Instance* c : kids)
        c->setParent(nullptr);
}

Signal* Instance::getPropertyChangedSignal(const std::string& prop)
{
    return &m_propSignals[prop]; // created on demand
}

void Instance::setAttribute(const std::string& key, const AttrValue& v)
{
    if (std::holds_alternative<std::monostate>(v))
        m_attributes.erase(key);
    else
        m_attributes[key] = v;
}

bool Instance::getAttribute(const std::string& key, AttrValue& out) const
{
    auto it = m_attributes.find(key);
    if (it == m_attributes.end())
        return false;
    out = it->second;
    return true;
}

void Instance::addTag(const std::string& tag)
{
    if (!hasTag(tag))
        m_tags.push_back(tag);
}

void Instance::removeTag(const std::string& tag)
{
    m_tags.erase(std::remove(m_tags.begin(), m_tags.end(), tag), m_tags.end());
}

bool Instance::hasTag(const std::string& tag) const
{
    return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

std::vector<std::string> Instance::getTags() const
{
    return m_tags;
}

std::string Instance::getFullName() const
{
    // Depth cap: belt-and-braces against cyclic ancestry (setParent now
    // rejects cycles, but a corrupted tree must not recurse forever).
    const int kMaxDepth = 100000;
    std::string name = m_name;
    const Instance* q = m_parent;
    int depth = 0;
    while (q && depth < kMaxDepth)
    {
        name = q->m_name + "." + name;
        q = q->m_parent;
        ++depth;
    }
    return name;
}

bool Instance::isA(const std::string& cn) const
{
    std::string cur = m_className;
    const auto& parents = classParents();
    while (true)
    {
        if (cur == cn)
            return true;
        auto it = parents.find(cur);
        if (it == parents.end())
            return false;
        cur = it->second;
    }
}

void Instance::destroy()
{
    // Cascade to descendants first (copy: destroy() mutates m_children via
    // setParent). Children end up unparented and marked destroyed instead of
    // keeping a stale m_parent.
    std::vector<Instance*> kids = m_children;
    for (Instance* c : kids)
        c->destroy();
    setParent(nullptr);
    m_destroyed = true;
}

bool Instance::getProp(const std::string& key, PropValue& out) const
{
    auto it = m_values.find(key);
    if (it == m_values.end())
        return false;
    out = it->second;
    return true;
}

std::string Instance::setProp(const std::string& key, const PropValue& v)
{
    auto it = m_defs.find(key);
    if (it == m_defs.end())
        return key + " is not a valid member of " + m_className + " \"" + m_name + "\"";
    const PropDef& def = it->second;
    if (def.readOnly)
        return "The " + key + " property of " + m_className + " is read-only";
    if (v.index() != def.def.index())
        return "Type mismatch setting " + key + " (expected " + def.type + ")";
    m_values[key] = v;
    if (key == "Name")
        m_name = std::get<std::string>(v);
    return "";
}

// ---------------------------------------------------------------------------
// Concrete classes
// ---------------------------------------------------------------------------
Part::Part() : Part("Part")
{
}

Part::Part(std::string className) : Instance(std::move(className))
{
    defProp("Position", "Vector3", Vec3{0, 0, 0});
    defProp("Size", "Vector3", Vec3{4, 1, 2});
    defProp("Anchored", "bool", false);
    defProp("CanCollide", "bool", true);
    defProp("Transparency", "number", 0.0);
    defProp("Color", "Color3", Color3{0.639, 0.635, 0.647}); // Medium stone grey
    defProp("CFrame", "CFrame", CFrameV{});
    defProp("Velocity", "Vector3", Vec3{0, 0, 0});
    defProp("Material", "string", std::string("Plastic"));
    defProp("Shape", "string", std::string("Block"));
}

SpawnLocation::SpawnLocation() : Part("SpawnLocation")
{
}

Model::Model() : Instance("Model")
{
}

Folder::Folder() : Instance("Folder")
{
}

ValueBase::ValueBase(std::string className) : Instance(std::move(className))
{
}

IntValue::IntValue() : ValueBase("IntValue")
{
    defProp("Value", "number", 0.0);
}

NumberValue::NumberValue() : ValueBase("NumberValue")
{
    defProp("Value", "number", 0.0);
}

StringValue::StringValue() : ValueBase("StringValue")
{
    defProp("Value", "string", std::string(""));
}

BoolValue::BoolValue() : ValueBase("BoolValue")
{
    defProp("Value", "bool", false);
}

ClickDetector::ClickDetector() : Instance("ClickDetector")
{
}

LuaSourceContainer::LuaSourceContainer(const std::string& className) : Instance(className)
{
    defProp("Source", "string", std::string(""));
    defProp("Disabled", "bool", false);
}

std::string LuaSourceContainer::setProp(const std::string& key, const PropValue& v)
{
    std::string err = Instance::setProp(key, v);
    if (err.empty() && key == "Source")
        m_source = std::get<std::string>(v);
    return err;
}

// Keeps Part's CFrame and Position properties in sync: the CFrame property
// carries the full rigid transform while Position is its translation. defProp
// writes m_values directly, so there is no recursion here.
std::string Part::setProp(const std::string& key, const PropValue& v)
{
    std::string err = Instance::setProp(key, v);
    if (!err.empty())
        return err;
    if (key == "CFrame")
    {
        const CFrameV& c = std::get<CFrameV>(v);
        Instance::setProp("Position", Vec3{c.m[9], c.m[10], c.m[11]});
    }
    else if (key == "Position")
    {
        PropValue cur;
        if (getProp("CFrame", cur))
        {
            CFrameV c = std::get<CFrameV>(cur);
            const Vec3& p = std::get<Vec3>(v);
            c.m[9] = p.x;
            c.m[10] = p.y;
            c.m[11] = p.z;
            Instance::setProp("CFrame", c);
        }
    }
    return "";
}

Script::Script() : LuaSourceContainer("Script")
{
}

ModuleScript::ModuleScript() : LuaSourceContainer("ModuleScript")
{
}

ServiceInstance::ServiceInstance(const std::string& className) : Instance(className)
{
}

RunService::RunService() : ServiceInstance("RunService")
{
}

// ---------------------------------------------------------------------------
// DataModel
// ---------------------------------------------------------------------------
DataModel::DataModel() : Instance("DataModel")
{
    setProp("Name", std::string("game"));
    // Well-known services, created up front like Roblox does.
    const char* services[] = {
        "Workspace", "Players", "Lighting", "ReplicatedStorage",
        "ServerStorage", "ServerScriptService", "StarterPack",
        "StarterGui", "ReplicatedFirst", "StarterPlayer", "Teams",
        "SoundService", "TweenService", "DataStoreService", "Debris",
    };
    // Services with dedicated C++ classes get those; the rest are plain
    // ServiceInstance containers. Players, Lighting, Teams, SoundService,
    // TweenService, DataStoreService and Debris carry engine state, so they
    // must be their real classes for polling/tweening/datastores to work.
    auto makeService = [](const std::string& s) -> std::unique_ptr<ServiceInstance> {
        if (s == "Workspace")
            return std::make_unique<Workspace>();
        if (s == "Players")
            return std::make_unique<Players>();
        if (s == "Lighting")
            return std::make_unique<Lighting>();
        if (s == "Teams")
            return std::make_unique<Teams>();
        if (s == "SoundService")
            return std::make_unique<SoundService>();
        if (s == "TweenService")
            return std::make_unique<TweenService>();
        if (s == "DataStoreService")
            return std::make_unique<DataStoreService>();
        if (s == "Debris")
            return std::make_unique<Debris>();
        return std::make_unique<ServiceInstance>(s);
    };
    for (const char* s : services)
    {
        auto svc = makeService(s);
        svc->setName(s);
        svc->setParent(this);
        m_services[s] = svc.get();
        m_owned.push_back(std::move(svc));
    }
    auto rs = std::make_unique<RunService>();
    rs->setName("RunService");
    rs->setParent(this);
    m_services["RunService"] = rs.get();
    m_owned.push_back(std::move(rs));

    // Default camera parented under Workspace, wired as CurrentCamera.
    // (The "Camera" factory entry is registered separately.)
    if (Instance* cam = createInstance("Camera"))
    {
        cam->setName("Camera");
        cam->setParent(m_services["Workspace"]);
        PropValue cv = static_cast<Instance*>(cam);
        m_services["Workspace"]->setProp("CurrentCamera", cv);
    }
}

Instance* DataModel::createInstance(const std::string& className)
{
    if (m_owned.size() >= kMaxInstances)
        return nullptr; // instance cap: see overInstanceCap()
    std::unique_ptr<Instance> inst;
    if (className == "Part")
        inst = std::make_unique<Part>();
    else if (className == "SpawnLocation")
        inst = std::make_unique<SpawnLocation>();
    else if (className == "Model")
        inst = std::make_unique<Model>();
    else if (className == "Folder")
        inst = std::make_unique<Folder>();
    else if (className == "Script")
        inst = std::make_unique<Script>();
    else if (className == "ModuleScript")
        inst = std::make_unique<ModuleScript>();
    else if (className == "Humanoid")
        inst = std::make_unique<Humanoid>();
    else if (className == "Player")
        inst = std::make_unique<Player>();
    else if (className == "Team")
        inst = std::make_unique<Team>();
    else if (className == "Camera")
        inst = std::make_unique<Camera>();
    else if (className == "Sound")
        inst = std::make_unique<Sound>();
    else if (className == "Tween")
        inst = std::make_unique<Tween>();
    else if (className == "BindableEvent")
        inst = std::make_unique<BindableEvent>();
    else if (className == "BindableFunction")
        inst = std::make_unique<BindableFunction>();
    else if (className == "DataStore")
        inst = std::make_unique<DataStore>();
    else if (className == "IntValue")
        inst = std::make_unique<IntValue>();
    else if (className == "NumberValue")
        inst = std::make_unique<NumberValue>();
    else if (className == "StringValue")
        inst = std::make_unique<StringValue>();
    else if (className == "BoolValue")
        inst = std::make_unique<BoolValue>();
    else if (className == "ClickDetector")
        inst = std::make_unique<ClickDetector>();
    else
        return nullptr;

    Instance* ptr = inst.get();
    m_owned.push_back(std::move(inst));
    return ptr;
}

Instance* DataModel::getService(const std::string& name) const
{
    auto it = m_services.find(name);
    return it == m_services.end() ? nullptr : it->second;
}
