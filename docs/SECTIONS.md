# FMBS / V77 Data Analysis

## Overview

This document describes the `FMBS` animation/asset format and its v76/v77 section layout.
The evidence is drawn from real game data — character sprites (`Scarlet_F.mbs`, v77, ~333
tracks) and background scenes (`BGBtPlain_a.mbs`, `BGEvForest_a.mbs`, `BGBtCathedral.mbs`,
`Classpedia_BG.mbs`) — together with observations from the reference viewer.

The **same 12-section container** is used for two quite different flavours of asset:

- **Character sprites** — a palette of named combat/dialogue animations; exactly **one
  track is active** at a time, containing several sub-animations (body parts) that play
  together.
- **Background scenes** — either a set of landscape elements that all animate
  **simultaneously**, or a small **state machine** of mutually-exclusive states
  (cutscene/set-piece scenes such as the Classpedia class-change).

The format gives us the building blocks (tracks, sequences, frames, keyframes, layers,
transforms, textures); **which tracks are live and how the camera frames them is external
game logic, not stored as data.**

## Field naming convention

CSV column names map 1:1 to C++ struct fields in `src/impl/mbs/sections.hpp`.
Where the struct has a named field, the CSV uses the struct name (e.g. `track_id`).
Where the struct uses `_unkN`, the CSV mirrors that.

S9 bounding boxes are represented as `left, top, right, bottom` (float) — these vary
per animation and serve as camera framing hints, not sprite bounds.

---

## File structure

Header (`file_header`, `#pragma pack(1)`):

```
magic[4]      "FMBS"
file_size     u32
header_size   u32
_pad0         u32
_unk0         u16
_unk1         u16
version       u16   0x76 or 0x77
_unk2         u16
_unk3         u32
_pad1         u32
```

The header is followed by a **descriptor table** for the 12 sections. Each descriptor is
`(count_offset, count_size, data_ptr_offset, row_length)`: read the row count at
`count_offset` (a `u16` or `u32`), the absolute file offset at `data_ptr_offset` (a `u32`
pointer), and each row is `row_length` bytes. A count/pointer of `0` means the section is
absent.

| Idx | Name | Row struct | Row len | Contents |
|-----|------|------------|---------|----------|
| 0 | `s0` | `section_0` | 0x18 | Fog/fragment colours (4×RGBA) |
| 1 | `s1` | `section_1` | 0x30 | Texture UV coords |
| 2 | `s2` | `section_2` | 0x30 | Vertex positions (quad corners) |
| 3 | `s3` | `section_3` | 0x50 | Hitboxes (**characters only**) |
| 4 | `s4` | `section_4` | 0x14 | Keyframe layers (tex/uv/vertex/colour refs) |
| 5 | `s5` | `section_5` | 0x08 | Hitbox entries (**characters only**) |
| 6 | `s6` | `section_6` | 0x1c | Keyframes (bounds + layer range) |
| 7 | `s7` | `section_7` | 0x24 | Transforms (move/rotate/scale/fog) |
| 8 | `s8` | `section_8` | 0x20 | Animation frames |
| 9 | `s9` | `section_9` | 0x30 | Tracks |
| a | `sa` | `section_a` | 0x18 | Sequences |
| b | `sb` | `section_b` | 0x14 | Timing modifiers (**characters only**) |

`version = 0x76` files are parsed into the same v77 in-memory model.

---

## Section 9 (s9) — Tracks

**Struct:** `left(top right bottom)[4], name[24], sa_set_id(u16), sa_set_no(u8),
sa_set_main(u8), sa_sb_set_id(u16), sa_sb_set_no(u8), disabled(u8)`

| Field | Meaning |
|-------|---------|
| `left,top,right,bottom` | Camera framing hint for this track (not sprite bounds — vary wildly) |
| `name` | Track identifier |
| `sa_set_id` / `sa_set_no` | Starting index / count of `section_a` sequences |
| `sa_set_main` | 1 on "middle" phase tracks of compound animations (KNOCKBACK, MAGIC_B_*, dialogue `_A2` phases) |
| `sa_sb_set_id` / `sa_sb_set_no` | Track-level timing-modifier range into `section_b` (**characters only**) |
| `disabled` | 1 = reserved/padding slot; skip |

