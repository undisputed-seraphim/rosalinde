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
Where the struct has a named field, the CSV uses the struct name (e.g. `sa_track_slot`).
Where the struct uses `_unkN`, the CSV mirrors that.

S9 bounding boxes are represented as `left, top, right, bottom` (float) — these vary
per animation and serve as camera framing hints, not sprite bounds.

## Field resolution status

This pass used the full 722-file v77 corpus plus the Ghidra project
(`main2.elf`) that implements the track/sprite path. Names below are grounded
where a code path or a corpus invariant exists. `_unkN` remains only where no
consumer or semantic was found; those entries are called out rather than
guessed. The old `s8sa_loop::preprocess` reading and several earlier flag names
have been superseded — see the historical note in Section 8.

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

A 722-file v77 corpus (all MBS under `CPK_unpacked`, 969,315 section-8 rows)
has only these header values:

| Field | Observed values |
|-------|-----------------|
| `header_size` / `_pad0` / `_pad1` | `0x120` / `0` / `0` |
| `_unk0` / `_unk1` | `5` / `8` (constant) |
| `version` | `0x77` (no v76 file in the unpacked corpus) |
| `_unk2` | `0x230` (560) in 664 files; `0x30` (48) in 58 files |
| `_unk3` | `0x2000` (8192, constant) |

`_unk0`/`_unk1` and `_unk3` are effectively file-format constants here; no
renderer consumer was found. `_unk2` is data-dependent (the 48-value files are
mostly `Chara`, `CharaFace`, `CharaOp`, plus `GUI/Opening100` and
`BattleBG/BGEvFarmer_D_b`), but its purpose is still open. It is not the section
count, texture count, track count, or frame count.

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
| 3 | `s3` | `section_3` | 0x50 | Hitbox geometry (characters plus some BG/prop assets) |
| 4 | `s4` | `section_4` | 0x14 | Keyframe layers (tex/uv/vertex/colour refs) |
| 5 | `s5` | `section_5` | 0x08 | Hitbox entries (characters plus some BG/prop assets) |
| 6 | `s6` | `section_6` | 0x1c | Keyframes (bounds + layer range) |
| 7 | `s7` | `section_7` | 0x24 | Transforms (move/rotate/scale/fog) |
| 8 | `s8` | `section_8` | 0x20 | Animation frames |
| 9 | `s9` | `section_9` | 0x30 | Tracks |
| a | `sa` | `section_a` | 0x18 | Sequences |
| b | `sb` | `section_b` | 0x14 | Timing modifiers (mostly characters; also some BG/GUI/effect assets) |

`version = 0x76` files are parsed into the same v77 in-memory model.

---

## Section 9 (s9) — Tracks

**Struct:** `left(top right bottom)[4], name[24], sa_sequence_start(u16), sa_sequence_count(u8),
sa_main_sequence(u8), sa_sb_start(u16), sa_sb_count(u8), disabled(u8)`

| Field | Meaning |
|-------|---------|
| `left,top,right,bottom` | Camera framing hint for this track (not sprite bounds — vary wildly) |
| `name` | Track identifier |
| `sa_sequence_start` / `sa_sequence_count` | Starting index / count of `section_a` sequences |
| `sa_main_sequence` | **0-based index of the track's main/default sequence**, not a boolean flag. Usually 0, but `GUI/Opening010.mbs` uses 6, 14, and 86 on its effect tracks. |
| `sa_sb_start` / `sa_sb_count` | Track-level timing-modifier range into `section_b`. Most links are on characters, but section B also occurs in `CharaOp`, `CharaNpc`, `BattleBG`, `GUI`, and `Effect` files. |
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

### `sa_main_sequence` pattern (characters)

Set to 1 on "middle" phase tracks: `KNOCKBACK`, `MAGIC_B_1/2`, `S05/S06 MAGIC_B`,
`VICTORY_1/2`, `D_IDLE_A–F`, `D_SURPRISED_A2`/`D_EXPLAIN_A2`/`D_HAND_A2`/`D_GUTS_POSE_A2`,
`FACEBATTLE`.

### `sa_sb_start` linkage to section B (characters)

