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
├── CMakeLists.txt              # Root project. C++23, builds eltolinde lib + rosalinde exe
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
│   ├── eltolinde.hpp           # Umbrella header: includes mbs.hpp, fms.hpp, ftx.hpp
│   └── impl/
│       ├── mbs.hpp             # MBS animation parser interface (MBS class)
│       ├── mbs.cpp             # Header parsing, v76/v77 dispatch
│       ├── mbs/
│       │   ├── sections.hpp    # All v77 section structs (s0..sb) + glm types
│       │   ├── detail.hpp      # Shared parsing: lookup tables, resolve_table, populate_sections
│       │   ├── v76.cpp         # v76 parser (populates mbs::v77 struct)
│       │   └── v77.cpp         # v77 parser + CSV debug dumper + s7_matrix()
│       ├── ftx.hpp             # FTEX/FTX0 texture archive declarations
│       ├── ftx.cpp             # FTEX parsing + BC3/BC4/BC7 decompress + Tegra deswizzle
│       ├── fms.hpp             # FMSB string table parser (header-only)
│       ├── nnd.hpp             # NND parser (array of uint32, header-only)
│       ├── asb.hpp / asb.cpp   # ASB compiled script (stubbed)
│       ├── otm.hpp / otm.cpp   # OTBM file format (stubbed)
│       └── byte_reader.hpp     # Zero-dependency span cursor for binary parsing
│
├── tools/
│   └── extractor.cpp           # CLI extractor. --list, --extract, --columns, --acb, --ftx, --fms, --mbs
│
├── viewer/                     # OpenGL 4.6 / SDL3 sprite viewer
│   ├── CMakeLists.txt          # Builds viewer.exe (links: eltolinde, imgui, glad, glm, Boost)
│   └── src/
│       ├── main.cpp            # Entry point, CLI args, window setup, main loop
│       ├── state.cpp/hpp       # State: CPK I/O, sprite loading, render orchestrator
│       ├── sprite.cpp/hpp      # Sprite: GL buffers, texture array, animation playback
│       ├── shader.cpp/hpp      # Embedded GLSL ES 3.20 keyframe shader
│       ├── camera.cpp/hpp      # 2D camera (pan/zoom)
│       ├── tables.cpp/hpp      # ~85 char class + ~170 battle BG hardcoded lookup tables
│       ├── game.cpp/hpp        # Vestigial Game class (mostly empty, unused)
│       ├── engine/
│       │   ├── Engine.cpp/hpp  # Game loop driver
│       │   └── Window.cpp/hpp  # SDL3/OpenGL window RAII wrapper
│       └── glxx/              # "GL c++ extensions" — thin OpenGL RAII wrappers
│           ├── object.hpp      # CRTP base for GL object handles
│           ├── buffers.hpp/cpp # GL buffer wrapper
│           ├── shader.hpp/cpp  # GL shader program wrapper
│           ├── texture.hpp     # 3D / 2DArray texture wrapper
│           ├── textures.hpp/cpp # 2D / cubemap texture wrapper
│           └── error.hpp/cpp   # GL debug callback + error checking
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
├── sprite.hpp                  # WIP next-gen Sprite API (NOT compiled, NOT integrated)
│                               # Has its own Config, pre-processing, Vulkan/GL paths
│
├── SECTIONS.md                 # Deep dive into v77 animation format (~315 lines)
├── README.md                   # Brief overview
├── .clang-format               # tabs, 120col, left pointer alignment
├── .gitignore                  # Ignores build/, .vscode/, object files, executables
├── cleanbuild.sh               # Shell script to clean the build directory
├── tegra_deswizzle.comp        # GLSL compute shader for Tegra X1 texture deswizzle
├── quad_vanillaware_FMBP_FMBS.php  # PHP reference script for reverse engineering
│
├── AcfReferenceTable.bin       # CRIWARE audio reference data
├── CueNameTable.bin            # CRIWARE cue name data
├── bcm.acb.log                 # ACB log
│
└── csv/                        # Debug CSV dumps from v77 parser
    ├── section_8.csv           # Animation frames
    ├── section_9.csv           # Tracks
    └── section_a.csv           # Sequences
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
   eltolinde.lib ←── criware.lib ←── ZLIB (via detex)
        ↑
   rosalinde.exe (extractor)
