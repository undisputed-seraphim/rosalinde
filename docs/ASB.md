# ASB / AngelScript / stage & drama — brain dump

Working notes. Not authoritative documentation, just everything we currently
know and are thinking about, so it can be picked up cold. If you are looking for
the terse facts, the "Data sources" and "Format" sections are the useful ones;
the "Viewer plan" section is the direction of travel.

## TL;DR

- `.asb` files are **compiled AngelScript bytecode** (stock AngelScript, not a
  custom container). Vanillaware compiled their scripts with an AngelScript
  2.33.0–2.35.1 toolchain and saved for the Switch, so the raw 32-bit fields are
  **big-endian**.
- The game is a **C++ engine + a script layer**: a stock AngelScript VM with a
  large custom native API registered into it, a name-keyed scene/object registry,
  and a coroutine-style scheduler. The scripts are the game; the engine renders.
- We can fully decode every `.asb` (all 680 in `Unicorn.CPK` parse to the exact
  final byte) and extract a decent model of what each scene creates.
- The viewer is slowly turning into **an ASB interpreter with a renderer
  attached** — see "Viewer plan".

## What `.asb` is

Compiled AngelScript. Concretely:

- The first byte is `stripDebugInfo`, then section counts (enums, classes,
  funcdefs, typedefs, globals, script funcs, global funcs, bind infos), then the
  `usedTypes` / `usedTypeIds` / `usedFunctions` / `usedGlobalProps` /
  `usedStringConstants` / `usedObjectProps` trailers.
- `0x66` is ASCII `'f'` (new function marker), `'r'` is a function reference,
  `'\0'` a null function. Strings are `WriteString`-encoded
  (`len*2` in a var-int, low bit = "reference to a previous string"), then raw
  bytes.
- Instructions are `opcode byte` + args, each arg a **variable-length encoded
  integer** (`asCWriter::WriteEncodedInt64`). This "packed" encoding is *stock*
  (it exists since ~2.10); it is not a Vanillaware invention. The opcode enum is
  byte-for-byte identical across 2.34–2.39 (`asBC_MAXBYTECODE = 201`).
- The raw 32-bit scalar fields that are *not* var-ints — type flags and enum
  values — are stored **big-endian** because the save ran on a big-endian host
  (Switch). Everything byte-oriented (var-ints, strings, opcode bytes) is
  endian-neutral. This one detail is why a naive little-endian read desyncs
  immediately on enum-heavy scripts.
- `func->scriptData->declaredAt` is not a line number; it packs
  `(sectionIndex << 20) | line`.

Version pinning (how we know): we built the save/load tools from the real
AngelScript releases and cross-loaded. `{2.33.0, 2.34.0, 2.35.0, 2.35.1}` produce
byte-identical output and accept the `.asb`; 2.32 and 2.36 do not. The format is
otherwise stable, so the decoder targets that family.

Embedded metadata we get for free: function names/signatures, the source section
path (`C:/Project2/Unicorn/Soft/Data_JP/Script/*.ascp`), included headers
(`.ash`, `.ashp`, `UcEnumPlain.h`, `UcSoundDef.h`), local variable names, line
numbers, and the whole `usedX` tables.

## The execution model (how the game actually runs)

Theatre analogy:

- **Engine (C++)** = stage + crew + equipment: loads assets, builds animated
  sprites, draws them, plays audio, keeps a live "green room" of actors.
- **Each `.asb`** = a director's script (stage directions).
- **MBS** = the choreography for one actor's body (how one sprite animates).
- **FTX** = the costume/artwork (textures).
- **The registered native API** = the intercom the director uses to tell the
  crew what to do.

Layers top to bottom:

1. **AngelScript VM (stock).** Unmodified language/runtime.
2. **Native API registered into the VM.** At boot the engine calls
   `RegisterGlobalFunction` / `RegisterObjectType` / `RegisterObjectMethod` to
   expose hundreds of C++ functions and a few value types (`FnVector2`,
   `FnVector3`, `FnColor`, `UcSpriteTextParam`, `UcMsgBalloonParam`, `string`,
   `array`, …). *This* is the "custom runtime" people mean — a huge custom API
   surface on top of stock AngelScript, not a modified language.