Non-zero only on tracks needing timing modifiers (hitstop/speed curves): `DASH_1`,
`MAGIC_A_3`, `MAGIC_B_END`, `LISTED`/`ORDER_1`/`STANCE_1`, `S01–S03 MAGIC_A_3`,
`S05–S06 MAGIC_B_END`, `S18_QUICK_HEAL_1`/`S19_LISTED`, and the `D_WALK`/`D_DASH`
directionals. `sa_sb_count = 1` means one `section_b` entry is used.

---

## Section A (sa) — Animation Sequences

**Struct:** `s8_frame_start(u16), s8_frame_count(u16), s8_total_ticks(u32), s8_intro_ticks(i32), commit_ticks(i32),
sb_id(u16), sb_no(u8), s8_start_offset(u8), sa_track_slot(u16), _pad(u16)`

| Field | Meaning |
|-------|---------|
| `s8_frame_start` | Starting index into `section_8` |
| `s8_frame_count` | Number of `section_8` frames in this sequence |
| `s8_total_ticks` | Total tick sum of all frame durations |
| `s8_intro_ticks` | Intro ticks played once before looping. `-1` = none; `0` = whole sequence loops |
| `commit_ticks` | Uninterruptible window (characters). `-1` = none |
| `sb_id` / `sb_no` | Per-sequence timing modifier (characters) |
| `s8_start_offset` | **Dual-purpose** — see below |
| `sa_track_slot` | Index of this sequence within its parent track; gaps = reserved slots |

### Two-phase playback model (characters)

```
[s8_frame_start ........ s8_frame_start+s8_frame_count-1]   (s8_frame_count frames spanning s8_total_ticks ticks)
 |--- intro ---|------ loop body ------|
 |  guarded by   |  interruptible after  |
 | s8_intro_ticks   |       commit_ticks     |
```

| s8_intro_ticks | commit_ticks | Interpretation |
|-------------|--------------|----------------|
| -1 | -1 | Single-pass sequence, no loop/commit (transitional) |
| 0 | -1 | Full-loop, no intro |
| >0 | = s8_intro_ticks | Simple attack loop: N ticks once, rest loops |
| >0 | ≠ s8_intro_ticks | Complex attack with section_b timing (e.g. MAGIC_A_3: 99 intro / 59 commit) |

Backgrounds never use this: `s8_intro_ticks = -1` on all background sequences.

### Example: character IDLE and MAGIC_A

```
IDLE  sa_track_slot=0: s8_frame_count=55 s8_total_ticks=432 s8_intro_ticks=0   (6 sub-blocks × 72 ticks)
MAGIC_A_1: s8_total_ticks=102 s8_intro_ticks=18
MAGIC_A_2: s8_total_ticks=102 s8_intro_ticks=18
MAGIC_A_3: s8_total_ticks=155 s8_intro_ticks=99 commit_ticks=59 sb_id=2 s8_start_offset=1
MAGIC_A_4: s8_total_ticks=96  s8_intro_ticks=40
MAGIC_A_END: s8_total_ticks=171 s8_intro_ticks=27 commit_ticks=27 s8_start_offset=1
```

### `s8_start_offset` — dual semantics

**Characters (`s8_start_offset ∈ {0,1}`): "skip first frame".** `1` skips the first frame (a generic
body keyframe shared across many animations) and starts at `s8_frame_start+1`.

**Backgrounds (`s8_start_offset ∈ {0..9}`): "sub-sprite index".** An element such as `kusa_01` can
have sub-sprites at `s8_start_offset = 0,1,2,3,4`, each a visual component with its own loop,
sharing the same texture regions but animating at its own cadence (natural sway).

---

## Section 8 (s8) — Animation Frames

**Struct:** `s6_keyframe_id(u16), _pad0(u16), s7_transform_id(u16), s8_duration_ticks(u16), flags(u32),
s8_loop_target(u16), s5s3_interp(u8), interp_rate(u8), s7_interp(u8), s6_interp(u8),
s8_render_value(u8), s8_draw_offset(u8), s8_event_arg(u32), s8_event_flag(u16),
s8_event_value(u16), s8_reserved(u32)`

