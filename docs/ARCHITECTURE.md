# OpenBlox — Architecture

> **Working title.** OpenBlock is an original open-source voxel game engine,
> built clean-room from public documentation and permissively licensed
> dependencies. It is not affiliated with, endorsed by, or connected to
> Roblox Corporation. It contains no third-party source code, no proprietary
> assets, and does not bypass any authentication, security protections,
> access controls, or restrictions.

## 1. What this is

A modular voxel game engine for block-based 3D experiences: a real Luau
script runtime, a reflection-driven `Instance`/`DataModel` core, and
replaceable modules for physics, rendering, networking, audio, assets, and
platform services. Headless-first: the core runs with no GPU (game servers,
tests), and rendering attaches as a module.

## 2. Goals / non-goals

**Goals**

- A complete, original engine for block-based 3D games.
- Real behavior over claims; every supported API backed by a test.
- Modular: add physics, rendering, or networking backends without rewrites.
- Clean-room throughout: public documentation + permissively licensed
  dependencies only.

**Non-goals**

- Bypassing any third party's authentication, security, or access controls.
  Out of scope, permanently.
- Redistributing proprietary assets. Only assets we have rights to.
- Chasing undocumented internals of other engines.
- Claiming API support that hasn't been demonstrated by the test suite.

## 3. Module map

| Module | Responsibility | Technology choice | Status |
|---|---|---|---|
| `luau-runtime` | Script compilation & execution, per-script sandboxing, watchdog, memory budgets | Genuine Luau (MIT): `Luau.VM` + `Luau.Compiler` static libs, `luaL_sandboxthread`, interrupt callback | v0.1 |
| `core-datamodel` | `Instance` hierarchy, properties, signals, attributes/tags, `DataModel` + `ServiceProvider` | C++, reflection-driven metadata tables | v0.1 |
| `datatypes` | `Vector3`, `CFrame`, `Color3`, `UDim`/`UDim2`, `BrickColor`, `Enum`, … as Luau userdata | C++ | v0.1 |
| `scripting` | `Script`/`LocalScript`/`ModuleScript` contexts, `require`, `task` scheduler, `RunService` frame events | C++ + Luau coroutines | v0.1 |
| `physics` | Collision, character controllers (R6/R15 rigs), joints/constraints, deterministic fixed-step sim | Abstract `PhysicsWorld` interface; Jolt (MIT) backend | v0.3 |
| `rendering` | Parts, materials, lighting/shadows, particles, terrain | Abstract `RenderDevice`; bgfx (BSD-2) or wgpu-native (MIT/Apache-2.0) backend | v0.4 |
| `networking` | Authoritative server, DataModel replication, RemoteEvent/Function transport, latency handling | ENet (MIT) transport; replication as DataModel diffs | v0.5 |
| `audio` | `SoundService`, 3D positional audio | miniaudio (Unlicense/MIT-0) | v0.4 |
| `assets` | `.rbxlx` XML place import (user-owned places), mesh/texture/animation loading | Community-documented formats; rights-checked assets only | v0.4+ |
| `platform` | DataStores, badges, leaderboards, friends, chat, discovery — local-first, clearly labeled | C++ | v0.6 |
| `studio` | Debugger (Luau debug API: breakpoints/stepping), profiler, error reporting, compat harness UI | C++ | v0.7 |
| `security` | Script sandboxing, server-side RemoteEvent validation, capability model | Luau sandbox APIs + host checks | ongoing |

## 4. Key design decisions

1. **Real Luau, not a clone.** Luau is MIT-licensed and embeds cleanly
   (`Luau.VM` + `Luau.Compiler`, zero external dependencies). Language-level
   compatibility is inherited; all effort goes into the API surface.
2. **Reflection-driven API surface.** One metadata table per class
   (properties, methods, events, security tags) drives script bindings,
   serialization, and replication. Adding API coverage is mostly data, not code.
3. **DataModel as single source of truth.** The server owns the authoritative
   tree; replication ships diffs of it. This keeps gameplay, physics, and
   networking consistent by construction.
4. **Deterministic fixed-step physics.** Jolt's deterministic mode gives
   networked physics a stable foundation for client prediction/reconciliation.
5. **Headless-first.** Everything except `rendering` runs without a GPU, so
   game servers and the entire compat test suite run anywhere.
