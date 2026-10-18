# AGENTS.md — Rosalinde Codebase Guide

## What this project is

A C++23 toolset for extracting, parsing, and rendering sprites from Vanillaware games (Unicorn Overlord).
Layers:

1. **criware** — CPK container + UTF table parser
2. **eltolinde** — Game-specific formats: MBS (animation), FTX (textures), FMS (string tables), NND, OTBM
3. **scarlett** — AngelScript (`.asb`) decoder + runtime (hosts modules in a stock AngelScript engine)
4. **viewer** + **extractor** — Executables (OpenGL viewer + CLI extractor) sitting on top

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
   tools/asb/* (asb_harvest, asb_run) ←── scarlett.lib
```

- **criware**: No dependencies except STL. Self-contained CPK/UTF/CRILAYLA.
- **eltolinde**: Depends on criware, Boost::iostreams (legacy), ZLIB (vestigial), glm. FTX textures are decoded on the GPU by the viewer, not here.
- **scarlett**: Depends on the vendored AngelScript (angelscript + add-ons). Decodes `.asb` and hosts modules in a stock AngelScript 2.34 engine.
- **viewer**: Depends on eltolinde, scarlett, imgui, glad, glm, Boost::program_options, SDL3, ZLIB (screenshot).
- **rosalinde** (extractor): Depends on eltolinde, Boost::program_options.

---

## Build

```bash
cmake -B build -S .
cmake --build build -j 8
```

> **Globbed sources**: the viewer lists its sources with `file(GLOB_RECURSE)`. After adding or
> removing a source file, re-run `cmake -B build -S .` or the new file will not be compiled.
> (Windows/MSVC also works: `-G "Visual Studio 17 2022"` with `--config Debug`.)

See [`docs/VIEWER.md`](docs/VIEWER.md) for rendering internals, headless capture
(`xvfb` + `llvmpipe`), and the viewer's debug environment variables.

**Requirements** (all fetched via FetchContent if not installed):
- CMake 3.28+
- A C++23 compiler (MSVC 2022 `/std:c++latest`, or GCC 14 / Clang 19)
- Boost 1.88 (system, iostreams, program_options)
- SDL3 3.4, ZLIB 1.3.1, glm 1.0, EnTT 3.14+, spdlog 1.17, OpenGL, Catch2 3.12+

The build produces:
| Target | Type | Source |
|--------|------|--------|
| `criware` | static lib | `libcriware/src/*.cpp` |
| `criware_test` | executable | `libcriware/tests/*.cpp` |
| `eltolinde` | static lib | `src/**/*.*` |
| `scarlett` | static lib | `scarlett/src/*.cpp` |
| `scarlett_test` | executable | `scarlett/tests/*.cpp` |
| `rosalinde` | executable | `tools/extractor.cpp` |
| `asb_harvest` / `asb_run` / `asb_record` / `asb_trace` | executable | `tools/asb/*.cpp` |
| `imgui` | static lib | `thirdparty/imgui-1.91.3/*.cpp` |
| `viewer` | executable | `viewer/src/**/*.*` |

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

---

## Known mess / rough edges

| Issue | Location | Notes |
|-------|----------|-------|
| **viewer engine** | `viewer/src/engine/`, `battle_state.cpp` | `Engine`/`Window` drive the loop (`main.cpp`); `BaseGame` is the game interface. `Scene` is a Menu↔Battle state machine. |
| **glxx .cpp files are empty** | `glxx/buffers.cpp`, `glxx/textures.cpp` | Only contain an empty namespace. The corresponding .hpp files are header-only. |
| **Hardcoded lookup tables** | `viewer/src/tables.cpp` | Character class + battle BG paths hardcoded (class→MBS/FTX/variants). Should move to external data (JSON). |
| **CSV debug dumps** | `src/impl/mbs/v77.cpp`, `mbs_dump/` | Debug code writes CSVs to a hardcoded path. Should be behind a flag or removed. |
| **Root-level test archive** | `Unicorn.CPK` | Test data in repo root. Should be under `testdata/`. |
| **OTM semantics partly open** | `src/impl/otm.cpp` | Audio timeline decoded (track = MBS s9, `cue_id` = ACB cue, `frame` = track frame); event/cue params and flags unknown. |
| **NND semantics partly open** | `src/impl/nnd.cpp` | Graph + named-link blocks parsed (302/302); `area`, edge flags, name meta pairs and link meaning unknown. |
| **Boost iostreams legacy** | `CMakeLists.txt` | `eltolinde` still links Boost::iostreams, but no `src/` file uses it any more. |
| **No CI** | — | Builds are local-only. No GitHub Actions or similar. |
| **Test coverage is minimal** | `libcriware/tests/` | Only one Catch2 test (`substream.t.cpp`). No tests for CPK, UTF, MBS, FTX, or the viewer. |
| **FetchContent + find_package duplication** | `thirdparty/*.cmake` | GLM and SDL3 try `find_package()` first, then fall back to FetchContent. Can cause version mismatches. |
| **AS host needs exact registrations** | `scarlett/src/host.cpp` | The hosted VM only works if registration never fails: a failed `RegisterObject*` sets the engine's `configFailed`, which stops `PrepareEngine` computing system-function interfaces and silently breaks value-returning natives. So skip anything already registered, register namespace-qualified types inside their namespace, and leave `asEP_INIT_GLOBAL_VARS_AFTER_BUILD` on. All 680 scripts load and run. |

---

## Guidance for agents working independently

### Before making changes
1. **Build first**: `cmake --build build -j 8`
2. **Test the extractor**: `./build/rosalinde --cpk ~/Unicorn.CPK --list`
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
- **viewer**: Higher-risk. The GL state, shaders, and animation playback are fragile. Verify visual output headlessly (see [`docs/VIEWER.md`](docs/VIEWER.md)) rather than assuming. Script scenes render two ways: the static fold (`--script`) and hosted-VM playback (`--runtime`, with `--scene-frame N` for deterministic capture, `--scene-fps`, `--scene-camera design|fit|first|last|<n>`).
- **tools/extractor.cpp**: CLI utility. Adding flags is safe.

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
./build/rosalinde --cpk ~/Unicorn.CPK --list | wc -l
# Expected: 4752

# Run the viewer headlessly and capture a frame (see docs/VIEWER.md)
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
  xvfb-run -a -s "-screen 0 1920x1080x24" \
  ./build/viewer/viewer --cpk ~/Unicorn.CPK --frames 1 --screenshot /tmp/frame.png
```