### Flag bits (u32)

Counts below are from 969,315 section-8 rows across the 722-file v77 corpus
(one frame may carry several bits; counts are per bit, not per flag value).

| Bit | Count | Meaning |
|-----|-------|---------|
| `0x01` | 47,882 | `FLIPX` — horizontal mirror. Confirmed by `sprite_build_matrix` (`flags & 1`). |
| `0x02` | 11,862 | `FLIPY` — vertical mirror. Confirmed by `sprite_build_matrix` (`flags & 2`). |
| `0x04` | 48,052 | `JUMP` — next offset is `s8_loop_target`; **absolute**, see below. |
| `0x08` | 4,796 | `HOLD` — no frame advance. `anim_element_update` sets render flags and returns without incrementing the frame. Often co-occurs with `0x20`/`0x800`, but 101 frames carry `0x08` alone. |
| `0x10` | 0 | Never occurred; no semantics. |
| `0x20` | 813,843 | `ACTIVE` — renderable frame. `sprite_build_matrix` uses it for a render-object flag; the viewer uses it as the draw gate. |
| `0x40` | 24,566 | `HIDDEN` — skip update and draw. `s8_frame_is_hidden` returns this bit; `UcSpriteMone_update_plain` (plain Mone update) and `sprite_ease_and_build` return immediately when it is set. |
| `0x80` | 3,260 | `EVENT` — invokes TrackInstance vtable `+0xa0` with the payload words at `+0x14`, `+0x18`, `+0x1a`, `+0x1c`. Not a pose flag. |
| `0x100` / `0x200` | 0 | Never occurred. |
| `0x400` | 125,032 | `NODRAW` — update/timing continue, but `sprite_build_matrix` returns before creating a render object. The old "HITBOX-only" name is wrong. |
| `0x800` | 126,028 | `LAST` — end/hold marker. `anim_element_update` takes the no-advance path for `0x800`/`0x1000`; higher-level playback treats it as the end marker. |
| `0x1000` | 1 | `STOP` — same no-advance path as `0x800`; a single corpus occurrence. Purpose still open. |
| `0x2000` | 13,498 | `GLOBAL` — sets the track/global renderer flags (`puVar5[0xc] | puVar5[0xd]`). Mostly Effect/GUI frames; likely an overlay/special pass, but the exact pass is still open. |

Notes:

- `0x40` is a **hard update/render suppress**, not an interpolation hint.
- `0x400` suppresses the generic render object but does not stop the frame clock;
  this is distinct from `0x40`.
- The viewer's previous comment that `0x400` is harmless because it co-occurs
  with `0x20` is not the engine behavior; the engine checks `0x400` explicitly
  after the `ACTIVE` test.
- A frame can be `0x800` alone (non-rendered terminal sentinel) or `0x820`
  (renderable final pose). The `HOLD`/`LAST` combination is common (`0x828`:
  2,310 frames).

### JUMP + s8_loop_target — absolute target

`s8_loop_target` is a **sequence-relative absolute frame offset**, not
`current + offset`. `anim_element_update` stores `s8_loop_target` as the new
current offset; the frame index is
`section_8[s8_frame_start + s8_start_offset + s8_loop_target]`. The corpus
strongly confirms this:

- 17,956 nonzero jump targets are valid as absolute offsets.
- Only 5,806 are valid under the old relative reading.
- 26 are out of range under the absolute reading; the zero case is separate.

`s8_loop_target == 0` with `JUMP` is the largest group (30,070 frames). The code
sets the current offset to 0, i.e. **restart at the sequence's first played
frame**, `s8_frame_start + s8_start_offset`. It is not a generic "hold current
frame" unless that frame is already current. `HOLD` (`0x08`) is the explicit
no-advance bit.

`LAST` (`0x800`) is the end/hold marker. `s8_start_offset` affects both the
initial frame and the loop target; the global target is
`s8_frame_start + s8_start_offset + s8_loop_target`.

### The old `s8sa_loop::preprocess` walk (commit `31e3c5d`, removed)

