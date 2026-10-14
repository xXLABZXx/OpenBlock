// OpenBlox core DataModel implementation. Clean-room code.
#include "rbx.h"
#include "scheduler.h"

#include <lua.h>

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
        {"DataModel", "Instance"},
    };
    return parents;
}

// ---------------------------------------------------------------------------
// Signal
// ---------------------------------------------------------------------------
int Signal::connect(lua_State* thread, int funcRef, bool once)
{
    m_handlers.push_back(Handler{thread, funcRef, true, once});
    return (int)m_handlers.size(); // 1-based id
}

void Signal::addWaiter(lua_State* co)
{
    m_waiters.push_back(co);
}

void Signal::disconnect(int id)
{
    if (id >= 1 && id <= (int)m_handlers.size())
        m_handlers[id - 1].connected = false;
}

bool Signal::isConnected(int id) const
{
    return id >= 1 && id <= (int)m_handlers.size() && m_handlers[id - 1].connected;
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
    // Copy the handler list (with original indexes) so Connect/Disconnect
    // during firing is safe.
    struct Target
    {
        size_t index;
        Handler h;
    };
    std::vector<Target> targets;
    for (size_t i = 0; i < m_handlers.size(); ++i)
        if (m_handlers[i].connected)
            targets.push_back(Target{i, m_handlers[i]});

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
            disconnect((int)t.index + 1); // 1-based id
    }

    // Resume threads suspended in Wait() with the fired args.
    for (lua_State* w : m_waiters)
    {
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

void Instance::setParent(Instance* p)
{
    if (m_parent == p)
        return;
    if (m_parent)
    {
        auto& v = m_parent->m_children;
        v.erase(std::remove(v.begin(), v.end(), this), v.end());
    }
    m_parent = p;
    if (m_parent)
        m_parent->m_children.push_back(this);
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
    for (Instance* c : m_children)
    {
        out.push_back(c);
        c->getDescendants(out);
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
    if (m_parent)
        return m_parent->getFullName() + "." + m_name;
    return m_name;
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
Part::Part() : Instance("Part")
{
    defProp("Position", "Vector3", Vec3{0, 0, 0});
    defProp("Size", "Vector3", Vec3{4, 1, 2});
    defProp("Anchored", "bool", false);
    defProp("CanCollide", "bool", true);
    defProp("Transparency", "number", 0.0);
    defProp("Color", "Color3", Color3{0.639, 0.635, 0.647}); // Medium stone grey
}

Model::Model() : Instance("Model")
{
}

Folder::Folder() : Instance("Folder")
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
    };
    for (const char* s : services)
    {
        auto svc = std::make_unique<ServiceInstance>(s);
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
}

Instance* DataModel::createInstance(const std::string& className)
{
    std::unique_ptr<Instance> inst;
    if (className == "Part" || className == "SpawnLocation")
        inst = std::make_unique<Part>();
    else if (className == "Model")
        inst = std::make_unique<Model>();
    else if (className == "Folder")
        inst = std::make_unique<Folder>();
    else if (className == "Script")
        inst = std::make_unique<Script>();
    else if (className == "ModuleScript")
        inst = std::make_unique<ModuleScript>();
    else
        return nullptr;

    if (className == "SpawnLocation")
    {
        // Keep the requested class name on the Part subclass.
        // (Simplest honest stand-in until dedicated classes exist.)
    }
    Instance* ptr = inst.get();
    m_owned.push_back(std::move(inst));
    return ptr;
}

Instance* DataModel::getService(const std::string& name) const
{
    auto it = m_services.find(name);
    return it == m_services.end() ? nullptr : it->second;
}
