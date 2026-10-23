# scarlett

`scarlett` is the AngelScript runtime for Vanillaware's `.asb` scripts. It
decodes the compiled bytecode, hosts the module in a stock AngelScript VM,
reconstructs the engine native API the module imports, and exposes the scene
the script builds as `asb::SceneState`.

It is a library, not a renderer. The viewer consumes `SceneState`; `asbrun`
inspects decoded modules and static scenes. Nothing here touches OpenGL,
audio, or the game executable.

## What it is made of

| Layer | Source | Responsibility |
|---|---|---|
| Decoder | `src/asb.cpp` | Stock AngelScript bytecode reader → `asb::Module`, disassembler, static scene extractor |
| Host | `src/host.cpp` | Creates the `asIScriptEngine`, supplies types and native symbols, loads the module, runs contexts |
| Shim | `src/registry.cpp` | `Registry::dispatch`: the behaviour behind every native call, plus the live `SceneState` |
| Diagnostics | `include/scarlett/trace.hpp`, `call_census.hpp` | Trace events, complete native-call records, call/stub census |

`Registry` is private (`src/registry.hpp`). The supported surface is `Host`,
`RuntimeSession`, the `asb::` model types, and the trace/census helpers.

## Execution model

An `.asb` is compiled AngelScript bytecode, not a container format and not a
native binary. It contains the module's functions, types, globals, strings, and
a `usedFunctions` table describing every external symbol the module calls:
name, namespace, full signature, and kind (`a` = engine native, `m` = module
function, `s` = shared, `n` = null).

It does **not** contain native implementations, and it does not name symbols
that are resolved dynamically at runtime. The pipeline is:

```text
CPK entry (.asb bytes)
  → asb::parse()             Module: bytecode + usedFunctions/usedTypes/globals
  → Host::register_module()  recreate every kind-'a' symbol in the engine as a
                             generic stub; register value/reference types,
                             behaviours, factories, globals and properties
  → Host::load()             asIScriptModule::LoadByteCode(); the engine links
                             each CALLSYS index to the registered symbol
  → RuntimeSession           execute the entry function under a Wait scheduler
        CALLSYS → GenericStub → Registry::dispatch() → SceneState
  → consumer                 viewer renders SceneState; asbrun inspects modules
```

The critical part is the load/link step. `LoadByteCode` reads each
`usedFunctions` entry, rebuilds a declaration from the file, and searches the
already-registered globals, object methods, behaviours, factories, and funcdefs
by namespace, name and signature. If one is missing or different, the load
fails with "invalid bytecode". `usedTypes` are validated the same way, including
their size and flags. This is why registration must happen *before* `load()`,
and why registration is reconstructed from the module rather than taken from a
static API table.

Consequences:

- **scarlett supplies the symbols; AngelScript executes.** The VM owns opcode
  dispatch, call frames, the value stack, object lifetime, exceptions and
  suspension. scarlett creates the engine, registers the module's external
  symbols, loads the bytecode, prepares contexts and calls `Execute`.
- **Native behaviour is ours.** Every native is registered with
  `asCALL_GENERIC` and `GenericStub`. The game's real implementation is never
  involved. `Registry::dispatch` implements the subset that affects the scene;
  anything unhandled returns a default value and is reported as a stub.
- **Only `kind a` is registered by us.** `kind m` functions are part of the
  module and execute normally. `kind n` is a null link.
- **The VM has no direct renderer access.** It mutates `SceneState`; the host
  application reads that state. This is the seam that lets the viewer render a
  script without the VM knowing about GL.
- **`RuntimeSession` is the scheduler.** `Wait(n)` calls `Suspend()`; the
  session resumes the context at `frame + max(1, n)`. `advance()` moves the
  scene clock by one 60 Hz frame, updates movement/camera tweens, then re-enters
  the VM when the wait expires. Optional providers (track lengths and track
  names) make animation waits carry real time; the viewer installs them, while
  a raw `Host` without providers leaves such waits immediate.
- **`Host::execute` bypasses the scene.** It runs a context with no `Registry`
  attached, so all natives are default stubs and `Wait` does not park. This is
  only useful for smoke tests (`asbrun exec`).

### Symbols and ABI details that must match

The loader compares serialized declarations against the engine's registry, so
the host must reproduce:

- global functions, object methods, constructors/destructors, factories and
  list factories, in the correct namespace;
- value-type names, sizes and flags (`FnVector2` 12, `FnVector3` 12,
  `FnVector4` 16, `FnColor` 4, with the same POD/application flags the game
  used);
- script-defined object types as opaque references;
- engine global properties and object properties (registered as placeholders);
- `string` and `array` from the stock `scriptstdstring` / `scriptarray` add-ons,
  including the game's `array<T> f(int&in, int)` factory spelling.

A duplicate registration makes AngelScript set `configFailed`; it then stops
building system-function interfaces and value-returning natives silently break.
The host therefore skips declarations that already exist instead of registering
them twice.

## What it does

- Decodes every `.asb` in the archive (680 modules), including function bodies,
  signatures, namespaces, local names, line info and the `used*` tables.
- Disassembles functions and lifts a linear call stream (`SceneEvent`) and a
  folded `SceneIR` for `asbrun scene` / `timeline` / `scan`.