The old helper also treated the JUMP target as absolute and `LAST` as a stop, so
its core control-flow reading is now confirmed. Its extra claim that the loop
point is found by revisiting a frame, and that `s8_intro_ticks`/`commit_ticks`
are unused, is still **only partially answered**: `anim_element_update` uses
`s8_loop_target` directly and the game's two-phase timing is handled elsewhere.
Do not use the old walker as the timing model; it is retained only as history.

### Interpolation fields

| Field | Corpus values | Effect |
|-------|---------------|--------|
| `s5s3_hitbox_interpolation` | 0 (967,796), 1 (861), 2 (658) | Hitbox interpolation mode. `sprite_build_matrix` dispatches 0=linear, 1=light, 2=cubic. |
| `interpolation_rate` | 1 (650,691), 2–8 common; 0 rare | Interpolation frequency/rate. `sprite_ease_and_build` uses it to compute the blend factor. |
| `s7_transform_interpolation` | 0 (780,570), 2 (129,633), 1 (59,112) | Transform interpolation mode; `sprite_build_matrix` has linear/cubic branches. |
| `s6_keyframe_interpolation` | 0 (708,439), 2 (204,657), 1 (56,219) | Keyframe/layer interpolation mode; branches at `param_3+0x11`. |
| `s8_render_value` | 0 (925,007), 1 (41,894), 2 (2,414) | Not read by the s7/s6/s5s3 interpolation branches. `sprite_build_matrix` writes it into render-object field `+0x1e4`; its exact visual/material meaning is still open. |

The old "tiers" description was inference, not a code path. `s5s3_interp` is
0–2 in the corpus, not 0–7.

**Two-asset note:** `TrackInstance` does hold two MBS asset pointers (`+0x08`,
`+0x98`), and `track_instance_bind_assets` binds them. However, the function named
`mbs_asset_bind_s0s1s2` binds the **s0/s1/s2 sections** (colour/UV/vertex
buffers), not the s8 `s8_render_value` field. The earlier idea that this
field is a two-source blend is therefore **unconfirmed**; do not use it as the
basis for a two-asset transition until the `+0x1e4` consumer is mapped.

### Other fields

- `s8_draw_offset` is a **signed draw/sort offset**, not a turn count. Corpus values are
  `0` (968,628), `1` (601), `-1` (61), `-2` (24), `-3` (1). `sprite_build_matrix`
  adds it to one of the render group/priority/shader offsets depending on the
  track's renderer flags.
- `s8_event_arg` (u32 at `+0x14`) is nonzero in only 43 frames, mostly with
  `0x80`.
- `s8_event_flag` (u16 at `+0x18`) is nonzero in 3,562 frames. It is the value
  the `0x80` TrackInstance callback consumes as a renderer flag index
  (`track_instance_set_flag_bit`), not padding.
- `s8_event_value` (u16 at `+0x1a`) is nonzero in 8 frames; the callback
  inspected does not consume it, so its meaning remains open.
- `s8_reserved` (u32 at `+0x1c`) is **zero in all 969,315 corpus frames**; keep
  it reserved until a consumer is found.

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

**Struct:** `left, top, right, bottom (float), s4_layer_start(u16), s5_hitbox_start(u16), s4_layer_count(u16),
s5_hitbox_count(u8), flags(u8), _pad0(u16)`

A keyframe = a bounding box plus a contiguous range of `section_4` layers and `section_5`
hitbox entries. `left/top/right/bottom` are *bounds*, but they are not reliable content
bounds: some keyframes carry runaway values (e.g. `dodai00` reaching x≈50374). For
accurate geometry use `section_2` vertices (below).

`flags` is populated, not reserved:

| Value | Count | Notes |
|-------|-------|-------|
| `0x1e` | 138,371 | Four low bits `0x02|0x04|0x08|0x10`; dominant normal keyframe. |
| `0x1a` | 46,125 | Missing `0x04`. |
| `0x1f` | 12,961 | Adds `0x01`; common on larger layer counts. |
| `0x1b` | 4,022 | `0x01|0x02|0x08|0x10`. |
| `0x3a` | 1,033 | `0x20` plus `0x1a`. |
| `0x12`, `0x0a`, `0x18`, … | rarer | Other bit combinations. |