3. **Scene/object registry (engine-side).** Live objects keyed by name.
   `CreateSpriteMone(...)` creates one; `DrSetPosV(name, …)`,
   `DrSetIdleAnimation(name, …)`, `DeleteSprite(name)` mutate it by name.
4. **Scheduler (coroutine-style).** `Wait(t)` / `SUSPEND` parks a script and
   returns to the engine; the engine keeps rendering, then resumes it later.
   This is what makes cutscenes play out over time. `…Async` returns
   immediately, `…Wait` blocks until done.

Every `CALLSYS <id>` in the bytecode is a call into this registry; at load the
id is bound *by name/signature*. Each `.asb`'s `usedFunctions` table (the
`kind 'a'` entries) is literally the set of engine functions that script uses —
that is the script↔engine interface. There are ~686 distinct engine functions
in use across the archive.

### API vocabulary (prefixes)

- `Dr*` — drama / cutscene (create chara, effects, camera, animation).
- `Fl*` — field / overworld (stages, symbols, collisions, weather).
- `St*` — stage / strategy (units, AI, battle scene).
- `Db*` — database / debug (local params).
- `Uc*` — Unicorn data types / enums (`UcMsgBalloonParam`, …).

Common scene calls: `DrCreateStage(int,int,int,int)`,
`DrCreateChara(displayName, defName, int)`,
`CreateSpriteMone(name, mbs, ftx, track, FnVector2 pos, layer, scale, bool, …)`,
`DrSetPosV(name, FnVector3)`, `DrSetIdleAnimation(name, anim)`,
`DrSetCameraPosV(FnVector3, …)`, `DrSetScale`, `DrCreateEffect`, `DeleteSprite`,
`StCreateUnit(defName, …)`, `FlCreateStage`, `SetSpritePosV`, …

### A cutscene, frame by frame

1. Engine loads the module and calls its entry (`scene00` / `MAIN`).
2. The script runs until it yields, issuing create/set calls that land in C++ and
   update the registry / render list.
3. `Wait(30)` parks the script; the engine renders frames 0…29, advancing MBS
   animations.
4. On frame 30 the script resumes → next batch of calls. Repeat.

So a "scene" is a program that runs over many frames, not a file format.

## Tooling

The `asbrun` shell is the master ASB tool: `open`, `summary`, `scene`, `timeline`,
`play`, `trace`, `record`, `calls`, `state`, `scan`, and `export`. See
`tools/ASBRUN_README.md` for the command reference. The old `asb_harvest`/`asb_run`/
`asb_record`/`asb_trace` binaries are legacy reference implementations.

The decoder lives in the `scarlett` library, so the viewer and the tools link it.

## Scene extraction pipeline

A ladder of "how much do we pretend to be the VM":

- **Static snapshot (done).** Decode → ordered events → fold into a **SceneIR**.
  For each script we get named **entities** with `mbs`/`ftx`, `position`,
  `scale`, `track`, `visibility`, draw order, plus camera keys and stage ids.
- **Static timeline (done).** Attach times (`Wait`/`SUSPEND`, async vs wait)
  and replay the event list over time. `asbrun timeline FUNC` exposes this;
  no branching/loops/computed values.
- **Execute the VM with a shim API (done).** Load `.asb` into real stock
  AngelScript, register our own implementations of the native functions and
  value types, run under an `asIScriptContext`, drive a `Wait` scheduler. Gives
  real control flow and evaluated arguments, but means reimplementing (a growing
  subset of) the engine's registered services, and stub return values drive
  branching. Implemented in `scarlett` (`Host::run` + the `Registry` shim),
  exposed through the `asbrun` trace/record modes (`asb_record` is legacy). All 680
  scripts load and run.

