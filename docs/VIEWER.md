# Viewer — Rendering, Headless Capture, and Debug Tooling

Notes on the OpenGL/SDL3 sprite viewer (`viewer/`). For the binary format itself see
[`SECTIONS.md`](SECTIONS.md); this document covers the engine-side facts: how the viewer
turns FMBS/FTX data into pixels, how to render headlessly, and the debug affordances.

## Modules

| File | Role |
|------|------|
| `main.cpp` | CLI parsing (`--cpk`, `--screenshot`, `--frames`, `--class`, `--class2`, `--bg`) → `ViewerConfig` → `Scene` |
| `scene.cpp/hpp` | `BaseGame` implementation; Menu↔Battle state machine; owns `AssetLoader` + `DebugUI` |
| `menu_state.cpp/hpp` | ImGui class/stage picker |
| `battle_state.cpp/hpp` | The scene: background + character layers, camera, animation graph, screenshot |
| `animation_graph.cpp/hpp` | Hardcoded action→track→next map (IDLE / D_WALK_A1 / MAGIC_A chain) |
| `asset_loader.cpp/hpp` | CPK open, FTX decode, `load_layer`, `load_background`, **background track selection** |
| `debug_ui.cpp/hpp` | ImGui lifecycle + debug panels |
| `sprite_layer.hpp` | `SpriteLayer` + `BackgroundScene` value types |
| `sprite.cpp/hpp` | `SpriteData` (parse+preprocess) and `SpriteInstance` (playback, `build_vertices`, bounds) |
| `sprite_renderer.cpp/hpp` | Texture-array upload + draw |
| `shader.cpp/hpp` | Embedded GLSL 450 vertex/fragment |
| `camera.cpp/hpp` | 2D camera (pan/zoom/fit/follow) |
| `screenshot.cpp/hpp` | PNG (zlib) / PPM writer |
| `tables.cpp/hpp` | Hardcoded class/background lookup tables |
| `engine/` | `Engine` game loop + `Window` (SDL3/OpenGL RAII) — driven by `main.cpp` |
| `glxx/` | Thin OpenGL RAII wrappers |

## Rendering pipeline

1. `AssetLoader` opens the CPK; for a layer/background it extracts the `.mbs` and `.ftx`.
2. FTX: `FTX::parse` → `FTX::decompress` (BC3/BC4/BC7) → `FTX::deswizzle` (Tegra X1).
3. `SpriteData::load` parses the MBS (v76/v77) and `preprocess()`es it into
   `CachedKeyframe`s (per-layer `tex_id`, `uv`, `xy`, `color`, `attributes`, `blend`) and a
   `Track` list.
4. `SpriteRenderer::upload_textures` packs every FTX texture into one
   `GL_TEXTURE_2D_ARRAY`.
5. Draw: for each sub-animation (`run`/`sa`), set `u_mvp = projection * view * s7`, build
   vertices, and issue draws **grouped by blend mode**.

### Vertex format and attributes

`SpriteVertex` is `#pragma pack(1)`:

```
int16_t  texid   // texture-array layer
vec2     uv      // texel-space UV
vec3     xyz     // local vertex position (+ z for layer depth)
uint32_t color   // RGBA
```

VAO (offsets live in `sprite_renderer.cpp::SpriteRenderer()`):

| Loc | Attribute | Type |
|-----|-----------|------|
| 0 | `texid` | `glVertexAttribIPointer(0, 1, GL_SHORT, ...)` — **integer** |
| 1 | `uv` | `glVertexAttribPointer(1, 2, GL_FLOAT, ...)` |
| 2 | `xyz` | `glVertexAttribPointer(2, 3, GL_FLOAT, ...)` |
| 3 | `color` | `glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, ...)` |

> **Pitfall — integer attributes.** The shader declares `in uint a_texid` and uses it as the
> texture-array layer index. It **must** be bound with `glVertexAttribIPointer`. Using
> `glVertexAttribPointer` (the float path) is undefined for integer inputs and, on
> Mesa/llvmpipe, produced a garbage layer index — elements sampled the wrong (usually black)
> layer, punching black rectangles through backgrounds. Characters masked this because their
> sprites use a single texture (layer 0).

### Shader (`shader.cpp`)