The low bits are clearly a per-keyframe mask, but there is **no confirmed
renderer or hitbox consumer** in the inspected path; `s5s3_interp_*` reads
`s5_hitbox_start/count`, not `s6.flags`. Do not rename these bits yet.

---

## Section 4 (s4) — Keyframe Layers

**Struct:** `s4_source_tile_id(u32), color_id(u8), flags(u8), blend_id(u8), tex_id(u8),
attributes(u32), s1_uv_id(u16), s0_color_row_id(u16), s2_vertex_id(u16), _pad(u16)`

A layer references one texture quad: `s2_vertex_id` → vertices (`section_2`), `s1_uv_id` → UVs
(`section_1`), `s0_color_row_id` → colours (`section_0`), `tex_id` → the texture within the FTX
atlas.

### `s4_source_tile_id` — source tile / UV identifier

`s4_source_tile_id` is not a layer draw order. Across the corpus it is almost
always paired with exactly one `s1_uv_id`, and its distinct-value count usually
equals the `s1` row count:

- `Chara/Cleric_F`: 698 distinct values for 698 `s1` rows.
- `Chara/Scarlet_F`: 1278 / 1278.
- `BattleBG/BGBtPlain_a`: 184 / 184.
- `BattleBG/BGBtCapital_B_01`: 152 / 152.
- `GUI/Opening010`: 43 distinct IDs for 42 `s1` rows (one UV row has two source IDs).

The relation is one-to-one in both directions in 655/722 files; another 50 files
have unique `s4_source_tile_id` values with reused `s1` rows. Values are not
monotonic and are not the layer index (e.g. `BGBtCapital_B_01` cycles
`0,6,12,18,1,7,…`; `Opening010` uses `103..148`). This is consistent with an
exporter/source tile/UV ID. No consumer of `s4_source_tile_id` was found in
`sprite_build_matrix`, `anim_eval_track`, or the track-instance update path, so
it is treated as source metadata whose runtime effect (if any) is still
unconfirmed.

### `blend_id`

Selects the blend mode; it must be honoured. Corpus distribution:
`blend=0` ×7,564,990, `1` ×249,292, `2` ×35,269, `3` ×26,539. The exact engine
enum is still unconfirmed, but the viewer's mapping (0=normal alpha,
1=additive, 2=multiply, 3=screen) is the best-supported reading. Treating
everything as normal alpha is wrong for glow/shadow layers.

### `flags`

| Bit | Status |
|-----|--------|
| `0x01` | **ENABLED / valid layer** (strong corpus invariant; no direct renderer check found) — present on 7,765,217 corpus layers; `0x00` only 2,691. Combined `0x05` is `0x01|NOTEX`. |
| `0x02` | `SKIP` |
| `0x04` | `NOTEX` — no texture; solid colour/veil |
| `0x08`, `0x10`, `0x20`, `0x40`, `0x80` | Rare/absent in the corpus; do not assign semantics without a code consumer. |

The old "very common `0x01` unidentified" entry is resolved to the enabled/valid
bit; the exact engine meaning of `SKIP` is still not mapped to a renderer call.

### `attributes`

A bitmask; only layers whose attributes are enabled by the instance's `variant_flags`
are drawn. This is how a single MBS covers several class/costume variants. The
high bits are the class/group and the low byte is the variant; `docs/APPEARANCE.md`
documents the derived `group(class) | (1 << variant)` rule.

---

## Section 5 (s5) — Hitbox Entries

**Struct:** `s3_id(u16), s5_hitbox_kind(u8), s5_hitbox_control(u8), flags(u32)`

Referenced by `section_6.s5_hitbox_start` / `s5_hitbox_count`; `s3_id` indexes the
0x50-byte hitbox geometry in `section_3`. It is most common on characters, but
the corpus also has rows in `BattleBG`, `GUI`, `FieldBG`, and `Effect` assets
(prop/collision or camera hitboxes), so it is not character-only.

