# AGENTS.md — Rosalinde Codebase Guide

## What this project is

A C++23 toolset for extracting, parsing, and rendering sprites from Vanillaware games (Unicorn Overlord).
Layers:

1. **criware** — CPK container + UTF table parser
2. **eltolinde** — Game-specific formats: MBS (animation), FTX (textures), FMS (string tables), NND, OTBM
3. **scarlett** — AngelScript (`.asb`) decoder + runtime (hosts modules in a stock AngelScript engine)
4. **viewer** + **extractor** + **ASB CLI** — Executables (OpenGL viewer, CPK extractor, and `asbrun` script shell) sitting on top

---

## Dependency graph

```
              OpenGL / GLAD
                   ↑
       SDL3 ←─────┴────── ImGui
        ↑                   ↑
   viewer.exe ←── imgui ────┘
        ↑
        ├── eltolinde.lib ←── criware.lib
        └── scarlett.lib  ←── angelscript.lib (vendored 2.34.0)

   rosalinde.exe (extractor) ←── eltolinde.lib
   asbrun.exe (ASB CLI) ←── scarlett.lib, criware.lib, eltolinde.lib
```

- **criware**: No dependencies except STL. Self-contained CPK/UTF/CRILAYLA.
- **eltolinde**: Depends on criware, Boost::iostreams (legacy), ZLIB (vestigial), glm. FTX textures are decoded on the GPU by the viewer, not here.
- **scarlett**: Depends on the vendored AngelScript (angelscript + add-ons). Decodes `.asb` and hosts modules in a stock AngelScript 2.34 engine.
- **viewer**: Depends on eltolinde, scarlett, imgui, glad, glm, Boost::program_options, SDL3, ZLIB (screenshot).
- **rosalinde** (extractor): Depends on eltolinde. Interactive CPK shell with in-memory format inspection.
- **asbrun** (ASB CLI): Depends on scarlett, criware, eltolinde. Interactive shell for ASB inspection and live playback.

---

## Environment

- Game archive: `~/Unicorn.CPK`
- Unpacked archive: `~/zwei/unicorn_overlord/CPK_unpacked`
- Game engine ELF (RE): `~/zwei/unicorn_overlord/main2.elf`
- No save file is required.

---

## Build

```bash
cmake -B build -S .
cmake --build build -j 8
```

> **Globbed sources**: the viewer lists its sources with `file(GLOB_RECURSE)`. After adding or
> removing a source file, re-run `cmake -B build -S .` or the new file will not be compiled.
> (Windows/MSVC also works: `-G "Visual Studio 17 2022"` with `--config Debug`.)

**Requirements** (all fetched via FetchContent if not installed):
- CMake 3.28+
- A C++23 compiler (MSVC 2022 `/std:c++latest`, or GCC 14 / Clang 19)
- Boost 1.88 (system, iostreams, program_options)
- SDL3 3.4, ZLIB 1.3.1, glm 1.0, EnTT 3.14+, spdlog 1.17, OpenGL, Catch2 3.12+

The build produces:

| Target | Type | Purpose |
|--------|------|---------|
| `criware` | static lib | CPK/UTF/CRILAYLA |
| `criware_test` | executable | criware tests |
| `eltolinde` | static lib | game-format parsers |
| `scarlett` | static lib | ASB decoder + AngelScript host |
| `scarlett_test` | executable | scarlett tests |
| `rosalinde` | executable | interactive CPK shell + format dumps |
| `asbrun` | executable | interactive ASB shell (master ASB tool) |
| `imgui` | static lib | vendored UI |
| `viewer` | executable | OpenGL drama viewer |

---

## Headless rendering

The viewer renders offscreen on the **real GPU** (AMD radeonsi via Mesa) with no X server:

```bash
SDL_VIDEODRIVER=offscreen MESA_LOADER_DRIVER_OVERRIDE=radeonsi \
  ./build/viewer/viewer --cpk ~/Unicorn.CPK \
      --script Script/Dr_OW_0_Opening.asb \
      --scene-frame 60 --screenshot /tmp/frame.png
```

- `--scene-frame N` advances deterministically before capture.
- `--screenshot PATH` writes one PNG/PPM and exits.
- `--capture-frames 0,60,420,480 --capture-dir DIR` reuses the asset cache for a batch.
- `--trace FILE --trace-categories vm,stage,camera,sprite,draw` writes NDJSON. `vm/call`
  events carry the full call text, caller, handled status, and return value.
- `--probe DIR` writes `state-<frame>.json`, `draw-<frame>.ndjson`, and
  `summary-<frame>.txt` at `--probe-frames` (default: capture frames or `--scene-frame`).
  The draw dump is layer-level: run/`s8`/`s6`/`s7`, UVs, blend, fog, sort keys, and skip reasons.
