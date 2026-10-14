# AGENTS.md — Rosalinde Codebase Guide

## What this project is

A C++23 toolset for extracting, parsing, and rendering sprites from Vanillaware games (Unicorn Overlord).
Three layers:

1. **criware** — CPK container + UTF table parser
2. **eltolinde** — Game-specific formats: MBS (animation), FTX (textures), FMS (string tables), NND, ASB, OTBM
3. **viewer** + **extractor** — Two executables (OpenGL viewer + CLI extractor) sitting on top

---

## Directory layout (current, messy)

```
rosalinde/
│
├── CMakeLists.txt              # Root project. C++23, builds eltolinde/scarlett libs + rosalinde exe
│
├── libcriware/                 # CPK/UTF archive library (no game-format knowledge)
│   ├── CMakeLists.txt          # Builds criware.lib + criware_test.exe
│   ├── include/criware/
│   │   ├── cpk.hpp             # CPKSchema, CPKTable (wraps table<CPKSchema>), TopLevelCpk
│   │   ├── utf.hpp             # UTF binary parser, UTFTable<>, compile-time table<>, row_view<>
│   │   ├── acb.hpp             # ACB audio table traits (CueTable/WaveformTable/SynthTable)
│   │   ├── afs2.hpp            # AFS2 archive header
│   │   ├── substream.hpp       # Cipher-decrypting streambuf
│   │   ├── utils.hpp           # read_value<>, trim_string()
│   │   └── endian_swap.hpp     # Template endian swap
│   ├── src/
│   │   ├── cpk.cpp             # TopLevelCpk parser, CPKTable::extract
│   │   ├── utf.cpp             # @UTF binary parser (do NOT rewrite — proven, handles edge cases)
│   │   ├── acb.cpp             # ACB deserialization
│   │   ├── afs2.cpp            # AFS2 parser
│   │   └── crilayla.cpp        # CRILAYLA compression
│   └── tests/
│       └── substream.t.cpp     # Catch2 unit test for substreambuf
│
├── src/                        # eltolinde library (game format parsers)
│   ├── eltolinde.hpp           # Umbrella header: includes fms.hpp, ftx.hpp
│   └── impl/
│       ├── mbs/
│       │   ├── sections.hpp    # All v77 section structs (s0..sb) + file_header + glm types
│       │   ├── detail.hpp      # Shared parsing + parse_v76/parse_v77 entry points
│       │   ├── csv_dump.hpp    # CSV debug dump of parsed sections
│       │   ├── v76.cpp         # v76 parser (populates mbs::v77 struct)
│       │   └── v77.cpp         # v77 parser + s7_matrix()
│       ├── ftx.hpp             # FTEX/FTX0 texture archive declarations
│       ├── ftx.cpp             # FTEX parsing + BC3/BC4/BC7 decompress + Tegra deswizzle
│       ├── fms.hpp             # FMSB string table parser (header-only)
│       ├── nnd.hpp             # NND parser (array of uint32, header-only)
│       ├── otm.hpp / otm.cpp   # OTBM file format (stubbed)
│       └── byte_reader.hpp     # Zero-dependency span cursor for binary parsing
│
├── scarlett/                   # AngelScript runtime library (decoder + hosted VM)
│   ├── CMakeLists.txt          # Builds scarlett (links: angelscript_addon)
│   ├── include/scarlett/
│   │   ├── asb.hpp             # .asb module decoder (Module/Signature/Instruction/SceneIR)
│   │   └── host.hpp            # Hosts a decoded module in a stock AngelScript 2.34 engine
│   └── src/
│       ├── asb.cpp             # Parser + disassembler + scene extraction (SceneIR)
│       └── host.cpp            # Engine setup + module import registration + load/run
│
├── tools/
│   ├── extractor.cpp           # CLI extractor. --list, --extract, --columns, --acb, --ftx, --fms, --mbs
│   └── asb/                    # asb_harvest (analysis/generators) + asb_run (host CLI); link scarlett
│
├── viewer/                     # OpenGL 4.6 / SDL3 sprite viewer
│   ├── CMakeLists.txt          # Builds viewer (links: eltolinde, glad, glm, Boost, SDL3, ZLIB)
│   └── src/
│       ├── main.cpp            # Entry point, CLI parsing (--cpk/--screenshot/--frames/--class/--bg)
│       ├── scene.cpp/hpp       # BaseGame impl: Menu↔Battle state machine
│       ├── menu_state.cpp/hpp  # ImGui class/stage picker
│       ├── battle_state.cpp/hpp # Scene: background + character layers, camera, screenshot
│       ├── animation_graph.*   # Hardcoded action→track map (IDLE / D_WALK / MAGIC_A chain)
│       ├── asset_loader.*      # CPK/FTX loading + background track selection
│       ├── debug_ui.cpp/hpp    # ImGui lifecycle + debug panels
│       ├── sprite_layer.hpp    # SpriteLayer / BackgroundScene value types
│       ├── sprite.cpp/hpp      # SpriteData parse/preprocess + SpriteInstance playback
│       ├── sprite_renderer.*   # Texture-array upload + blend-grouped draw
│       ├── shader.cpp/hpp      # Embedded GLSL 450 keyframe shader
│       ├── camera.cpp/hpp      # 2D camera (pan/zoom/fit/follow)
│       ├── screenshot.cpp/hpp  # PNG (zlib) / PPM writer
│       ├── tables.cpp/hpp      # ~85 char class + ~170 battle BG lookup tables
│       ├── engine/
│       │   ├── Engine.cpp/hpp  # Game loop driver (driven by main.cpp)
│       │   └── Window.cpp/hpp  # SDL3/OpenGL window RAII wrapper
│       └── glxx/               # "GL c++ extensions" — thin OpenGL RAII wrappers
│           ├── object.hpp      # CRTP base for GL object handles
│           ├── buffers.hpp/cpp # GL buffer wrapper
│           ├── shader.hpp/cpp  # GL shader program wrapper
│           ├── texture.hpp     # 3D / 2DArray texture wrapper
│           ├── textures.hpp/cpp # 2D / cubemap texture wrapper
│           └── error.cpp/hpp   # GL debug callback + error checking
│
├── thirdparty/
│   ├── CMakeLists.txt          # FetchContent for detex, glad, glm, SDL3, entt + builds imgui
│   ├── imgui-1.91.3/           # Dear ImGui (9 files + SDL3/OpenGL backends)
│   ├── detex.cmake             # detex 0.1.2 (BCn block decompression)
│   ├── glad.cmake              # GLAD OpenGL loader
│   ├── glm.cmake               # GLM 1.0.2 (fallback)
│   ├── SDL3.cmake              # SDL3 3.4.0 (fallback)
│   └── entt.cmake              # EnTT 3.15.0 (ECS, currently unused)
│
├── docs/                       # Format + project documentation
│   ├── SECTIONS.md             # FMBS/v77 format reference (characters + backgrounds)
│   ├── VIEWER.md               # Viewer rendering, headless capture, debug tooling
│   ├── UTF.md                  # @UTF container notes
│   └── ACB_AFS2.md             # ACB / AFS2 notes
│
├── README.md                   # Brief overview
├── LICENSE
├── .clang-format               # tabs, 120col, left pointer alignment (LLVM base)
├── .gitignore                  # Ignores build/, .vscode/, object files, executables
├── cleanbuild.sh               # Shell script to clean the build directory
├── mbs_dump/                   # CSV dumps from the v77 parser
└── Unicorn.CPK                 # Test archive (root-level; ideally under testdata/)
```