- `s5_hitbox_control` is the interpolator's control byte:
  - bit `0x01` set: the entry has **no geometry** (the three `s5s3_interp_*`
    functions skip the shape copy);
  - bit `0x02` set: **disable interpolation** and use the current hitbox only;
  - `0` = normal, `2` = non-interpolated, `1` = no geometry, `3` = both.
  Corpus: `0` ×4,910, `2` ×22; the other values are one-off.
- `s5_hitbox_kind` is `0` ×4,746, `1` ×184, `2`/`3` one each. It is **not read
  by the three interpolation functions**, so it is a gameplay-facing kind/group
  field whose exact meaning is still open.
- `flags` values are `4` ×3,800, `0` ×333, `3` ×229, `2` ×203, `1` ×134, then
  rarer `5–7` and `64`. The three `s5s3_interp_*` functions do **not** read this
  field (the bit test in `s5s3_interp_light` is `s5_hitbox_control & 2`), so the
  bit map is still open.

---

## Section 2 / 1 / 0 — Vertices, UVs, Colours

- `section_2` holds the four quad corner positions per layer (the **actual geometry** used
  for rendering and for computing content bounds).
- `section_1` holds per-corner UVs (in texture-texel space).
- `section_0` holds per-corner RGBA colours.

`section_0` (character) is a small atmospheric ramp table; backgrounds use a much larger
one (e.g. 262 entries) providing full-scene atmospheric perspective gradients.

---

## Section B (sb) — Timing Modifiers

**Struct:** `speed_num(u32), speed_den(u16), _pad0(u16), timing_a(u32),
oneshot(u16), timing_b(u16), _pad1(u32)` — row length `0x14`.

The earlier 0x16-byte layout was wrong: it placed `_pad0` over the two zero bytes at
`+0x06`, read `timing_a` as `_pad0`, and read `oneshot` two bytes too far. The
correct row starts:

```
+0x00 speed_num  u32
+0x04 speed_den  u16
+0x06 _pad0      u16   (always 0)
+0x08 timing_a   u32
+0x0c oneshot    u16
+0x0e timing_b   u16
+0x10 _pad1      u32   (always 0)
```

Referenced from `section_9` (`sa_sb_start`) and `section_a` (`sb_id`). Example
values:

```
sb[0]: 7/2   oneshot=2   (DASH_1)
sb[2]: 47/17 oneshot=0   (MAGIC_A_3 / S01–S03)
sb[4]: 43/7              (LISTED / ORDER / STANCE)
sb[14]: 43/7             (D_WALK_A1)
sb[16]: 4/1  oneshot=1   (D_DASH_A1)
```

`speed_num`/`speed_den` are consistent with a per-frame tick-rate ratio.
`oneshot` is 0–3 on character rows. Section B is **not character-only**: the
corpus has rows in `CharaOp`, `CharaNpc`, `BattleBG`, `GUI`, and `Effect` files.
`BGBtRoad_h` is the clearest file that exercises `timing_a` / `timing_b`
strongly (`timing_a` 40–140, `timing_b` 10–100, `oneshot` 10); those fields may
be a background loop/interval pair, but their exact semantics remain open.
`timing_a`/`timing_b` are zero on all character rows examined.

---

## Animation Hierarchy

### Character sprite data flow

```
Section 9 (Track) — named animation  "IDLE", "MAGIC_A_3"   (one active at a time)
  └── sa_sequence_start → Section A (sequence list)
        ├── s8_frame_start → Section 8 (frames)
        │     ├── s6_keyframe_id → Section 6 (keyframe)
        │     │     ├── s4_layer_start → Section 4 (layers: tex, UV, vertex, colour)
        │     │     └── s5_hitbox_start → Section 5 (hitbox entries)
        │     └── s7_transform_id → Section 7 (transform)
        ├── s8_intro_ticks / commit_ticks → two-phase playback timing
        └── sb_id → Section B (timing modifiers)
```

### Background scene data flow