### Track naming — character sprites

**Battle actions:** `IDLE`, `TURN`, `UPWARD`, `UPWARD_IDLE`, `UPWARD_END`, `DOWN`,
`DOWN_IDLE`, `DOWN_END`, `GUARD`, `AVOID`, `KNOCKBACK`, `DASH_1`, `DASH_2`

**Magic/attacks (4 phases + end):** `MAGIC_A_1` → `MAGIC_A_2` → `MAGIC_A_3` → `MAGIC_A_4`
→ `MAGIC_A_END` (same pattern for B, C, D)

**Character-specific skill overrides:** `S01_`/`S02_`/`S03_` (MAGIC_A variants),
`S05_`/`S06_` (MAGIC_B), `S07_` (MAGIC_C), `S18_QUICK_HEAL`, `S19_LISTED`, `S22_CHASE`,
`S23_COUNTER`, `S24_ACCUMULATE`, `S25_EX1`

**Dialogue animations (`D_` prefix):**
- Idles: `D_IDLE_A`..`D_IDLE_F` (6 directional faces)
- Transitions: `D_IDLE_A_TO_F`, `D_IDLE_F_TO_A`, ...
- Movement: `D_WALK_A1/A2`, `D_WALK_B1/B2`, `D_WALK_D1/D2`, `D_DASH_*`, `D_WALK_BACK_*`
- Expressions: `D_SURPRISED`, `D_NOD`, `D_SIGH`, `D_LAUGH`, `D_DENY`, `D_ANGER`, `D_THINK`
- Gestures: `D_EXPLAIN`, `D_HAND`, `D_GUTS_POSE`
- Eye/face: `D_LOOK_IDLE`, `D_LOOK_TURN`, `D_LOOK_OVER`, `D_LOOK_FRONT`, `D_LOOK_UP`, `D_LOOK_AWAY`
- Postures: `D_SIT`, `D_SIT_FRONT`, `D_SIT_PRAY`, `D_DOWN`, `D_DOWN_SOFT`, `D_DOWN_UPWARD`
- Special: `D_VICTORY`, `D_DIE`
- **`FACEBATTLE`** — close-up dialogue camera (BB ±400×455)

### Track naming — background scenes

Two families, distinguished by the naming and by whether tracks are *layers* or *states*.

**Landscape elements** (romaji names) — all play simultaneously:

| Pattern | Meaning | Examples |
|---------|---------|----------|
| `kumo_` | Cloud | kumo_01, kumo_02 |
| `yama_` | Mountain | yama_01 |
| `oka_` | Hill | oka_01.. |
| `mori_` | Forest | mori_01 |
| `iwayama_` | Rocky cliff | iwayama_01.. |
| `gake_` | Cliff face | gake_01, gake_02 |
| `sougen_` | Meadow | sougen_01..05 |
| `ki_` | Tree | ki_01, ki_02 |
| `haikyo_` | Ruins | haikyo_01 |
| `gareki_` | Debris | gareki_01, gareki_02 |
| `eda_` | Branch | eda_01, eda_02 |
| `iwa_` | Rock | iwa_01..06 |
| `kareki_` | Dead tree | kareki_01 |
| `koiwa_` | Small rock | koiwa_01, koiwa_02 |
| `kusamiti_` | Grass path | kusamiti_01.. |
| `kusa_` | Grass tuft | kusa_01..10 |
| `tobukusa_` | Flying grass | tobukusa_01 |
| `effect_` | VFX | effect_01..03 |
| `sora_`/`zimen_` | Sky / ground | (town scenes) |
| `mizu`/`mizutamari` | Water / puddle | |

**Cutscene / state families** — mutually exclusive; only one state renders:

- `CLS_*_{ENKE,KNKE}_{IDLE,OPEN}` with `BFR`/`AFTR`/`INST`/`VWR` prefixes. `ENKE` ≈ 遠景
  (far view = the coloured "card"), `KNKE` ≈ 近景 (near view = dark silhouette). Rendering
  both composites the silhouette over the card and looks wrong.
- `VWR`/`PSTN` tracks are UI chrome / position markers.

### Track selection and variants

A background is a *set* of tracks, and only part of the set is live at once:

- **Landscape**: render **every** element track; they are independent layers of one scene.
- **State scenes**: pick **one** state (e.g. a single `*_ENKE_IDLE` card), never the whole
  family.
- **Day/night variants** appear two ways: a suffix `*_yoru` (dodai00_yoru) and a prefix
  `yor*` (yoru01). Prefer the day set.
- An `_1` suffix (oka_4_1, hikari_00_1, akari_02_1) is usually an **extra instance** of an
  element at a different placement, not a variant — keep it.

### `sa_set_main` pattern (characters)

Set to 1 on "middle" phase tracks: `KNOCKBACK`, `MAGIC_B_1/2`, `S05/S06 MAGIC_B`,
`VICTORY_1/2`, `D_IDLE_A–F`, `D_SURPRISED_A2`/`D_EXPLAIN_A2`/`D_HAND_A2`/`D_GUTS_POSE_A2`,
`FACEBATTLE`.

### `sa_sb_set_id` linkage to section B (characters)

Non-zero only on tracks needing timing modifiers (hitstop/speed curves): `DASH_1`,
`MAGIC_A_3`, `MAGIC_B_END`, `LISTED`/`ORDER_1`/`STANCE_1`, `S01–S03 MAGIC_A_3`,
`S05–S06 MAGIC_B_END`, `S18_QUICK_HEAL_1`/`S19_LISTED`, and the `D_WALK`/`D_DASH`
directionals. `sa_sb_set_no = 1` means one `section_b` entry is used.

---

## Section A (sa) — Animation Sequences

**Struct:** `s8_id(u16), s8_no(u16), s8_sum(u32), s8_sum_once(i32), commit_ticks(i32),
sb_id(u16), sb_no(u8), s8_st(u8), track_id(u16), _pad(u16)`

| Field | Meaning |
|-------|---------|
| `s8_id` | Starting index into `section_8` |
| `s8_no` | Number of `section_8` frames in this sequence |
| `s8_sum` | Total tick sum of all frame durations |
| `s8_sum_once` | Intro ticks played once before looping. `-1` = none; `0` = whole sequence loops |
| `commit_ticks` | Uninterruptible window (characters). `-1` = none |
| `sb_id` / `sb_no` | Per-sequence timing modifier (characters) |
| `s8_st` | **Dual-purpose** — see below |
| `track_id` | Index of this sequence within its parent track; gaps = reserved slots |

### Two-phase playback model (characters)

```
[s8_id ........ s8_id+s8_no-1]   (s8_no frames spanning s8_sum ticks)
 |--- intro ---|------ loop body ------|
 |  guarded by   |  interruptible after  |
 | s8_sum_once   |       commit_ticks     |
```

| s8_sum_once | commit_ticks | Interpretation |
|-------------|--------------|----------------|
| -1 | -1 | Single-pass sequence, no loop/commit (transitional) |
| 0 | -1 | Full-loop, no intro |
| >0 | = s8_sum_once | Simple attack loop: N ticks once, rest loops |
| >0 | ≠ s8_sum_once | Complex attack with section_b timing (e.g. MAGIC_A_3: 99 intro / 59 commit) |

Backgrounds never use this: `s8_sum_once = -1` on all background sequences.

### Example: character IDLE and MAGIC_A

```
IDLE  track_id=0: s8_no=55 s8_sum=432 s8_sum_once=0   (6 sub-blocks × 72 ticks)
MAGIC_A_1: s8_sum=102 s8_sum_once=18
MAGIC_A_2: s8_sum=102 s8_sum_once=18
MAGIC_A_3: s8_sum=155 s8_sum_once=99 commit_ticks=59 sb_id=2 s8_st=1
MAGIC_A_4: s8_sum=96  s8_sum_once=40
MAGIC_A_END: s8_sum=171 s8_sum_once=27 commit_ticks=27 s8_st=1
```