- Reconstructs and runs the module in AngelScript 2.34.0, with a coroutine-style
  `Wait` scheduler and a prelude hook (the viewer runs `ON_TITLE_UI_START`).
- Implements ~630 of the 686 distinct `kind a` declarations as native handlers:
  entities, chara resolution, animation/track selection, camera and tweens,
  fades, shake, bokashi, effects, point lights, variation, fog, gradient, stage
  configuration, dialogue parameters, save/party state, battle configuration,
  and vector/color/value-type maths.
- Exposes the result as `SceneState`: entities, camera, fades, stage, text,
  effects, lights, audio, items, callbacks, UI, field records, plus a revision
  counter.
- Provides trace events, complete `NativeCall` records, and a call/stub census
  used by the viewer's `--trace`/`--calls`.
- Loads the engine-derived runtime tables supplied by its host: CharaSet/class
  assets, stage and facility data, drop tables, enum names, battle backgrounds,
  and ACB sound lengths.

## What it does not do

- It does not execute AngelScript bytecode itself. There is no interpreter,
  JIT or bytecode walker in `scarlett`; execution is stock AngelScript 2.34.0.
  scarlett's job is to supply the symbols that make the module linkable and
  give the scene-relevant ones behaviour.
- It does not modify or fork AngelScript. The vendored tarball is built
  unmodified; the language, calling conventions, object model and suspension
  semantics are upstream.
- It does not provide the game's native implementations. It provides matching
  declarations so the loader can link them, then an approximation in
  `Registry::dispatch`. Natives outside that approximation are default stubs,
  which can steer branches differently from the real script.
- It does not own the scene clock. AngelScript supplies `Suspend`/`Execute`;
  `RuntimeSession` supplies the 60 Hz frame loop, wait bookkeeping and resume
  policy.
- It does not compile the game's scripts. `.asb` is already compiled; the
  source compiler path (`Host::compile`) exists only for tests/repls.
- It does not interpret the script statically for playback. `extract_scene` and
  `build_scene_ir` are metadata passes for tooling, not an execution path.

## Static extraction is not playback

`extract_scene` / `build_scene_ir` scan a function's instructions linearly and
attribute constant producers (`PshC4/C8`, strings/globals, vector constructors)
to the next call. They do not follow branches, loops, helper return values, or
computed arguments. Use them for inventory (`asbrun scene`, `timeline`, `scan`),
not for fidelity. Playback should go through `Host` + `RuntimeSession`.

One ordering trap: `SceneEvent.strings` are in **bytecode push order**, which is
the reverse of the declared parameter order. For example `DrUiCreateChara`
prints as `(ftx, mbs, label, charaName, int)` while the runtime parameter order
is `(charaName, label, mbs, ftx, int)`. Runtime handlers index declared
parameters; static consumers should match `.mbs`/`.ftx` rather than position.

## Format notes

- Stock AngelScript, compiled by a 2.33.0–2.35.1 toolchain. The vendored engine
  is 2.34.0.
- Raw 32-bit scalars (type flags, enum values) are **big-endian** (Switch
  host). Var-ints, opcode bytes and string bytes are endian-neutral.
- Strings use AngelScript `WriteString`: a var-int length, doubled, with the low
  bit set meaning "reference to an earlier string".
- Instructions are an opcode byte plus var-int operands. The opcode layout is
  the 201-entry table of 2.35.1 and is stable across the target family.
- Section order: debug flag, enums, classes, funcdefs, methods/properties,
  typedefs, globals, script functions, global function refs, binds, then
  `usedTypes`, `usedTypeIds`, `usedFunctions`, `usedGlobalProps`,
  `usedStrings`, `usedObjectProps`.
- `declaredAt` packs `(sectionIndex << 20) | line`.
- The `usedFunctions` tag is `'a'`, `'m'`, `'s'` or `'n'`.
- Script time is 60 Hz. MBS animation sequences run at 120 ticks/s; providers
  convert track durations to 60 Hz frames.

## Native API inventory

The archive imports 686 distinct `kind a` declarations: 656 engine natives and
30 add-on/behaviour internals (`$beh*`, `$fact`, `$list`, `string::*`,
`opIndex`, `length`). Roughly: `St*` 173, `Fl*` 92, `Dr*` 85, plus `Set*`,
`Get*`, `Is*` families. Every engine name has a handler or an explicit
family match in `registry.cpp`; `docs/DEFERRED_APIS.md` reports 0 deferred at
name level.

Name coverage is not run-time fidelity. The status split in
`docs/API_AUDIT.md` is the useful view: about 110 natives write state the
renderer consumes today; the rest are state-only, record-only (text/UI/field/
items/audio/gradients), no-ops, or queries.

Adding a native means:

1. confirm the declaration in `docs/API_NATIVES.tsv` or with `asbrun scene`;
2. add a `name == "..."` branch in `Registry::dispatch` (or a typed block);
3. write the state the host consumer needs, set return values on `gen`, and
   call `mark_dirty()` when entities/camera change;
4. return `true` so the generic stub does not overwrite the return slot;
5. use `viewer --headless --calls` to confirm the native is no longer a stub.

Dispatch matches the bare function name, ignoring namespaces; check the object
type (`fn->GetObjectType()`) when a method name is shared.