```

- **criware**: No dependencies except STL. Self-contained CPK/UTF/CRILAYLA.
- **eltolinde**: Depends on criware, detex (BCn decompress), Boost::iostreams (legacy), ZLIB, glm.
- **viewer**: Depends on eltolinde, imgui, glad, glm, Boost::program_options.
- **rosalinde** (extractor): Depends on eltolinde, Boost::program_options.

---

## Build

```bash
cmake -B build -S . -G "Visual Studio 17 2022"
cmake --build build --config Debug -j 8
```

**Requirements** (all fetched via FetchContent if not installed):
- CMake 3.28+
- MSVC 2022 with C++23 (`/std:c++latest`)
- Boost 1.88 (system, iostreams, program_options)
- SDL3 3.4, ZLIB 1.3.1, glm 1.0, EnTT 3.14+, OpenGL, Catch2 3.12+

The build produces:
| Target | Type | Source |
|--------|------|--------|
| `criware` | static lib | `libcriware/src/*.cpp` |
| `criware_test` | executable | `libcriware/tests/*.cpp` |
| `eltolinde` | static lib | `src/**/*.*` |
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
| **sprite.hpp at root** | `sprite.hpp` | New Sprite API WIP. Not in any CMakeLists.txt. Not compiled. Conflicts with `viewer/src/sprite.hpp`. |
| **viewer engine unused** | `viewer/src/game.cpp/hpp`, `engine/` | The `Engine`/`BaseGame`/`Game` classes exist but `main.cpp` drives the loop inline. Vestigial. |
| **glxx .cpp files are empty** | `glxx/buffers.cpp`, `glxx/textures.cpp` | Only contain an empty namespace. The corresponding .hpp files are header-only. |
| **Hardcoded lookup tables** | `viewer/src/tables.cpp` | ~250 lines of hardcoded character class and battle BG paths. Should move to external data (JSON). |
| **CSV debug dumps** | `src/impl/mbs/v77.cpp`, `csv/` | Debug code writes CSVs to a hardcoded path. Should be behind a flag or removed. |
| **Root-level binary files** | `AcfReferenceTable.bin`, `CueNameTable.bin`, `bcm.acb.log` | Test data mixed into repo root. Should be under `testdata/` |
| **Incomplete parsers** | `asb.cpp`, `otm.cpp` | Mostly stubs. |
| **Two competing Sprite classes** | `sprite.hpp` (root) vs `viewer/src/sprite.hpp` | The root one is the intended replacement but hasn't been integrated yet. |
| **Boost iostreams legacy** | `src/impl/mbs.cpp` | The istream path in `mbs.cpp` wraps the span path. Boost::iostreams is only needed for legacy code paths. |
| **No CI** | — | Builds are local-only. No GitHub Actions or similar. |
| **Test coverage is minimal** | `libcriware/tests/` | Only one Catch2 test (`substream.t.cpp`). No tests for CPK, UTF, MBS, FTX, or the viewer. |
| **FetchContent + find_package duplication** | `thirdparty/*.cmake` | GLM and SDL3 try `find_package()` first, then fall back to FetchContent. Can cause version mismatches. |

---

## Guidance for agents working independently

### Before making changes
1. **Build first**: `cmake --build build --config Debug -j 8`
2. **Test the extractor**: `./build/Debug/rosalinde.exe --cpk "C:/Users/undis/Downloads/ryujinx/Games/Unicorn.CPK" --list`
3. **Check what you're about to touch**: Read neighboring files to understand conventions (tabs, no comments, namespace usage).

### File organization rules
- New game-format parsers go under `src/impl/` (or `src/impl/<format>/` if multi-file).
- New CRIWARE container support goes under `libcriware/`.
- Viewer-only code stays under `viewer/src/`.
- CLI-only code stays under `tools/`.
- Header-only files are fine — the project mixes .hpp + .cpp and header-only freely.
- **Do NOT add new files to the repo root.** Move root-level binaries to `testdata/`.

### Working on different areas
- **criware library**: Low-risk. The only invariant is that `utf.cpp` must not be rewritten. New `table<>` schemas are welcome.
- **eltolinde library**: Medium-risk. The MBS/FTX parsers are reverse-engineered. Verify against the CPK test archive.
- **viewer**: Higher-risk. The GL state, shaders, and animation playback are fragile. Verify visual output.
- **tools/extractor.cpp**: CLI utility. Adding flags is safe.

### Common pitfalls
- **Double +8 on string offsets**: `chunk_header` offsets need +8 adjustment (relative → absolute). `read_column_defs` already receives adjusted values. Do NOT add +8 again.
- **`has_default` / `is_valid` flags**: Columns with these flags in the UTF table store no row data. Row parsers must skip them.
- **Tabs, not spaces**: The `.clang-format` enforces tabs. If your editor inserts spaces, the linter will reject.
- **`substreambuf::xsgetn()`** had a byte-duplication bug in earlier versions. Prefer span-based parsing with `byte_reader.hpp`.

### Testing
```bash
# Build everything
cmake --build build --config Debug -j 8

# Run criware unit test
./build/libcriware/Debug/criware_test.exe

# Run extractor integration test (requires Unicorn.CPK)
./build/Debug/rosalinde.exe --cpk "C:/Users/undis/Downloads/ryujinx/Games/Unicorn.CPK" --list | wc -l
# Expected: 4752

# Run viewer (requires CPK)
./build/viewer/Debug/viewer.exe --cpk "C:/Users/undis/Downloads/ryujinx/Games/Unicorn.CPK" --list
```