```glsl
// vertex: v_uv = a_uv; v_fog = a_fog; v_texid = a_texid; gl_Position = u_mvp * vec4(a_xyz,1);
// fragment:
ivec3 texsize = textureSize(u_tex, 0);
vec3  texcoord = vec3(v_uv.x/float(texsize.x), v_uv.y/float(texsize.y), float(v_texid));
fragColor = textureOffset(u_tex, texcoord, ivec2(2, 0)) * v_fog;
```

- UVs are kept in **texel space** and normalized by the array's max dimensions.
- `textureOffset(..., ivec2(2, 0))` is a pre-existing **+2-texel x** artifact-avoidance hack.
- `v_fog` is the per-vertex colour multiplied by the frame's `s7.fog` (the ARGB tint from
  the format) and any per-attribute tint.

### Texture array

`upload_textures` allocates `max_w × max_h × N` (`max_*` = max over all textures) and uploads
texture `i` at the **bottom-left** of slice `i`. Consequently `data.textures[i]` order ==
`tex_id` == array layer, and UVs (computed from each texture's own size) normalize correctly
against the array size. Verified GPU (`glGetTexImage`) == CPU decode byte-for-byte.

### Transforms

`s7` is composed as `M = T(move) · S(scale) · R(rotate)`; the renderer uses
`mvp = projection * lookAt(view) * M`.

### Blending

`s4.blend_id` selects a blend mode; the renderer does up to 4 passes per run
(0=normal, 1=additive, 2=multiply, 3=screen — mode mapping is a best guess, not confirmed).

## Background heuristics (stand-in for external game logic)

The MBS does not encode which tracks are live or how the camera frames them; the viewer
approximates it in `asset_loader.cpp::select_background_tracks` and `battle_state.cpp`:

- **Landscape** scenes → render **all** element tracks; **state/cutscene** families
  (`ENKE`/`KNKE`) → render a single `*_ENKE_IDLE` card.
- Skip day/night variants (`*_yoru` suffix, `yor*` prefix).
- Apply the `s7.fog` tint on background draws.
- Camera: character-fit by default; `BG_CAM=<track_idx>` frames a single background
  element. Background-fit framing is set aside (see the format doc's bounds caveats).

## Headless capture

The viewer can render one frame to an image and exit, which is how it is verified without a
GPU/display.

```bash
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
  xvfb-run -a -s "-screen 0 1920x1080x24" \
  ./build/viewer/viewer --cpk ~/Unicorn.CPK \
      --frames 1 --bg BGBtPlain_a --screenshot /tmp/frame.png
```

Flags: `--screenshot <png|ppm>` (PNG via zlib, or PPM), `--frames <n>` (render N frames
before capture; default 1), `--class` / `--class2` / `--bg` (screenshot-mode selection).
`imgui.ini` writing is disabled in screenshot mode.

## Debug environment variables

| Var | Effect |
|-----|--------|
| `BG_DUMP=1` | Dump selected s9 tracks + s4 flag/blend histogram |
| `BG_LAYERS=<idx>` | Dump per-run keyframe layers (tex/blend/colour/uv/xy) for one track |
| `FTX_DUMP=1` | Export decoded FTX textures to `/tmp/opencode/ftx_*.png` |
| `FTX_GPU_DUMP=1` | Read the uploaded texture array back (`glGetTexImage`) to `gpu_tex.png` |
| `BG_ONLY=<idx>` | Render a single background element in isolation |
| `BG_COLOR=1` | Tint each background element a distinct colour |
| `BG_CAM=<idx>` | Fit the camera to one background element's s9 bounds |
| `BG_NOFOG=1` | Disable the `s7.fog` tint |

## Data tables

`viewer/src/tables.cpp` maps class name → `{mbs, ftx, variants}` and background name →
`{mbs, ftx}`. The `variants` map (character name → `variant_flags`) is how a single MBS
covers multiple characters; `variant_flags` is matched against `s4.attributes`.

## Gotchas

- The viewer sources are globbed (`file(GLOB_RECURSE ...)`), so **re-run CMake**
  (`cmake -B build -S .`) after adding/removing files.
- `SpriteRenderer`/`BackgroundScene` are move-only (they own GL handles); `BackgroundScene`
  element instances must be `rebind()`-ed to the final `data` after a move.
- Verify visual changes with a headless capture rather than assuming.