- `--only NAME` isolates an object (exact name or alias); `--hide TEXT` removes matches;
  `--keep-stage` retains the stage with `--only`. `--clear-color #RRGGBB[AA]` overrides
  the background (default is the blue-gray editor clear).
- `--data-dir DIR` selects the runtime tables (default: `viewer/data` or `$VIEWER_DATA`).
- `--scene-fps` controls playback stepping; `--chara-table` overrides the viewer data table.

### Perceptual opening regression (`visiontest/`)

`visiontest/opening_diff.py` compares deterministic viewer captures against the
reference video (`opening_cinematic.mp4`). It is an **approximate** regression:
compressed video, camera differences, and watermarks mean pixel identity is not
expected.

```bash
python3 visiontest/opening_diff.py \
  --reference opening_cinematic.mp4 \
  --out visiontest/out
```

- Requires `ffmpeg`; uses the Python standard library otherwise.
- Captures with `SDL_VIDEODRIVER=offscreen` and `MESA_LOADER_DRIVER_OVERRIDE=radeonsi`
  unless those variables are already set.
- `visiontest/manifest.json` defines frame fixtures, normalized regions, thresholds,
  and `known_open` frames.
- The runner searches ±2 frames for temporal alignment, downscales and lightly blurs both
  sources, then reports SSIM, gradient correlation, MAE/percentile error, saturation,
  luminance, edge density, and histogram intersection per region.
- Outputs `visiontest/out/report.json`, `visiontest/out/index.html`, and side-by-side/
  difference images. `out/` is ignored.
- A `known_open` failure is reported as `xfail` and does not fail the run; `--strict`
  makes those failures fatal. Remove `known_open` only after the frame is reviewed and
  the threshold is tightened.
- Use `ignore_regions` in the manifest for watermarks or letterbox areas.
- The reference video is not committed. The script is a test harness, not a runtime
  asset-name/frame gate; keep such gates out of viewer code.

---

## Engine / runtime facts

- **Live model**: `asb::RuntimeSession` owns `SceneState`; `DramaPlayback` steps it at 60 Hz;
  `SceneRenderer` consumes the live entities. `--script` is the only viewer playback path.
- **Natives vs scripts**: `.asb` `usedFunctions` kind `a` = engine native (shimmed by
  `Registry`), kind `m` = module code executed by the VM (no shim).
- **Behaviours**: `$beh0` constructor, `$beh2` destructor, `$beh3`/`$fact` factory (returns a
  handle), `$beh4` list factory. Factories stubbed to null cause `ChkNullV` failures; when
  `dispatch` returns true it must placement-new object return slots.
- **MBS**: `section_9` tracks, `section_a` sequences, `section_8` frames. Animation sequences
  advance at **120 frames/s**; script `Wait` and scene frames are 60 Hz. `s7.fog` is a
  per-frame ARGB tint. Full-screen achromatic `NOTEX` quads are draws (veils/fades); smaller
  or saturated ones are masks/palette sources. A finished run holds its final pose only while
  that pose is visible.
- **Space**: stage/field content uses the drama field camera. `CreateSpriteMone` overlays live
  in design space (1920×1080, fixed camera); `CreateSpriteMoneWorld` and
  `CreateSpriteMoneStageStart` stay in field space. `DrUiCreateChara` is UI space.
- **Viewer host**: the viewer supplies all runtime providers: chara tables, track
  name/length callbacks, facility names, stage tables, drop tables, and ACB sound
  lengths. `asbrun` remains a script-only inspector and does not model animation waits.
- **Formats**: `.asb` → `docs/ASB.md`; MBS/FTX → `docs/SECTIONS.md`; engine tables and native
  registration → `docs/EXE.md`; combat/stage surface → `docs/COMBAT.md`.

---

## Viewer debug environment variables