The SceneIR fold recognises: create (`CreateSpriteMone`, `DrUiCreateChara`,
`CreateSpriteCoA/Effect`, `DrCreateChara`, `DrCreateNpc`), position
(`DrSetPosV`/`SetSpritePosV`/`DrUiSetPosV`/…), scale, animation
(`DrSetIdleAnimation`/`SetSpriteMoneAnimation`), visibility (`DeleteSprite`),
camera (`DrSetCameraPosV`/`DrSetCameraZoom`), stage (`DrCreateStage`/`FlCreateStage`/
`StSetBattleScene`).

Argument lifting is currently a *scan*: it collects constant producers
(`PshC4`/`PshC8` numbers, `PGA` strings/globals, `FnVector2/3::beh0`
constructors) between calls and attributes them to the next call. Good enough for
literals; computed arguments (`GetPosV`, arithmetic, offsets) are approximate.

## Data sources inside `Unicorn.CPK`

- `Debug/_UcEnum_*.inc` — generated enum headers (plain text):
  `CHARASET` (1388 character definitions), `CLASSTYPE` (74 classes), `NPCTYPE`
  (320 NPCs), plus Buff/Item/Skill/Facility/… Each entry is
  `NAME , // localized label`.
- `MsgSheet/*.fms` — FMSB string tables (localized). `UcCharaList`,
  `UcClassList`, `UcCharaNpcList` are names; `UcCharaColorList` is a list of
  **color names** (`青`, `赤`, …).
- `Chara/<Class>_<M|F>NN.mbs` + `.ftx` — battle sprites.
- `CharaOp/Op_<Class>_<M|F>N.mbs` + `.ftx` — drama/cutscene sprites (what the
  scene scripts reference).
- `Chara/<Class>.otm`, `OTM/Chara/<Class>_<PART>.otm` — `OTBM` object/part maps
  (120-byte header, embedded `<CLASS>.MBS`), parts like `_MOBM1..6`, `_GLO`,
  `_HDL`, `_AMR`. Per **class**, not per named character.
- `DbReg/DbRegDb.drg` — the engine's tuning DB (font/color/sound/field/camera).
  No asset references.
- `NodeNetworkData/*.nnd` — overworld map graph; unrelated to chara.

### How scripts identify characters

By **def name**, never a numeric appearance mask: `DrCreateChara(display,
defName, int)` (e.g. `DrCreateChara("主人公","アレイン",0)`), `DrCreateNpc`,
`DrUiCreateChara(ftx, mbs, dispName, charName)`, `StCreateUnit(defName, …)`. A
scan of all 680 scripts found **none** of the viewer's appearance-mask values,
so scenes do not pass those numeric flags — the engine resolves the def name.

## Name resolution chain

- **Drama:** `dispName`/`charName` → `CharaOp` asset, via the `DrUiCreateChara`
  calls harvested across the archive (`--chara-table` →
  `viewer/data/chara_table.tsv`, 47 named characters).
- **Battle:** class → `Chara/<Class>_<M|F>` comes from the executable's CLASSTYPE
  record table (dumped by `tools/dump_chara_table.py`); a def name → class by the
  class token inside it (`PL_FIGHTER_C0 → FIGHTER`).
- Generated viewer tables: `classes.tsv` (class → asset, dumped from the
  executable) and `battlebgs.tsv` (BattleBG → asset). The only remaining
  hand-maintained data is `variants.tsv` (the per-character appearance masks).

## The viewer plan (the interesting bit)

The viewer started as a manual MBS/FTX sprite browser with a hard-coded table of
classes. It is becoming — deliberately — **an ASB interpreter with a renderer
attached**. Concretely:

- It already decodes `.asb` (via `scarlett/`), extracts a scene, and builds a
  `SpriteLayer` per entity with positions/tracks/visibility.
- The asset tables are now **generated** from the game's own data, not
  hand-written, so the viewer has no baked-in knowledge of specific assets.
- The direction: instead of "load MBS X and place it manually", the viewer takes
  a **script + function + time** and reconstructs (and eventually replays) the
  scene the engine would have built.

The path there is the same ladder as "Scene extraction pipeline":