### `s8_st` — dual semantics

**Characters (`s8_st ∈ {0,1}`): "skip first frame".** `1` skips the first frame (a generic
body keyframe shared across many animations) and starts at `s8_id+1`.

**Backgrounds (`s8_st ∈ {0..9}`): "sub-sprite index".** An element such as `kusa_01` can
have sub-sprites at `s8_st = 0,1,2,3,4`, each a visual component with its own loop,
sharing the same texture regions but animating at its own cadence (natural sway).

---

## Section 8 (s8) — Animation Frames

**Struct:** `s6_id(u16), _pad0(u16), s7_id(u16), frames(u16), flags(u32),
loop_s8_id(u16), s5s3_interp(u8), interp_rate(u8), s7_interp(u8), s6_interp(u8),
s0s1s2_interp(u8), n_180(u8), _pad1(u32), _pad2(u16), sfx_mute(u16), sfx_id(u32)`

### Flag bits (u32)

| Bit | Name | Meaning |
|-----|------|---------|
| 0x01 | FLIPX | Horizontal mirror (directional variant key) |
| 0x02 | FLIPY | Vertical mirror (rare) |
| 0x04 | JUMP | Control-flow: the frame after this one is at `current + loop_s8_id` |
| 0x20 | ACTIVE | Frame is emitted to the renderer (clear on hitbox-only / transition frames) |
| 0x80 | POSE | Non-idle body pose (GUARD, KNOCKBACK, DOWN, SIT) |
| 0x400 | HITBOX | Hitbox-only frame (no sprite layers) |
| 0x800 | LAST | End-of-sequence marker; with JUMP it is the loop point |
| 0x2000 | — | Rarely set; purpose unclear |

### JUMP + loop_s8_id

`loop_s8_id` is a **relative** offset when `JUMP=1`: `next = current + loop_s8_id`
(e.g. D_WALK loops back 17 frames).

### Interpolation fields

| Field | Range | Effect |
|-------|-------|--------|
| `s5s3_interp` | 0–7 | Hitbox interpolation |
| `interp_rate` | 0–2 | Interpolation frequency |
| `s7_interp` | 0–2 | Transform interpolation |
| `s6_interp` | 0–1 | Keyframe layer transition |
| `s0s1s2_interp` | 0–2 | Colour/UV/vertex interpolation |

Tiers: none (all 0), light (`s7_interp=1`, `s5s3_interp=1`), heavy
(`s7_interp=2`, `s0s1s2_interp=2`).

### Other fields

- `n_180`: 0–24, correlates with 180° turn frames.
- `sfx_mute` / `sfx_id`: mostly 0; frame-level sound-effect trigger.

---

## Section 7 (s7) — Transforms

**Struct:** `move(vec3), rotate(vec3), scale(vec2), fog(u32)`

The transform is composed, applied to a vertex, as **scale → translate → rotate**:

```
M = T(move) · S(scale) · R(eulerAngleXYZ(rotate))
```

### `fog` is a per-frame ARGB colour tint

`fog` is a packed `0xAARRGGBB` **colour** multiplied into the sprite (vertex colour),
where the alpha byte is an opacity/strength. It is **not** a pure depth key. Verified
examples from backgrounds:

| Track / layer | `fog` | Effect when multiplied |
|---------------|-------|------------------------|
| most elements | `FFFFFFFF` | neutral (no change) |
| `isu00` shadow sub-sprites | `B4000000` | near-black, α≈0.70 → soft shadow |
| `hikari00` | `1EFFFFFF` | white, α≈0.12 → faint light shaft |
| `hikari01` | `32DD6924` | orange, α≈0.20 → warm glow |
| `dodai01_1` | `FF5280E9` | opaque blue tint |
| `kumo_01` | `FFE0CDC5` | warm beige (atmospheric tint) |