| Var | Effect |
|-----|--------|
| `VIEWER_LOG_LEVEL` | spdlog level: `trace`, `debug`, `info`, `warn`, `error`, `off` |
| `SCENE_TRACE` | legacy stderr trace; prefer `--trace` |
| `ASB_TEXT`, `ASB_CENSUS`, `ASB_CALLBACKS` | dialogue, native-call, callback diagnostics |
| `VIEWER_TRACE_SPRITES`, `VIEWER_TRACE_DETAIL` | per-draw and run-state tracing |
| `VIEWER_RUN_ONLY`, `VIEWER_RUN_FROM`, `VIEWER_MAIN_ONLY` | restrict MBS runs for debugging |
| `VIEWER_RUNS=seq`, `VIEWER_SEQ_MAIN`, `VIEWER_ADVANCE`, `VIEWER_ONCE` | sequential-phase / start-offset / end-mode experiments |
| `VIEWER_HIDE` | hide objects whose name contains the value |
| `VIEWER_DATA` | runtime data directory (same as `--data-dir`) |
| `VIEWER_CAM_Y`, `VIEWER_ORTHO` | camera/design-space overrides |
| `BG_DUMP`, `BG_LAYERS`, `FTX_GPU_DUMP` | MBS/FTX asset diagnostics |
| `FTX_GPU_DUMP_DIR`, `FTX_GPU_DUMP_MATCH` | dump uploaded texture arrays to PNGs, optionally filtered by asset label |
| `VIEWER_FLIP_V` | experimental UV V-flip by MBS/FTX substring for orientation diagnosis |
| `VIEWER_RUN_ORDER` | experimental run draw order: `reverse` or `track` |
| `VIEWER_NO_FOREGROUND_HOLD` | disable compound-track last-visible-pose hold |
| `VIEWER_NO_MAIN_LIFETIME` | disable main-sequence lifetime culling |
| `VIEWER_FONT`, `VIEWER_SIZE` | font override / offscreen size |

---

## Key conventions

- **C++23**: Uses `std::span`, `std::expected` (future), fold expressions, concepts, `fixed_string` NTTP
- **Tabs for indentation** (`.clang-format`: `UseTab: Always`)
- **No comments** in code (unless documenting complex format logic)
- **Namespaces**: `criware::`, `utf::`, `mbs::`, `sprite::`, `glxx::`, `ftx::`, `fms::`, `nnd::`, `asb::`, `otm::`
- **String type**: `std::string` for ownership, `std::string_view` for UTF/CPK views (no `wstring`, no `u8string`)
- **Parsing style**: Span-based with `byte_reader.hpp` (preferred). Iostream is legacy wrapper.
- **GL version**: OpenGL ES 4.6 core (GLAD). Shaders are GLSL ES 3.20.
- **UTF tables are schema-driven**: `utf::raw_table` validates the @UTF header, column descriptors, defaults, strings, and data refs. `utf::table<Schema>` is the typed column store; it takes the row count from the @UTF header and reads only the schema columns. Schemas are hardcoded per file/table type. Keep the `has_default` (0x20) and `is_valid` (0x40) semantics in `raw_table` intact.
- **CPK is a container**: `cpk::archive` owns the decrypted TOC buffer and a `std::vector<cpk::entry>`; entry names are `std::string_view`s into that buffer. Lookup is indexed; iteration is over the entry vector.
- **Docs do not inventory source files**: reference targets, modules, commands, and data files;
  use `rg`/`asbrun` to discover the current layout. File lists rot quickly.

---

## Known mess / rough edges

| Issue | Area | Notes |
|-------|------|-------|
| **glxx .cpp files are empty** | `viewer/src/glxx` | Header-only wrappers; two `.cpp` files only contain an empty namespace. |
| **CSV debug dumps** | MBS parser/debug path | Debug code writes CSVs to `mbs_dump/`; should be behind a flag or removed. |
| **Root-level test archive** | repo root | Test data in repo root. Should be under `testdata/`. |
| **OTM semantics partly open** | OTM parser | Audio timeline decoded (track = MBS s9, `cue_id` = ACB cue, `frame` = track frame); event/cue params and flags unknown. |
| **NND semantics partly open** | NND parser | Graph + named-link blocks parsed (302/302); `area`, edge flags, name meta pairs and link meaning unknown. |
| **Boost iostreams legacy** | build config | `eltolinde` still links Boost::iostreams, but no source uses it. |
| **No CI** | — | Builds are local-only. No GitHub Actions or similar. |
| **Test coverage is minimal** | criware tests | Only one Catch2 test (`substream.t.cpp`). No tests for CPK, UTF, MBS, FTX, or the viewer. |
| **FetchContent + find_package duplication** | `thirdparty/*.cmake` | GLM and SDL3 try `find_package()` first, then fall back to FetchContent. Can cause version mismatches. |
| **AS host needs exact registrations** | scarlett host | A failed `RegisterObject*` sets the engine's `configFailed`, which stops `PrepareEngine` computing system-function interfaces and silently breaks value-returning natives. Skip anything already registered, register namespace-qualified types inside their namespace, and leave `asEP_INIT_GLOBAL_VARS_AFTER_BUILD` on. All 680 scripts load and run. |
| **MBS rendering conformance** | viewer sprite path | Open: per-texture UV/orientation (title colored phase), `s6` keyframe transitions, and window/prio draw ordering (title/sanctum layering). |

---

## Guidance for agents working independently