```
Section 9 (Track) — element or state "kumo_01", "CLS_CHNG_BFR_ENKE_IDLE"
  └── sa_sequence_start → Section A (sequence list)
        ├── s8_start_offset → sub-sprite index within this element
        ├── s8_frame_start → Section 8 (frames)
        │     ├── s6_keyframe_id → Section 6 (keyframe)
        │     │     └── s4_layer_start → Section 4 (texture region + UV + vertex)
        │     └── s7_transform_id → Section 7 (world placement + fog tint)
        └── s8_total_ticks → loop duration (long atmospheric loops, no intro/commit)
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
| Section A `s8_start_offset` | 0/1 (skip first frame) | 0–9 (sub-sprite index) |
| Section 8 loops | Fast 6–72 frame cycles with JUMP | Long 50–300 frame atmospheric loops |
| Frame sections | Two-phase intro/commit used | `s8_intro_ticks = -1` throughout |

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

| Section | Field | Status |
|---------|-------|--------|
| header | `_unk0`, `_unk1`, `_unk3` | **Resolved as constants** in the v77 corpus (`5`, `8`, `0x2000`); no renderer consumer found. |
| header | `_unk2` | **Open.** `0x230` (664 files) or `0x30` (58 files); not section/texture/track/frame count. |
| s4 | `s4_source_tile_id` | **Mostly resolved:** exporter/source tile/UV ID (1:1 with `s1_uv_id` in 655/722 files). Runtime consumer not found; exact effect still open. |
| s4 | `attributes` | **Partially answered:** class/group high bits + variant low byte; exact engine per-layer test still open. |
| s4 | `blend_id` | **Open:** 0–3; mapping not confirmed; viewer uses normal/add/multiply/screen as a guess. |
| s4 | `flags` | **Partially answered:** `0x01` enabled/valid, `0x02` SKIP, `0x04` NOTEX; exact SKIP renderer behavior open. |
| s5 | `s5_hitbox_kind` | **Open:** hitbox kind/group; not read by the interpolation functions. |
| s5 | `s5_hitbox_control` | **Resolved:** bit0 no geometry, bit1 disable interpolation. |
| s5 | `flags` | **Open:** hitbox flags; individual bits tested but not named. |
| s6 | `flags` u8 | **Open:** per-keyframe mask (`0x1e`, `0x1a`, `0x1f`, `0x3a`, …); no confirmed consumer. |
| s8 | `s8_render_value` | **Open:** written to render-object `+0x1e4`, not used by the s7/s6/s5s3 interpolation branches. |
| s8 | `0x1000` | **Open:** one corpus occurrence; same no-advance path as `0x800`. |
| s8 | `0x2000` | **Partially answered:** sets global/track renderer flags; likely effect/GUI overlay. Exact pass open. |
| s8 | `s8_draw_offset` | **Partially answered:** signed draw/sort offset, not a turn count; target depends on renderer flags. |
| s8 | `s8_event_arg`/`s8_event_flag`/`s8_event_value`/`s8_reserved` | **Partially answered:** `0x80` payload; `s8_event_flag` is a renderer flag index, `s8_reserved` is always 0. |
| sb | `speed_num`/`speed_den` | **Likely** tick-rate ratio; no direct consumer mapped. |
| sb | `timing_a`/`timing_b`/`oneshot` | **Open:** character timing vs background interval pair; section B is not character-only. |
| — | Background layer depth order | **Open:** not reliably encoded in `fog`; possible track order or another key. |

---

## Corpus statistics (earlier 46-file sample and full-corpus field checks)

Dumped with `rosalinde --mbs-dump` (6 GUI, 14 BattleBG, 25 character, 1 FieldBG,
…) and tallied per column.

### section_9 (tracks) — 46-file sample

- `sa_sequence_count` (sequences per track): `0→508, 1→566, 2→106, 3→407, 4→407, 5→257,
  6→161, 7→56, …` up to 87. **58 % of tracks have >1 sequence.**
- **Every multi-sequence track's sequences tile the `s8` array contiguously**
  (`s8_frame_start` ranges back-to-back; 1499/1499). The sequences are therefore *ordered
  segments of one linear timeline*, not interleaved parallel parts.
- `sa_track_slot` has reserved gaps in ~half of multi-seq tracks (758 gaps / 741 contiguous).
- `sa_main_sequence` is a **0-based sequence index**, not a boolean. Many character and
  background tracks use 0; compound tracks use higher indices (e.g. `GUI/Opening010`
  track 1 uses 86, track 3 uses 6, track 5 uses 14).
- `disabled=1` on 505 tracks (reserved/padding) — skip them.
- `left/top/right/bottom` is a framing rect: only **6/2573** tracks exceed ±5000,
  so it is usually sane (`TITLELOGO` is one outlier at ±10000).

### section_a (sequences) — 46-file sample

- `s8_intro_ticks`: **−1 (single-pass) 4874, 0 (full loop) 1986, >0 (intro+loop) 856**
  → most sequences are single-pass.
- `s8_start_offset != 0` on **3974/7716** sequences — `s8_start_offset` is significant (characters:
  skip-first-frame; backgrounds: sub-sprite index).
- `commit_ticks != -1`: 847. `sb_id != 0` (link into section_b): only 197.

### section_8 (frames) — full 722-file corpus

- 969,315 rows. Bit counts: `0x20` 813,843; `0x40` 24,566; `0x400` 125,032;
  `0x800` 126,028; `0x04` 48,052; `0x08` 4,796; `0x01` 47,882; `0x02` 11,862;
  `0x80` 3,260; `0x2000` 13,498; `0x1000` 1; `0x10`/`0x100`/`0x200` 0.
- `s8_loop_target == 0` with `JUMP` occurs on 30,070 frames. The engine sets the
  current offset to 0, so this is **restart at the first played frame**
  (`s8_frame_start + s8_start_offset`), not a generic hold.
- Nonzero loop targets: 17,956 valid absolute, 5,806 valid relative, 26
  out-of-range absolute. This is decisive for the absolute reading.
- `s8_draw_offset` is signed: `0` ×968,628, `1` ×601, `-1` ×61, `-2` ×24, `-3` ×1.
- Payload fields: `+0x14` nonzero 43×, `+0x18` nonzero 3,562×, `+0x1a` nonzero
  8×, `+0x1c` zero in all frames.

### section_b

- 3,128 rows across the corpus, including `Chara`, `CharaOp`, `CharaNpc`,
  `BattleBG`, `GUI`, and `Effect` — **not character-only**.
- Corrected row layout is 0x14 bytes; `oneshot` is u16 at `+0x0c`, not `+0x0e`.
- Character rows use `speed_num`/`speed_den` and `oneshot` 0–3. `BGBtRoad_h`
  uses `timing_a` 40–140 and `timing_b` 10–100 with `oneshot=10`; that file is
  the main evidence that the two u32/u16 fields are real, not padding.
- `sa.sb_id != 0` is only 197 rows, so most section-B rows are not linked from
  a sequence; the link is sparse.

### Playback implications

- **`0x20` clear ⇒ don't draw** — the viewer's `ACTIVE` gate. The engine also
  checks `0x40` (skip update/draw) and `0x400` (suppress the generic render
  object) independently; do not equate `ACTIVE` with the whole engine gate.
- **`0x40` is a hard hide** — `s8_frame_is_hidden` returns it, and both
  `UcSpriteMone_update_plain` and `sprite_ease_and_build` return before updating or drawing.
- **`0x400` is no-render, not inactive** — the frame clock still advances, but
  `sprite_build_matrix` returns before creating a render object. The old viewer
  behavior that treated it as a harmless ACTIVE modifier is not engine-faithful.
- **`0x08` is the explicit hold** — no frame advance; do not use
  `s8_loop_target == 0` for that purpose.
- **A finished single-pass part stops drawing** (`s8_intro_ticks < 0` → `hide`).
  A `0x820` frame is drawn on its own tick, then the part ends; the 1-tick
  terminal-frame handling in the viewer is a display workaround, not the file
  semantics.
- **Contiguous tiling ⇒ sequences play sequentially** (`VIEWER_RUNS=seq`) — still a
  hypothesis; the default stays parallel (characters are mostly single-sequence);
  not flipped globally yet.
- `JUMP` + `s8_loop_target == 0` **restarts at the first played frame**; it is
  not a hold. The viewer now uses the absolute target
  (`viewer/src/sprite.cpp`).