---

## Dependency graph

```
              OpenGL / GLAD
                   ↑
       SDL3 ←─────┴────── ImGui
        ↑                   ↑
   viewer.exe ←── imgui ────┘
        ↑
        ├── eltolinde.lib ←── criware.lib ←── ZLIB (via detex)
        └── scarlett.lib  ←── angelscript.lib (vendored 2.34.0)

   rosalinde.exe (extractor) ←── eltolinde.lib
   tools/asb/* (asb_harvest, asb_run) ←── scarlett.lib
```

- **criware**: No dependencies except STL. Self-contained CPK/UTF/CRILAYLA.
- **eltolinde**: Depends on criware, detex (BCn decompress), Boost::iostreams (legacy), ZLIB, glm.
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
- SDL3 3.4, ZLIB 1.3.1, glm 1.0, EnTT 3.14+, OpenGL, Catch2 3.12+

The build produces:
| Target | Type | Source |
|--------|------|--------|
| `criware` | static lib | `libcriware/src/*.cpp` |
| `criware_test` | executable | `libcriware/tests/*.cpp` |
| `eltolinde` | static lib | `src/**/*.*` |
| `scarlett` | static lib | `scarlett/src/*.cpp` |
| `rosalinde` | executable | `tools/extractor.cpp` |
| `imgui` | static lib | `thirdparty/imgui-1.91.3/*.cpp` |
| `viewer` | executable | `viewer/src/**/*.*` |