### Before making changes
1. **Build first**: `cmake --build build -j 8`
2. **Test the extractor**: `printf 'find /\nq\n' | ./build/rosalinde --cpk ~/Unicorn.CPK | wc -l`
3. **Check what you're about to touch**: Read neighboring files to understand conventions (tabs, no comments, namespace usage).

### File organization rules
- New game-format parsers go under `src/impl/` (or `src/impl/<format>/` if multi-file).
- AngelScript / `.asb` work goes under `scarlett/` (the runtime library); tools link `scarlett`.
- New CRIWARE container support goes under `libcriware/`.
- Viewer-only code stays under `viewer/src/`.
- CLI-only code stays under `tools/`.
- Header-only files are fine — the project mixes .hpp + .cpp and header-only freely.
- **Do NOT add new files to the repo root.** Move root-level binaries to `testdata/`.

### Working on different areas
- **criware library**: Medium-risk. `utf::raw_table` owns the @UTF validation and flag semantics; `utf::table<Schema>` owns the typed column projection. New schemas are welcome, but the raw table parser and CRILAYLA decoder should be changed carefully.
- **eltolinde library**: Medium-risk. The MBS/FTX parsers are reverse-engineered. Verify against the CPK test archive.
- **viewer**: Higher-risk. GL state, shaders, and animation playback are fragile. Verify visually with a GPU offscreen capture. `--script` is the only playback path; use `--scene-frame`, `--scene-fps`, `--capture-frames`, and `--trace`.
- **rosalinde**: interactive CPK shell (`ls`, `find`, `stat`, `e`, `d`, `source`). Script mode on stdin is the stable interface; see `tools/README.md`.
- **asbrun**: master ASB shell (`open`, `summary`, `scene`, `timeline`, `play`, `trace`, `record`, `calls`, `state`, `scan`, `export`). The old `asb_harvest`/`asb_run`/`asb_record`/`asb_trace` binaries are legacy; see `tools/ASBRUN_README.md`.

### Common pitfalls
- **Double +8 on string offsets**: `chunk_header` offsets need +8 adjustment (relative → absolute). `read_column_defs` already receives adjusted values. Do NOT add +8 again.
- **`has_default` / `is_valid` flags**: Columns with these flags in the UTF table store no row data. Row parsers must skip them.
- **Tabs, not spaces**: The `.clang-format` enforces tabs. If your editor inserts spaces, the linter will reject.
- **`substreambuf::xsgetn()`** had a byte-duplication bug in earlier versions. Prefer span-based parsing with `byte_reader.hpp`.

### Testing
```bash
# Build everything
cmake --build build -j 8

# Run criware unit test
./build/libcriware/criware_test

# Run scarlett unit test (the ASB decode/load/run test needs a CPK)
UNICORN_CPK=~/Unicorn.CPK ./build/scarlett/scarlett_test

# Run extractor integration test (requires Unicorn.CPK)
printf 'find /\nq\n' | ./build/rosalinde --cpk ~/Unicorn.CPK | wc -l
# Expected: 4752

# Inspect an MBS entry in memory (no extraction)
printf 'd GUI/Logo.mbs --track 9 --seq 21 --limit 8\nq\n' | ./build/rosalinde --cpk ~/Unicorn.CPK

# Inspect / trace an ASB module
printf 'open Dr_OW_0_Opening.asb\nsummary\nq\n' | ./build/tools/asb/asbrun --cpk ~/Unicorn.CPK

# Capture a deterministic frame on the GPU (no X server)
SDL_VIDEODRIVER=offscreen MESA_LOADER_DRIVER_OVERRIDE=radeonsi \
  ./build/viewer/viewer --cpk ~/Unicorn.CPK --script Script/Dr_OW_0_Opening.asb \
  --scene-frame 60 --screenshot /tmp/frame.png

# Approximate opening regression against the reference video
python3 visiontest/opening_diff.py --reference opening_cinematic.mp4
# Use --strict to fail known-open frames too.
```

---

## Docs

`docs/` holds the format and RE references. Start with `docs/ASB.md`, `docs/SECTIONS.md`
(MBS/FTX), and `docs/EXE.md` (engine tables/registration). API audits are
`docs/API_AUDIT.md`, `docs/DEFERRED_APIS.md`, `docs/API_NATIVES.tsv`, and
`docs/API_SCRIPTS.tsv`. Combat/stage planning is `docs/COMBAT.md`. CLI references are
`tools/README.md` and `tools/ASBRUN_README.md`.

The following are untracked by the user: `docs/API_AUDIT.md`, `docs/API_NATIVES.tsv`,
`docs/API_SCRIPTS.tsv`, `docs/ASB.md`, `docs/COMBAT.md`, `docs/DEFERRED_APIS.md`,
and `docs/EXE.md`.