**Rendering implication:** ignoring `fog` turns shadow/light sub-sprites into opaque
bright blobs and light shafts into hard streaks. Multiplying it in is essential.

The earlier "fog alpha = binary depth marker with a fixed compositing order" hypothesis
did **not** hold up — alpha varies continuously and is used per-frame, not as a stable
depth tier. See *Remaining unknowns* for the (still-open) question of how the engine
orders background layers.

---

## Section 6 (s6) — Keyframes

**Struct:** `left, top, right, bottom (float), s4_id(u16), s5_id(u16), s4_no(u16),
s5_no(u8), flags(u8), _pad0(u16)`

A keyframe = a bounding box plus a contiguous range of `section_4` layers and `section_5`
hitbox entries. `left/top/right/bottom` are *bounds*, but they are not reliable content
bounds: some keyframes carry runaway values (e.g. `dodai00` reaching x≈50374). For
accurate geometry use `section_2` vertices (below).

---

## Section 4 (s4) — Keyframe Layers

**Struct:** `_unk0(u32), color_id(u8), flags(u8), blend_id(u8), tex_id(u8),
attributes(u32), s1_id(u16), s0_id(u16), s2_id(u16), _pad(u16)`

A layer references one texture quad: `s2_id` → vertices (`section_2`), `s1_id` → UVs
(`section_1`), `s0_id` → colours (`section_0`), `tex_id` → the texture within the FTX
atlas.

### `blend_id`

Selects the blend mode; it must be honoured. Observed distribution in `BGBtPlain_a`:
`blend=0` ×887, `1` ×206, `2` ×174, `3` ×96 (`BGBtCathedral`: `0` ×755, `1` ×274).
The mapping is not confirmed, but a plausible reading is 0=normal alpha, 1=additive,
2=multiply, 3=screen. Treating everything as normal alpha is wrong for glow/shadow layers.

### `flags`

Known bits: `SKIP = 0x02`, `NOTEX = 0x04`. A very common `0x01` is also seen; combined
`0x05` (`0x01|NOTEX`) occurs. `NOTEX` layers carry no texture (solid colour).

### `attributes`

A bitmask; only layers whose attributes are enabled by the instance's `variant_flags`
are drawn. This is how a single MBS covers several class/costume variants.

---

## Section 2 / 1 / 0 — Vertices, UVs, Colours

- `section_2` holds the four quad corner positions per layer (the **actual geometry** used
  for rendering and for computing content bounds).
- `section_1` holds per-corner UVs (in texture-texel space).
- `section_0` holds per-corner RGBA colours.

`section_0` (character) is a small atmospheric ramp table; backgrounds use a much larger
one (e.g. 262 entries) providing full-scene atmospheric perspective gradients.

---

## Section B (sb) — Timing Modifiers (characters only)

**Struct:** `speed_num(u32), speed_den(u16), _pad0(u32), _pad1(u16), oneshot(u16),
_pad2(u32), _pad3(u16)`

Referenced from `section_9` (`sa_sb_set_id`) and `section_a` (`sb_id`). Example values:

```
sb[0]: 7/2   oneshot=2   (DASH_1)
sb[2]: 47/17 oneshot=0   (MAGIC_A_3 / S01–S03)
sb[4]: 43/7              (LISTED / ORDER / STANCE)
sb[14]: 43/7             (D_WALK_A1)
sb[16]: 4/1  oneshot=1   (D_DASH_A1)
```

Hypothesis: `speed_num`/`speed_den` scale the per-frame tick duration; `oneshot` marks a
one-shot application. Pads are always 0. Backgrounds have no section B.

---

## Animation Hierarchy

### Character sprite data flow

```
Section 9 (Track) — named animation  "IDLE", "MAGIC_A_3"   (one active at a time)
  └── sa_set_id → Section A (sequence list)
        ├── s8_id → Section 8 (frames)
        │     ├── s6_id → Section 6 (keyframe)
        │     │     ├── s4_id → Section 4 (layers: tex, UV, vertex, colour)
        │     │     └── s5_id → Section 5 (hitbox entries)
        │     └── s7_id → Section 7 (transform)
        ├── s8_sum_once / commit_ticks → two-phase playback timing
        └── sb_id → Section B (timing modifiers)
```