6. **Clean-room boundaries.** Public Creator Docs + Luau + permissive
   dependencies. GPL-licensed reimplementation projects were identified in
   research and are excluded as dependencies (reference only).

## 5. Runtime data flow

**Script execution:** `Script.Source` → `luau_compile` → `luau_load` into a
sandboxed thread (`luaL_sandboxthread`) → the `task` scheduler resumes the
thread on signals, `task.delay`/`task.wait` expiry, and `RunService.Heartbeat`.

**Replication (v0.5+):** server `DataModel` → change journal → ENet reliable /
unreliable channels → client applies to its replica tree. `RemoteEvent` /
`RemoteFunction` travel as ordered messages with server-side payload validation.

## 6. Prototype v0.1 scope (this repo, now)

C++17, CMake `FetchContent` of Luau 0.742, headless. Implements:

- Luau runtime with sandbox + instruction watchdog + memory budget
- `Instance` core: `Name`, `Parent`, `Archivable`, `Clone`, `Destroy`,
  `ClearAllChildren`, `GetChildren`/`GetDescendants`, `FindFirstChild` (+
  `OfClass`/`WhichIsA` variants), `IsA`, `WaitForChild`, attributes, tags
- Events: `ChildAdded`, `ChildRemoved`, `DescendantAdded`, `DescendantRemoving`,
  `AncestryChanged`, `Destroying`, `GetPropertyChangedSignal`
- `DataModel` (`game`) + `ServiceProvider.GetService`; `workspace` global
- Services: `Workspace`, `Players`, `Lighting`, `ReplicatedStorage`,
  `ServerStorage`, `ServerScriptService`, `RunService`
- Classes: `Part` (`Position`, `Size`, `Anchored`, `Color`, `Transparency`,
  `CanCollide`, `Touched`), `Model`, `Player`, value types `Vector3`/`Color3`
- `RBXScriptSignal` (`Connect`/`Disconnect`/`Once`/`Wait`),
  `RBXScriptConnection`
- `task` library: `spawn`, `defer`, `delay`, `wait`, `cancel`
- `Script` execution + `require` for `ModuleScript`; `print`/`warn`
- Demo experience script + compatibility test harness generating
  `docs/COMPATIBILITY.md`

**Explicitly not in v0.1:** rendering, physics simulation, networking, audio
output, `.rbxl`/`.rbxlx` loading, R15 rigs, animations.

## 7. Biggest technical challenges (honest)

1. **The long tail.** 600+ documented classes, and games depend on undocumented
   behaviors. Breadth is a multi-year grind; the reflection-driven design keeps
   each addition cheap.
2. **Replication semantics.** Deferred signal timing, property replication
   rules, network ownership — subtle, game-breaking if wrong.
3. **Physics parity.** Character controllers, constraints, and edge cases;
   determinism across platforms for netplay.
4. **Rendering parity.** Materials, lighting, atmosphere, terrain — the most
   visible gap, and the most subjective to verify.
5. **Security model.** Server-side validation strict enough to stop abuse,
   lenient enough not to break legitimate games.
6. **Format churn.** The `.rbxl` binary format has changed without version
   bumps (community reports); treat import as best-effort, validated against
   real Studio output.

## 8. Research basis (2026-10-14, verified from primary sources)

- **Luau:** MIT; CMake targets `Luau.VM`/`Luau.Compiler`; selective
  `luaopen_*`; `luaL_sandbox`/`luaL_sandboxthread`; `lua_Callbacks.interrupt`
  watchdog; custom allocator memory ceilings. Release 0.742, actively
  maintained.
- **API surface:** public Creator Docs mirror (`Instance.yaml`, `DataModel`);
  universal `Instance` members enumerated; services catalogued; `task` library
  and `RBXScriptSignal` semantics; `.rbxlx` XML format (community-documented).
  Member lists for `Part`/`Humanoid`/`Player` should be re-verified against the
  live reference before locking checklists (create.roblox.com was unreachable
  from the research sandbox).
- **Building blocks:** bgfx (BSD-2), wgpu-native (MIT/Apache-2.0), Jolt (MIT),
  ENet (MIT), miniaudio (Unlicense/MIT-0). Reference projects: Librebox (MIT,
  API-shape reference), GodotLuau (MIT, working reference incl. `.rbxl`
  import). GPL projects excluded as dependencies.
