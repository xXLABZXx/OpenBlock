// OpenBlox host entry point. Clean-room code.
//
// Creates a Luau state with a capped allocator and an execution budget,
// opens a safe subset of the standard libraries, sandboxes the global
// environment, registers the OpenBlox API, runs scripts on isolated threads,
// and steps a fixed-timestep Heartbeat loop. --tests runs the compatibility
// suite and regenerates docs/COMPATIBILITY.md.
#include "bindings.h"

#include <lua.h>
#include <lualib.h>
#include <luacode.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Execution budget + memory cap (sandboxing primitives)
// ---------------------------------------------------------------------------
double g_oboxDeadline = 1e30;

double oboxWallNow()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// Called by the VM at safepoints (loop back-edges, calls). Raising here aborts
// runaway scripts at the offending line. (Same pattern as Luau's own REPL.)
static void oboxInterrupt(lua_State* L, int gc)
{
    if (gc >= 0)
        return; // never interrupt the garbage collector
    if (oboxWallNow() > g_oboxDeadline)
        luaL_error(L, "OpenBlox: script exceeded its execution budget (infinite loop?)");
}

struct AllocCap
{
    size_t used = 0;
    size_t limit = 256u * 1024u * 1024u; // 256 MiB for the whole VM
};

static void* cappedAlloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    AllocCap* cap = (AllocCap*)ud;
    if (nsize == 0)
    {
        cap->used -= osize;
        free(ptr);
        return nullptr;
    }
    if (nsize > osize && cap->used + (nsize - osize) > cap->limit)
        return nullptr; // Luau raises a memory error
    void* nptr = realloc(ptr, nsize);
    if (nptr)
        cap->used += nsize - osize;
    return nptr;
}

// ---------------------------------------------------------------------------
// Safe standard library subset
// ---------------------------------------------------------------------------
static void openLib(lua_State* L, const char* name, lua_CFunction fn)
{
    lua_pushcfunction(L, fn, name);
    lua_pushstring(L, name);
    lua_call(L, 1, 0);
}

static void nilGlobal(lua_State* L, const char* name)
{
    lua_pushnil(L);
    lua_setglobal(L, name);
}

static void nilField(lua_State* L, const char* lib, const char* field)
{
    lua_getglobal(L, lib);
    if (lua_istable(L, -1))
    {
        lua_pushnil(L);
        lua_setfield(L, -2, field);
    }
    lua_pop(L, 1);
}

static void openSafeLibs(lua_State* L)
{
    // Deliberately NOT opened: debug, io (not in Luau), and the Require
    // module (OpenBlox provides its own require()).
    openLib(L, "", luaopen_base);
    openLib(L, LUA_COLIBNAME, luaopen_coroutine);
    openLib(L, LUA_TABLIBNAME, luaopen_table);
    openLib(L, LUA_STRLIBNAME, luaopen_string);
    openLib(L, LUA_MATHLIBNAME, luaopen_math);
    openLib(L, LUA_UTF8LIBNAME, luaopen_utf8);
    openLib(L, LUA_BITLIBNAME, luaopen_bit32);
    openLib(L, LUA_BUFFERLIBNAME, luaopen_buffer);
    openLib(L, LUA_VECLIBNAME, luaopen_vector);

    // Base library: remove file/code loading primitives.
    nilGlobal(L, "dofile");
    nilGlobal(L, "loadfile");
    nilGlobal(L, "loadstring"); // may not exist; harmless either way

    // Keep os.time/clock/date/difftime (Roblox exposes these); remove the rest.
    openLib(L, LUA_OSLIBNAME, luaopen_os);
    const char* dangerous[] = {"execute", "exit", "getenv", "remove", "rename", "setlocale", "tmpname", nullptr};
    for (int i = 0; dangerous[i]; ++i)
        nilField(L, "os", dangerous[i]);
}

// ---------------------------------------------------------------------------
// Host setup
// ---------------------------------------------------------------------------
struct HostOwner
{
    Host host;
    AllocCap cap;
};

static Host* setupHost(AllocCap& cap)
{
    Host* host = new Host();
    host->L = lua_newstate(cappedAlloc, &cap);
    lua_State* L = host->L;
    openSafeLibs(L);
    host->game = new DataModel();
    host->sched = new Scheduler(L);
    oboxRegisterApi(L, host);
    return host;
}

static void lockDown(Host* host)
{
    luaL_sandbox(host->L); // freeze globals + library tables (safeenv)
    lua_callbacks(host->L)->interrupt = oboxInterrupt;
}

static void teardownHost(Host* host)
{
    lua_close(host->L);
    delete host->sched;
    delete host->game;
    delete host;
}

// ---------------------------------------------------------------------------
// Compatibility harness (test mode)
// ---------------------------------------------------------------------------
struct CompatResult
{
    std::string name;
    std::string status; // working | partial | unsupported | failed
    std::string note;
};

