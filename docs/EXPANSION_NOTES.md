# OpenBlock expansion notes (2026-10-14)

Developer notes for the v0.2 expansion pass. This file is allowed to discuss
the prior prototype's Roblox-oriented framing; user-facing docs were reframed
separately. New code presents OpenBlock as an original open-source voxel game
engine — the words "emulator" / "Roblox-compatible" do not appear in any new
user-visible string.

## What was added (~5,450 new lines)

### Math types (`src/mathx.h`, `src/mathx.cpp`, `src/bindings_math.cpp`)
- `CFrameV` property type + full CFrame math: constructors (`new`,
  `Angles`, `fromEulerAnglesXYZ`, lookAt), `*`, `+`, `-`, `Inverse`, `Lerp`
  (position lerp + orthonormalized basis lerp, documented approximation),
  `GetComponents`, world/object space point & vector transforms.
- `UDim` / `UDim2`, `BrickColor` (48-entry original palette, name/number/
  Color3-snapping constructors), `Enum` system (Material, EasingStyle,
  EasingDirection, HumanoidStateType, SortOrder, Font, PlaybackState) with
  `EnumItem` userdata.
- `easeValue(style, direction, t)` shared with TweenService (id order matches
  the Enum tables).
- `Part` gains `CFrame` (translation-synced with `Position`), `Velocity`,
  `Material`, `Shape` properties. `PropValue` variant extended; attributes
  intentionally do NOT accept the new types (`AttrValue` unchanged).

### Services (`src/services.h`, `src/services.cpp`, `src/bindings_services.cpp`)
- `Players`/`Player` (`PlayerAdded`/`PlayerRemoving` via per-frame child
  diffing, `GetPlayers`, `GetPlayerByUserId`, `Character`), `Lighting`,
  `Teams`/`Team`, `Camera`, `Workspace` subclass (`CurrentCamera`,
  `Gravity` 196.2), `SoundService`/`Sound` (stub: Play/Stop/Pause/Resume),
  `TweenService`/`Tween`/`TweenInfo` (easing, delay, reverses, repeatCount,
  `Completed`, `PlaybackState`), `DataStoreService`/`DataStore`
  (synchronous in-memory, number/string/bool), `Debris`, `BindableEvent`,
  `BindableFunction` (sync invoke; yielding inside the callback errors).
- New services in `DataModel` ctor; default `Camera` parented to Workspace.
- `toPropValue` gained an `"Instance"` branch (needed for `Character`,
  `CurrentCamera`, etc.).

### Physics + Humanoid (`src/physics.h`, `src/physics.cpp`, `src/bindings_physics.cpp`)
- Per-frame gravity + velocity integration for unanchored parts; axis-
  separated AABB collision vs anchored parts (landing zeroes Y velocity).
- `Humanoid` (Health clamp, `TakeDamage`, `Died`, `MoveTo`/`MoveToFinished`,
  `Jump`), `BuildRig()` (6-part blocky rig at model origin; HumanoidRootPart
  doubles as the torso slot).
- Limitations: anchored-only collision, no rotation in AABB, dynamic-vs-
  dynamic not simulated.

### Renderer + place files + CLI (`src/render.h/.cpp`, `src/placefile.h/.cpp`)
- `oboxRenderPPM`: software rasterizer (perspective, painter's + z-buffer,
  per-face shading, ball support) → binary P6 PPM. `--screenshot out.ppm
  [--width W] [--height H]`.
- JSON place save/load (`--place-out` / `--place-in`); recursive merge on
  load makes save→load→save byte-identical. `Instance`-typed and read-only
  props are skipped with a warning.
- `docs/COMPATIBILITY.md` header reframed to "OpenBlock API coverage matrix".

## Leak / correctness fixes (independent audit findings)
- `Signal:Wait()` now parks the thread (`Scheduler::park`) before yielding:
  no more bogus "yielded outside of task.wait()" error, anchor retained,
  `task.cancel` drops parked waiters (`dropThread` covers `m_sleepSet`;
  `Signal::fire` skips unparked waiters).
- `Signal` connection refs: `luaL_unref`→`lua_unref` on disconnect; `~Signal`
  releases remainders (teardown now destroys DataModel before `lua_close`);
  stable uint32 connection ids + compaction (never mid-`fire()`).
- `wait()`/`deferThread()` sanitize NaN/Inf/negative delays.
- `resumeWork` error path sweeps all queues via `dropThread()`.
- `Instance::destroy()` cascades to descendants.
- `m_propSignals` is now `std::map` (stable addresses, no rehash dangle).
- `SpawnLocation` is a real `Part` subclass with correct `ClassName`.
- `--seconds` rejects negatives/NaN; `writeCompatMd` checks the stream.
- **Identity**: one cached userdata per `Instance` (`Host::instanceRefs`);
  `==`, table keys, and event payloads now agree across API boundaries.