### Background scene data flow

```
Section 9 (Track) — element or state "kumo_01", "CLS_CHNG_BFR_ENKE_IDLE"
  └── sa_set_id → Section A (sequence list)
        ├── s8_st → sub-sprite index within this element
        ├── s8_id → Section 8 (frames)
        │     ├── s6_id → Section 6 (keyframe)
        │     │     └── s4_id → Section 4 (texture region + UV + vertex)
        │     └── s7_id → Section 7 (world placement + fog tint)
        └── s8_sum → loop duration (long atmospheric loops, no intro/commit)
```

---

## Character Sprites vs Background Scenes

| Aspect | Character Sprite | Background Scene |
|--------|------------------|------------------|
| Track count | ~300+ (all actions) | ~15–110 (elements or states) |
| Track semantics | One animation active; named actions | Many simultaneous elements, **or** mutually-exclusive states |
| Section 3 / 5 (hitboxes) | Present | Absent |
| Section B (timing) | Present (hitstop, speed, one-shots) | Absent |
| Section 7 `fog` | Per-frame limb/effect tint | Per-frame atmospheric/effect tint |
| Section A `s8_st` | 0/1 (skip first frame) | 0–9 (sub-sprite index) |
| Section 8 loops | Fast 6–72 frame cycles with JUMP | Long 50–300 frame atmospheric loops |
| Frame sections | Two-phase intro/commit used | `s8_sum_once = -1` throughout |

---

## Background specifics

### Layer ordering / compositing

Unlike characters (one active track), backgrounds composite many tracks. The intended
depth order is **not** reliably encoded by `fog` alpha (see Section 7). Track order in
`section_9` roughly corresponds to back-to-front in the scenes examined, but this is not
confirmed; the engine may sort by another key.

### Variants and states

- **Landscape:** draw all element tracks.
- **State scenes (Classpedia):** draw a single state. `ENKE` (far view) is the coloured
  card; `KNKE` (near view) is a dark silhouette layer and must not be composited with it.
- **Day/night:** `*_yoru` / `yor*` variants should be excluded when a day scene is wanted.

### Content bounds / camera framing

`s9` bounds are framing *hints* (vary wildly and are not content bounds). `s6` bounds are
also unreliable (runaway values, and large-scale effect runs such as insert flashes extend
far). To derive a real content extent, union the transformed `section_2` vertex positions
of the live layers, and skip heavily-scaled runs (light shafts / flashes).

### Textures

Backgrounds reference one or more FTX atlases via `s4.tex_id` (an index into the texture
array); each layer's `s1` UV rectangle selects a sub-sprite within the atlas. The Classpedia
scene, for example, is a single texture-array-backed card.

---

## Remaining unknowns

| Section | Field | Observations |
|---------|-------|--------------|
| header | `_unk0`, `_unk1`, `_unk3` | Could hold scene-level data (camera/framing); unparsed |
| s4 | `_unk0` | Various values, unknown purpose |
| s4 | `attributes` | Bitmask; checked against `variant_flags`, exact bit meanings unconfirmed |
| s4 | `blend_id` | 0–3; mode mapping not confirmed |
| s4 | `flags` | `0x02`=SKIP, `0x04`=NOTEX; common `0x01` unidentified |
| s5 | `_unk0`, `_unk1` | Always 0 |
| s6 | `flags` u8 | Various values; keyframe properties |
| s8 | `0x2000` | Rarely set; purpose unclear |
| s8 | `n_180` | 0–24 on turn frames; exact semantics unconfirmed |
| sb | `speed_num`/`speed_den`/`oneshot` | Likely timing numerator/denominator/one-shot flag |
| — | Background layer depth order | Not reliably encoded in `fog`; possible track-order or another key |