---

## Key conventions

- **C++23**: Uses `std::span`, `std::expected` (future), fold expressions, concepts, `fixed_string` NTTP
- **Tabs for indentation** (`.clang-format`: `UseTab: Always`)
- **No comments** in code (unless documenting complex format logic)
- **Namespaces**: `criware::`, `mbs::`, `sprite::`, `glxx::`, `ftx::`, `fms::`, `nnd::`, `asb::`, `otm::`
- **String type**: `std::string` everywhere (no `wstring`, no `u8string`)
- **Parsing style**: Span-based with `byte_reader.hpp` (preferred). Iostream is legacy wrapper.
- **GL version**: OpenGL ES 4.6 core (GLAD). Shaders are GLSL ES 3.20.
- **The `utf.cpp` is sacred**: `UTF::operator>>` handles `has_default` (0x20), `is_valid` (0x40), string-offset resolution, and endian-swap correctly. The new `table<>` delegates to it. Do NOT rewrite that function.

---

## Known mess / rough edges

| Issue | Location | Notes |
|-------|----------|-------|
| **viewer engine** | `viewer/src/engine/`, `battle_state.cpp` | `Engine`/`Window` drive the loop (`main.cpp`); `BaseGame` is the game interface. `Scene` is a Menu↔Battle state machine. |
| **glxx .cpp files are empty** | `glxx/buffers.cpp`, `glxx/textures.cpp` | Only contain an empty namespace. The corresponding .hpp files are header-only. |
| **Hardcoded lookup tables** | `viewer/src/tables.cpp` | Character class + battle BG paths hardcoded (class→MBS/FTX/variants). Should move to external data (JSON). |
| **CSV debug dumps** | `src/impl/mbs/v77.cpp`, `mbs_dump/` | Debug code writes CSVs to a hardcoded path. Should be behind a flag or removed. |
| **Root-level test archive** | `Unicorn.CPK` | Test data in repo root. Should be under `testdata/`. |
| **Incomplete parsers** | `src/impl/otm.cpp` | Mostly a stub. |
| **Boost iostreams legacy** | `CMakeLists.txt` | `eltolinde` still links Boost::iostreams, but no `src/` file uses it any more. |
| **No CI** | — | Builds are local-only. No GitHub Actions or similar. |
| **Test coverage is minimal** | `libcriware/tests/` | Only one Catch2 test (`substream.t.cpp`). No tests for CPK, UTF, MBS, FTX, or the viewer. |
| **FetchContent + find_package duplication** | `thirdparty/*.cmake` | GLM and SDL3 try `find_package()` first, then fall back to FetchContent. Can cause version mismatches. |

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
- **criware library**: Low-risk. The only invariant is that `utf.cpp` must not be rewritten. New `table<>` schemas are welcome.
- **eltolinde library**: Medium-risk. The MBS/FTX parsers are reverse-engineered. Verify against the CPK test archive.
- **viewer**: Higher-risk. The GL state, shaders, and animation playback are fragile. Verify visual output headlessly (see [`docs/VIEWER.md`](docs/VIEWER.md)) rather than assuming.
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

# Run extractor integration test (requires Unicorn.CPK)
./build/rosalinde --cpk ~/Unicorn.CPK --list | wc -l
# Expected: 4752

# Run the viewer headlessly and capture a frame (see docs/VIEWER.md)
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
  xvfb-run -a -s "-screen 0 1920x1080x24" \
  ./build/viewer/viewer --cpk ~/Unicorn.CPK --frames 1 --screenshot /tmp/frame.png
```