## Bugs found during integration (fixed)
- Luau's `lua_ref` does NOT pop (unlike PUC `luaL_ref`) — a stray stack value
  corrupted later registrations ("attempt to call a userdata value").
- `TweenInfo.new` created its result userdata before validating args, shifting
  optional-arg indices (only full-arg calls worked).
- `TweenService:Create` used dot-call indices but tests call it with `:` (self
  at arg 1) — shifted to 2/3/4.
- Place-load duplicated the default Camera (merge now deduplicates children).
- `test_players.luau` count-based assertions raced with concurrent checks in
  the same file → rewritten identity-based.

## Test results (2026-10-14, three consecutive runs)
`./build/openblox --tests tests --seconds 8` → **15/15 files pass** each run:
109 working, 1 partial, 7 unsupported, 0 failed. No regressions in the 7
original files. `scripts/demo.luau` still runs end-to-end.

## Open follow-ups
- Renderer ignores part rotation; Transparency > 0 parts are skipped.
- DataStore is in-memory only; no persistence yet.
- Tween rotation uses component-wise lerp (approximation).
- `BindableFunction` callbacks cannot yield.
- Physics: no dynamic-vs-dynamic collision, no joints/constraints.

## Hardening pass (2026-10-14, follow-up)

### Leak-audit fixes (applied)
- `Signal:Wait()` now parks in the scheduler (`sched.park(L)`) before
  `lua_yield`; waiters are scheduler-visible, `task.cancel` drops them via
  centralized `dropThread()` (sweeps `m_deferred`, `m_sleeping`,
  `m_sleepSet`).
- `Signal` handler registry refs released on `disconnect`, `once`
  auto-disconnect, and in `~Signal`; `m_handlers` compacts (never mid-`fire()`);
  stable uint32 connection ids.
- NaN/Inf delays sanitized in `wait()`/`deferThread()`.
- `Instance::destroy()` cascades to descendants (copies child list first).
- `getPropertyChangedSignal` storage changed to `std::map` (no rehash
  dangling).
- `SpawnLocation` is a real subclass; `--seconds` rejects negative/NaN;
  `writeCompatMd` checks the ofstream.
- `checkInstance` no longer relies on dead `luaL_checkudata` fallback —
  `tostring(game)` and `game == game` work.

### Identity (fixed)
- One cached userdata per `Instance` (`Host::instanceRefs`); `__eq` on the
  Instance metatable compares underlying pointers. `Touched` payloads,
  table keys, and stored references now agree.
- Signal metatable gained `__eq` (same underlying `Signal*`).

### Fuzz fixes (applied)
- Parenting cycles rejected: `setParent` returns false on cycle, binding
  raises catchable "Parenting cycle" error; `getDescendants`/`cloneTree`
  have visited-sets; `getFullName` is iterative with a depth cap.
- Signal recursion guard: >1000 fires of one signal in a scheduler frame
  raises a catchable error (counter reset before raising so engine-context
  fires, e.g. from physics, can't `std::terminate`).
- Wall-clock run deadline: `--seconds` + 10 s grace enforced in
  `oboxRunFrames` (frame loop and drain loop).
- `touchCheck` runs in the drain loop.

### Security fixes (applied)
- Instance cap: 32768 per DataModel (catchable error past it).
- Signal handler cap: 10000 per signal (connect returns 0; binding raises).
- All 12 engine metatables frozen with `lua_setreadonly` after registration
  (plus Signal's `__index` table) — metatable hijack now raises.
- Cumulative script CPU budget (120 s per Scheduler lifetime) checked in
  the interrupt hook.
- `oboxRequire`: cycle sentinel cleared on yield-error path; module threads
  get `luaL_sandboxthread`.

### Touch semantics & performance
- `Touched` primed on first check: no spurious frame-1 events for
  pre-existing overlaps; still requires genuine volume overlap.
- `touchCheck` rewritten: per-frame part cache (single property read per
  part), dirty-skip when nothing moved, sweep-and-prune broadphase on X.
- Frame loop exits early when scheduler idle, no Heartbeat connections, and
  no unanchored parts.

### Bug found & fixed: BrickColor userdata segfault
- `oboxPushBrickColor` assigned a `BrickColorV` (containing `std::string`)
  into uninitialized `lua_newuserdata` memory — undefined behavior that
  segfaulted when running `test_datatypes2.luau` after `test_cframe.luau`
  (heap reuse made the garbage `std::string` crash on assignment).
- Fix: `UBrickColor` now stores a palette index (int); accessors look up
  via `brickColorPalette()`. Added `brickColorPaletteIndex()` helper.

### New classes (playtester gaps)
- `ClickDetector` with `MouseClick` signal (input-driven firing awaits the
  input layer; marked unsupported in tests).
- `IntValue`/`NumberValue`/`StringValue`/`BoolValue` with `Value` + `Changed`.

### New tests
- `tests/test_perf.luau` (2000-part touchCheck budget),
  `tests/test_values.luau`, `tests/test_click.luau`.
