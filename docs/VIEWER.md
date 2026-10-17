# Viewer — Rendering, Headless Capture, and Debug Tooling

Notes on the OpenGL/SDL3 sprite viewer (`viewer/`). For the binary format itself see
[`SECTIONS.md`](SECTIONS.md); this document covers the engine-side facts: how the viewer
turns FMBS/FTX data into pixels, how to render headlessly, and the debug affordances.

## Modules

| File | Role |
|------|------|
| `main.cpp` | CLI parsing (`--cpk`, `--script`, `--scene-frame`, `--screenshot`, `--capture-frames`, `--trace`) → `ViewerConfig` → `Scene` |
| `scene.cpp/hpp` | SDL/ImGui shell; owns `AssetLoader`, `DebugUI`, and one `DramaPlayback` |
| `drama_playback.cpp/hpp` | 60 Hz script playback loop; steps `asb::RuntimeSession` and draws the live scene |
| `scene_renderer.cpp/hpp` | Reconciles live `SceneState` objects with cached sprite assets and draws them |
| `asset_library.cpp/hpp` | Cached MBS/FTX assets; async CPU prepare, GL-thread upload |
| `sprite_asset.hpp` | Shared cached `SpriteData` + `SpriteRenderer` |
| `asset_loader.cpp/hpp` | CPK open, FTX decode, asset resolution/prepare |
| `background_tracks.cpp/hpp` | Background track policy (landscape vs state families, day/night) |
| `effect_manager.cpp/hpp` | Live effect event stream and sprite instances |
| `debug_ui.cpp/hpp` | ImGui lifecycle and optional CJK font |
| `sprite_layer.hpp` | Legacy layer types still used by effects/debug helpers |
| `sprite.cpp/hpp` | `SpriteData` (parse+preprocess) and `SpriteInstance` (playback, vertices, bounds) |
| `sprite_renderer.cpp/hpp` | Texture-array upload + draw |
| `shader.cpp/hpp` | Embedded GLSL 450 vertex/fragment |
| `camera.cpp/hpp` | 2D camera (pan/zoom/fit/follow) |
| `screenshot.cpp/hpp` | PNG (zlib) / PPM writer |
| `trace_writer.cpp/hpp` | NDJSON trace sink with category filtering |
| `engine/` | `Engine` game loop + `Window` (SDL3/OpenGL RAII) |
| `glxx/` | Thin OpenGL RAII wrappers |

## Rendering pipeline

1. `DramaPlayback` loads the selected `.asb`, registers the module, and starts an
   `asb::RuntimeSession`.
2. Each scene tick advances the VM and updates the live `asb::SceneState`.
3. `SceneRenderer::sync` reconciles scene objects with `AssetLibrary` entries:
   create, update, asset swap, or destroy.
4. `AssetLibrary` extracts the MBS/FTX, decodes FTX, and uploads a texture array
   on the GL thread. CPU preparation can be requested asynchronously.
5. `SceneRenderer` advances `SpriteInstance`s by scene frames and draws them in
   stable order. Background assets expand to their selected track set.
6. `DramaPlayback` draws effects, fades, and the current dialogue text, then
   captures a screenshot when requested.

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

## Background track policy

`background_tracks.cpp::select_background_tracks` centralizes the heuristic:

- landscape elements play simultaneously;
- state families (`ENKE`/`KNKE`) select one state;
- day/night variants prefer the day set.

`SceneRenderer` expands a background object into one `SpriteInstance` per selected
track, each with its own animation clock. GUI and character assets use the
script-selected track.

## Headless capture

Render on the GPU with no X server:

```bash
SDL_VIDEODRIVER=offscreen MESA_LOADER_DRIVER_OVERRIDE=radeonsi \
  ./build/viewer/viewer --cpk ~/Unicorn.CPK \
      --script Script/Dr_OW_0_Opening.asb \
      --scene-frame 60 --screenshot /tmp/frame.png
```

Flags: `--script` selects the drama and is required; `--scene-frame <n>` advances
deterministically before capture; `--screenshot <png|ppm>` writes one frame and
exits; `--frames <n>` controls how many rendered frames precede capture.

For several deterministic frames in one process, use `--capture-frames`:

```bash
SDL_VIDEODRIVER=offscreen MESA_LOADER_DRIVER_OVERRIDE=radeonsi \
  ./build/viewer/viewer --cpk ~/Unicorn.CPK \
      --script Script/Dr_OW_0_Opening.asb \
      --capture-frames 0,60,420,480 --capture-dir /tmp/opening-ours
```

This reuses the asset cache and writes `<prefix>-<frame:06>.png` files. See
[`TOOLS.md`](TOOLS.md) for structured tracing and MBS inspection.

## Debug environment variables

| Var | Effect |
|-----|--------|
| `SCENE_TRACE=1` | Legacy stderr trace; prefer `--trace` |
| `ASB_TEXT=1` | Trace dialogue/text events |
| `VIEWER_FONT=<path>` | Override the ImGui CJK font |
| `VIEWER_ORTHO=a,b,c,d` | Override the orthographic projection |

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
