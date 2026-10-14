# OpenBlox — Compatibility Roadmap

**Iron rule: no compatibility claim without a test.** The suite in `tests/`
scores every API member as ✅ working, 🟡 partial, or ❌ unsupported, and
`docs/COMPATIBILITY.md` is generated from test runs — never hand-edited.

API priority tiers come from usage-frequency research against the public
Creator Docs (see `docs/ARCHITECTURE.md` §8). Each phase below lists scope,
done-criteria, and top risks.

## Phase 0 — Foundation ✅ done

Research (Luau embedding, API surface, building blocks), repo scaffold, legal
stance, build toolchain verified.

## Phase 1 — v0.1 Headless core 🔨 now

Tier 0 — nothing works without these:

- Real Luau VM + `require`, `typeof`, `print`/`warn`
- `Instance`/`DataModel` core (§6 of ARCHITECTURE.md), `game` + `workspace`
  globals, `ServiceProvider.GetService`
- Datatypes: `Vector3`, `Vector2`, `CFrame`, `Color3`, `BrickColor`, `UDim`,
  `UDim2`, `Enum`
- `RBXScriptSignal`/`RBXScriptConnection`, `task` library
- `Script`/`LocalScript`/`ModuleScript` execution contexts
- Headless compat harness → `docs/COMPATIBILITY.md`; demo experience script

**Done when:** Tier 0 checklist is green in the generated matrix and the demo
script runs end-to-end.

## Phase 2 — v0.2 World & gameplay

Tier 1 — needed by nearly every experience:

- Container services: `Workspace`, `Players`, `Lighting`,
  `ReplicatedStorage`, `ReplicatedFirst`, `ServerScriptService`,
  `ServerStorage`, `StarterGui`/`StarterPack`/`StarterPlayer`, `SoundService`,
  `Teams`
- `Model`, `Folder`, `BasePart`/`Part` (+ `MeshPart`, `WedgePart`,
  `SpawnLocation`, `Seat`), `Humanoid` + R6/R15 rig conventions, `Tool`,
  `ValueBase` family, `ClickDetector`, `ProximityPrompt`
- `Workspace.Raycast`/shapecasts, `Workspace.Gravity`; `RunService` frame
  events + `IsServer`/`IsClient`/`IsStudio`
- `RemoteEvent`/`RemoteFunction`/`BindableEvent`/`BindableFunction`
  (local behavior first; transport in Phase 5)
- `TweenService`, `Debris`, `UserInputService`, `CollectionService`

**Done when:** a sample obby-style experience (parts, touch events, leaderstats,
tweens) runs headless with passing tests. **Top risk:** undocumented
`Humanoid` behaviors.

## Phase 3 — v0.3 Physics

- Jolt behind the `PhysicsWorld` interface; collisions; character controllers
  (Jolt's built-in controller → R6/R15 avatars); joints/constraints; ragdoll
  basics
- Deterministic fixed-step simulation

**Done when:** character movement + collision test scenes behave per spec.
**Top risk:** physics edge cases games silently depend on.

## Phase 4 — v0.4 Rendering & audio

- `RenderDevice` abstraction; bgfx or wgpu-native backend
- Parts, materials, lighting/shadows, particles, basic terrain
- miniaudio + `SoundService` (2D/3D playback)

**Done when:** the Phase 2 sample experience renders recognizably.
**Top risk:** rendering parity is the most subjective area to verify.

## Phase 5 — v0.5 Multiplayer

- Authoritative server; DataModel replication (change journal over ENet)
- Player join/leave, character lifecycle; RemoteEvent/Function transport with
  server-side validation; latency handling (interpolation first, prediction
  later)

**Done when:** two headless clients share one server simulation in tests.
**Top risk:** replication timing semantics (deferred signals, ownership).

## Phase 6 — Platform services

Local-first, clearly labeled implementations of documented APIs:
`DataStoreService`, badges, leaderboards, friends, chat, game discovery,
server management.

## Phase 7 — Studio-like tools

Debugger (breakpoints/stepping via Luau debug API), profiler, script error
reporting, compat-testing UI, basic place editing.

## Explicitly out of scope

Bypassing Roblox authentication/security/access controls; redistributing
proprietary Roblox assets; undocumented engine internals; 1:1 `.rbxl` binary
fidelity (best-effort import, validated against real Studio output).