1. **Snapshot** (today): fold a fixed function into a static scene and render it.
2. **Timeline**: honour `Wait`/`SUSPEND` ordering to replay a cutscene.
3. **Emulate**: run the real VM with a shim of the engine API so control flow
   and computed values are correct ("become a host for AngelScript again").

The interesting tension: a *static* reader can cover a lot of near-linear
cutscenes cheaply, while full fidelity essentially requires reimplementing the
registered services (value types, the scene registry, the scheduler). The
pragmatic plan is to build the static pipeline well and treat VM execution as a
deliberate, later escalation only for scenes that genuinely need branching.

Current status: the viewer runs the VM live (`--script`) over
`RuntimeSession`/`SceneState`; there is no separate static-fold playback path.
Generated character/BG tables load from `viewer/data`. The VM path models
value-type maths (`FnVector*` operators), per-entity animation/facing/fade,
camera tweens, and MBS-track waits (see `scarlett/`). Open work is MBS appearance
conformance (per-texture UV orientation, `s6` keyframe transitions, draw order)
and the remaining text/UI/field channels.

## Open problems / gaps

- **Coordinate space.** Stage/field content uses the drama field camera;
  `CreateSpriteMone` overlays use design space (1920×1080, fixed camera), while
  `CreateSpriteMoneWorld`/`CreateSpriteMoneStageStart` stay in field space. See
  `AGENTS.md` for the current model. (The old `--scene-camera` selection no
  longer exists.)
- **MBS appearance.** Per-texture UV orientation and `s6` layer transitions are
  not fully modelled; the title's colored phase and draw-order layering remain
  open. See `docs/TOOLS.md` for inspection commands and `AGENTS.md` for the
  known-mess entry.
- **Engine RE.** Native registration/relocation recipes and the camera model are
  in `docs/EXE.md`.
- **Appearance masks.** The viewer's `variant_flags` (per-character selection
  over an MBS's `s4.attribute` groups) are not in the enums, FMS, or scripts.
  Best guesses for a source: the `OTBM` part maps, or engine chara data. Until
  found, `viewer/data/variants.tsv` stays hand-authored (45 lines).
- **Computed arguments / branching.** The static argument lifter only recovers
  literals; anything derived from `GetPosV`, arithmetic, or branches is lost.
- **Helper inlining.** Creations inside script helpers (`DrObj`, `DrEffect`, lib
  functions) are only captured if the helper hard-codes the asset; most forward
  parameters, which we can't yet bind.
- **Character defs without sprites.** ~103 enum classes (mostly `BOSS_*`) have
  no `Chara/` asset; `Unify_F.mbs` exists but no class references it;
  `Unify_M` (viewer "SnowRanger") had to be dropped.
- **VM emulation cost.** The VM now runs all 680 scripts, but fidelity is a
  shim problem: unmodelled natives return defaults, so branches that consult
  engine state can still take the wrong path, and only `Wait(int)` carries
  real time (`WaitSpriteMoneAnimationEnd` etc. are immediate). `asbrun record`
  and `asbrun trace` log a scene's native calls for spotting gaps.

## Gotchas worth remembering

- Big-endian raw dwords; var-ints byte-oriented.
- `declaredAt = (sectionIndex<<20)|line`.
- Watch out for the identifier `cdecl` — it collides with a keyword on some
  Windows toolchains (renamed to `cdecl_str` once already).
- The viewer's data files are found via `VIEWER_DATA_DIR` (compile-time) or
  `$VIEWER_DATA`.
- Scripts reference FTX with a single-digit frame suffix that is zero-padded in
  the archive (`Op_Prince_M0.ftx` → `Op_Prince_M00.ftx`), and the game's VFS is
  case-insensitive (`GUI/interface.mbs` vs `GUI/Interface.mbs`).

## Loose ends / things that were wrong before

- `docs/ACB_AFS2.md` is about CRI audio ACB — unrelated to `.asb`.
- The old `src/impl/asb_format.md` described `.asb` as a custom container with
  packed instructions and a `0x6601`/`0x6602` header. That is all wrong: it is
  stock AngelScript, and `0x6601` is just `function_count=1` followed by `'f'`.