static std::vector<CompatResult>* getCompat(lua_State* L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "OB.Compat");
    auto* v = static_cast<std::vector<CompatResult>*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return v;
}

static int l_compat_pass(lua_State* L)
{
    size_t len = 0;
    const char* name = luaL_checklstring(L, 1, &len);
    auto* out = getCompat(L);
    std::string n(name, len);
    for (auto& r : *out)
    {
        if (r.name == n && r.status == "pending")
        {
            r.status = "working";
            r.note.clear();
            break;
        }
    }
    return 0;
}

static int l_compat_check(lua_State* L)
{
    size_t len = 0;
    const char* name = luaL_checklstring(L, 1, &len);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    Host* h = oboxGetHost(L);
    auto* out = getCompat(L);

    // Record as pending; the async test flips it to "working" via compat.pass.
    // If the test errors, the scheduler prints it and the record stays pending
    // (marked "failed" after the run).
    out->push_back({std::string(name, len), "pending", ""});

    // Run the test on its own scheduler thread via a Lua wrapper. The wrapper
    // must be Lua (not C) so task.wait() yields propagate through it; a C
    // wrapper would hit "yield across C-call boundary".
    lua_getfield(L, LUA_REGISTRYINDEX, "OB.CheckWrapper"); // [name, fn, wrapper]
    lua_State* co = lua_newthread(L);                      // [..., thread]
    luaL_sandboxthread(co);
    lua_pushvalue(L, -2); // wrapper
    lua_pushvalue(L, 2);  // fn
    lua_pushlstring(L, name, len);
    lua_xmove(L, co, 3);  // co: [wrapper, fn, name]
    lua_pop(L, 2);        // drop wrapper copy + thread object
    h->sched->deferThread(co, 2, 0.0);
    return 0;
}

static int l_compat_unsupported(lua_State* L)
{
    size_t len = 0;
    const char* name = luaL_checklstring(L, 1, &len);
    size_t nlen = 0;
    const char* note = luaL_optlstring(L, 2, "", &nlen);
    getCompat(L)->push_back({std::string(name, len), "unsupported", std::string(note, nlen)});
    return 0;
}

static int l_compat_partial(lua_State* L)
{
    size_t len = 0;
    const char* name = luaL_checklstring(L, 1, &len);
    size_t nlen = 0;
    const char* note = luaL_optlstring(L, 2, "", &nlen);
    getCompat(L)->push_back({std::string(name, len), "partial", std::string(note, nlen)});
    return 0;
}

static void registerCompat(Host* host, std::vector<CompatResult>* out)
{
    lua_State* L = host->L;
    lua_pushlightuserdata(L, out);
    lua_setfield(L, LUA_REGISTRYINDEX, "OB.Compat");
    lua_newtable(L);
    lua_pushcfunction(L, l_compat_check, "check");
    lua_setfield(L, -2, "check");
    lua_pushcfunction(L, l_compat_pass, "pass");
    lua_setfield(L, -2, "pass");
    lua_pushcfunction(L, l_compat_unsupported, "unsupported");
    lua_setfield(L, -2, "unsupported");
    lua_pushcfunction(L, l_compat_partial, "partial");
    lua_setfield(L, -2, "partial");
    lua_setglobal(L, "compat");

    // Pre-compile the async check wrapper: function(fn, name) fn() compat.pass(name) end
    const char* src = "return function(fn, name) fn() compat.pass(name) end";
    size_t bcSize = 0;
    char* bc = luau_compile(src, std::strlen(src), nullptr, &bcSize);
    if (bc)
    {
        if (luau_load(L, "@compat_wrapper", bc, bcSize, 0) == LUA_OK)
        {
            lua_call(L, 0, 1); // run the chunk -> wrapper function
            lua_setfield(L, LUA_REGISTRYINDEX, "OB.CheckWrapper");
        }
        free(bc);
    }
}

static const char* statusIcon(const std::string& s)
{
    if (s == "working")
        return "✅";
    if (s == "partial")
        return "🟡";
    if (s == "unsupported")
        return "❌";
    return "🔴";
}

static void writeCompatMd(const std::string& path, const std::vector<CompatResult>& results)
{
    auto rank = [](const std::string& s) {
        if (s == "failed")
            return 3;
        if (s == "unsupported")
            return 2;
        if (s == "partial")
            return 1;
        return 0;
    };
    std::vector<CompatResult> uniq;
    for (auto& r : results)
    {
        auto it = std::find_if(uniq.begin(), uniq.end(),
                               [&](const CompatResult& u) { return u.name == r.name; });
        if (it == uniq.end())
            uniq.push_back(r);
        else if (rank(r.status) > rank(it->status))
            *it = r;
    }
    std::sort(uniq.begin(), uniq.end(),
              [](const CompatResult& a, const CompatResult& b) { return a.name < b.name; });

    std::time_t t = std::time(nullptr);
    char datebuf[64];
    std::strftime(datebuf, sizeof(datebuf), "%Y-%m-%d", std::localtime(&t));

    std::ofstream f(path);
    f << "# OpenBlox compatibility matrix\n\n";
    f << "> Generated " << datebuf << " by `openblox --tests`. "
      << "Machine-checked: every row was exercised by the test suite. "
      << "Never hand-edit this file.\n\n";
    f << "| API member | Status | Note |\n";
    f << "|---|---|---|\n";
    std::string group;
    int counts[4] = {0, 0, 0, 0};
    for (auto& r : uniq)
    {
        std::string g = r.name.substr(0, r.name.find('.'));
        if (g != group)
        {
            group = g;
            f << "| **" << g << "** |||\n";
        }
        f << "| " << r.name << " | " << statusIcon(r.status) << " " << r.status << " | "
          << r.note << " |\n";
        if (r.status == "working")
            ++counts[0];
        else if (r.status == "partial")
            ++counts[1];
        else if (r.status == "unsupported")
            ++counts[2];
        else
            ++counts[3];
    }
    f << "\nSummary: " << counts[0] << " working, " << counts[1] << " partial, " << counts[2]
      << " unsupported, " << counts[3] << " failed.\n";
    std::printf("[OpenBlox] wrote %s (%d working, %d partial, %d unsupported, %d failed)\n",
                path.c_str(), counts[0], counts[1], counts[2], counts[3]);
}

static int runTests(const std::string& dir, double seconds, const std::string& compatOut)
{
    std::vector<std::string> files;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(dir, ec))
        if (e.path().extension() == ".luau")
            files.push_back(e.path().string());
    if (ec)
    {
        std::printf("[OpenBlox] cannot read tests dir %s\n", dir.c_str());
        return 1;
    }
    std::sort(files.begin(), files.end());

    std::vector<CompatResult> results;
    int filesFailed = 0;
    for (auto& path : files)
    {
        std::printf("--- %s ---\n", path.c_str());
        AllocCap cap;
        Host* host = setupHost(cap);
        registerCompat(host, &results);
        lockDown(host);

        size_t before = results.size();
        bool loaded = oboxRunFile(host, path);
        bool fileFailed = !loaded;
        if (loaded)
        {
            oboxRunFrames(host, (int)(seconds * 60.0), 1.0 / 60.0);
            // Async checks that never reported are failures (their error was
            // already printed by the scheduler).
            for (size_t i = before; i < results.size(); ++i)
                if (results[i].status == "pending")
                {
                    results[i].status = "failed";
                    results[i].note = "did not complete (see error above)";
                }
            for (size_t i = before; i < results.size(); ++i)
                if (results[i].status == "failed")
                {
                    fileFailed = true;
                    std::printf("FAIL %s: %s\n", results[i].name.c_str(),
                                results[i].note.c_str());
                }
        }
        std::printf("%s %s\n", fileFailed ? "FAIL" : "PASS", path.c_str());
        filesFailed += fileFailed ? 1 : 0;
        teardownHost(host);
    }
    writeCompatMd(compatOut, results);
    std::printf("[OpenBlox] %d/%d test files passed\n", (int)files.size() - filesFailed,
                (int)files.size());
    return filesFailed == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------
static void usage()
{
    std::printf("Usage:\n"
                "  openblox <script.luau> [--seconds N]   run a script (default 5s)\n"
                "  openblox --tests <dir> [--seconds N] [--compat-out PATH]\n"
                "                                        run the compatibility suite\n");
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        usage();
        return 1;
    }
    double seconds = 5.0;
    std::string compatOut = "docs/COMPATIBILITY.md";
    std::string scriptPath;
    std::string testsDir;
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--tests" && i + 1 < argc)
            testsDir = argv[++i];
        else if (a == "--seconds" && i + 1 < argc)
            seconds = std::atof(argv[++i]);
        else if (a == "--compat-out" && i + 1 < argc)
            compatOut = argv[++i];
        else if (!a.empty() && a[0] != '-')
            scriptPath = a;
        else
        {
            usage();
            return 1;
        }
    }

    if (!testsDir.empty())
        return runTests(testsDir, seconds, compatOut);

    if (scriptPath.empty())
    {
        usage();
        return 1;
    }
    AllocCap cap;
    Host* host = setupHost(cap);
    lockDown(host);

    std::printf("[OpenBlox] v0.1.0 running %s\n", scriptPath.c_str());
    if (oboxRunFile(host, scriptPath))
        oboxRunFrames(host, (int)(seconds * 60.0), 1.0 / 60.0);
    std::printf("[OpenBlox] done\n");
    teardownHost(host);
    return 0;
}
