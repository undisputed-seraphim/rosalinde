# V77/MBS Sections Data Analysis

## Overview

This document describes the v77 animation format sections based on CSV dumps from actual
game data: Scarlet_F.mbs (v77, 333 tracks — character sprite) and BGBtPlain_a.mbs
(v77, 62 tracks — background scene).

## Field naming convention

CSV column names map 1:1 to C++ struct fields in `src/impl/mbs/sections.hpp`.
Where the struct has a named field, the CSV uses the struct name (e.g. `track_id`).
Where the struct uses `_unkN`, the CSV mirrors that.

S9 bounding boxes are represented as `left, top, right, bottom` (float) — these vary
per animation and serve as camera framing hints, not sprite bounds.

---

## Section 9 (s9) — Animation Track Definitions

**Struct:** `left(top right bottom)[4], name[24], sa_set_id(u16), sa_set_no(u8),
sa_set_main(u8), sa_sb_set_id(u16), sa_sb_set_no(u8), disabled(u8)`

**Field semantics:**

| Field | Meaning |
|-------|---------|
| `left,top,right,bottom` | Camera framing bounds for this track (not sprite bounds — vary wildly: FACEBATTLE = 800×855, D_DIE = 302×142) |
| `name` | Track identifier (e.g. `IDLE`, `MAGIC_A_1`, `D_DIE`) |
| `sa_set_id` | Starting index into `section_a` |
| `sa_set_no` | Number of `section_a` entries consumed |
| `sa_set_main` | 0 = standard track, 1 = "main" variant (used on middle parts of compound animations: KNOCKBACK, MAGIC_B_*, D_SURPRISED_*_A2, D_EXPLAIN_*_A2, D_HAND_*_A2, D_GUTS_POSE_*_A2) |
| `sa_sb_set_id` | Starting index into `section_b` for timing modifiers |
| `sa_sb_set_no` | Number of `section_b` entries consumed |
| `disabled` | 0 = active track, 1 = reserved/padding slot |

### Track naming conventions

**Battle actions:**
`IDLE`, `TURN`, `UPWARD`, `UPWARD_IDLE`, `UPWARD_END`, `DOWN`, `DOWN_IDLE`, `DOWN_END`,
`GUARD`, `AVOID`, `KNOCKBACK`, `DASH_1`, `DASH_2`

**Magic/Attacks (4 phases + end):**
`MAGIC_A_1` → `MAGIC_A_2` → `MAGIC_A_3` → `MAGIC_A_4` → `MAGIC_A_END`
(same pattern for B, C, D)

**Character-specific skill overrides:**
`S01_`/`S02_`/`S03_` (MAGIC_A variants), `S05_`/`S06_` (MAGIC_B),
`S07_` (MAGIC_C), `S18_QUICK_HEAL`, `S19_LISTED`,
`S22_CHASE`, `S23_COUNTER`, `S24_ACCUMULATE`, `S25_EX1`

**Dialogue animations (D_ prefix):**
- Idles: `D_IDLE_A` through `D_IDLE_F` (6 directional faces, no C variant)
- Transitions: `D_IDLE_A_TO_F`, `D_IDLE_F_TO_A`, `D_IDLE_B_TO_D`, etc.
- Movement: `D_WALK_A1/A2`, `D_WALK_B1/B2`, `D_WALK_D1/D2`, `D_DASH_*`, `D_WALK_BACK_*`
- Expressions: `D_SURPRISED`, `D_NOD`, `D_SIGH`, `D_LAUGH`, `D_DENY`, `D_ANGER`, `D_THINK`
- Gestures: `D_EXPLAIN`, `D_HAND`, `D_GUTS_POSE`
- Eye/face: `D_LOOK_IDLE`, `D_LOOK_TURN`, `D_LOOK_OVER`, `D_LOOK_FRONT`, `D_LOOK_UP`, `D_LOOK_AWAY`
- Postures: `D_SIT`, `D_SIT_FRONT`, `D_SIT_PRAY`, `D_DOWN`, `D_DOWN_SOFT`, `D_DOWN_UPWARD`
- Special: `D_VICTORY`, `D_DIE`

**Special track:**
`FACEBATTLE` — close-up dialogue camera (BB ±400×455)

### `sa_set_main` pattern

Set to 1 on tracks that serve as the "middle" phase of a compound animation:
- KNOCKBACK (row 12)
- MAGIC_B_1, MAGIC_B_2 (rows 25-26): the charged attack variants
- S05/S06 MAGIC_B variants
- VICTORY_1, VICTORY_2
- D_IDLE_A–F: all idle dialogue faces are main (the base state)
- D_SURPRISED_A2, D_EXPLAIN_A2, D_HAND_A2, D_GUTS_POSE_A2: the middle/intensifying phase
- FACEBATTLE

### `sa_sb_set_id` linkage to section B

Non-zero only on tracks that need timing modifiers (hitstop, speed curves):

| Value | Track(s) |
|-------|----------|
| 1 | DASH_1 |
| 2 | MAGIC_A_3 |
| 3 | MAGIC_B_END |
| 4–6 | LISTED, ORDER_1, STANCE_1 |
| 7–9 | S01/S02/S03_MAGIC_A_3 |
| 10–11 | S05/S06_MAGIC_B_END |
| 12–13 | S18_QUICK_HEAL_1, S19_LISTED |
| 14–24 | D_WALK/D_DASH directionals |

When `sa_sb_set_no = 1`, the track uses 1 section_b entry. When `sa_sb_set_no = 0`,
section_b is not consulted.

---

## Section A (sa) — Animation Sequences

**Struct:** `s8_id(u16), s8_no(u16), s8_sum(u32), s8_sum_once(i32), commit_ticks(i32),
sb_id(u16), sb_no(u8), s8_st(u8), track_id(u16), _pad(u16)`

**Field semantics:**

| Field | Meaning |
|-------|---------|
| `s8_id` | Starting index into `section_8` |
| `s8_no` | Number of `section_8` frames in this sequence |
| `s8_sum` | Total tick sum of all frame durations (`Σ frames[i].frames` across s8_id..s8_id+s8_no-1) |
| `s8_sum_once` | "Intro" portion: ticks that play once before the loop begins. -1 = no intro. 0 = entire sequence loops. Positive = tick count of the lead-in before looping |
| `commit_ticks` | "Commit window": ticks during which this sequence cannot be interrupted. -1 = no commit window. Same as `s8_sum_once` on simple loops. Differs on attacks with section_b timing (MAGIC_A_3: 99 intro, 59 commit). The game must wait `commit_ticks` ticks before allowing a transition out |
| `sb_id` | Index into `section_b` for per-sequence timing modifier (0 = no modifier) |
| `sb_no` | Number of `section_b` entries consumed (always 1 when `sb_id` > 0) |
| `s8_st` | **Character sprites:** "Skip first frame": 0 = play frames [s8_id .. s8_id+s8_no-1]. 1 = play frames [s8_id+1 .. s8_id+s8_no-1]. Set on sequences whose first frame reuses the generic body keyframe (s6_id=12 or equivalent). **Background scenes:** "Sub-sprite index" (0–9): identifies which sub-sprite within a composite landscape element this sequence animates. Each sub-sprite has its own frame loop and transforms but shares the same texture regions. See [Background Scenes](#background-scenes-vs-character-sprites). |
| `track_id` | Index of this sequence within its parent track. Does NOT always increment consecutively — gaps indicate reserved/future slots |

### Two-phase playback model

Each section_a entry describes a sequence with two timing windows:

```
[s8_id ........ s8_id+s8_no-1]   (s8_no frames spanning s8_sum ticks)
 |--- intro ---|------ loop body ------|
 |  guarded by   |  can be interrupted |
 | s8_sum_once   |  after commit_ticks ticks  |
```

| s8_sum_once | commit_ticks | Interpretation |
|-------------|-------|----------------|
| -1 | -1 | Single-pass sequence (no looping, no commit). Transitional frames. |
| 0 | -1 | Full-loop sequence with no intro (e.g. IDLE: all 432 ticks loop). |
| >0 | = s8_sum_once | Simple attack loop: N ticks play once, rest loops. Commit = intro. |
| >0 | ≠ s8_sum_once | Complex attack with section_b timing. Intro and commit durations diverge (e.g. MAGIC_A_3: 99 intro, 59 commit — the 40-tick gap is interruptible after commit). |

### Example: IDLE track

```
track_id=0: s8_id=0,   s8_no=55, s8_sum=432, s8_sum_once=0,   commit_ticks=-1, s8_st=0
track_id=1: s8_id=55,  s8_no=1,  s8_sum=1,   s8_sum_once=-1,  commit_ticks=-1, s8_st=0
track_id=2: s8_id=56,  s8_no=5,  s8_sum=144, s8_sum_once=0,   commit_ticks=-1, s8_st=0
track_id=3: s8_id=61,  s8_no=18, s8_sum=144, s8_sum_once=0,   commit_ticks=-1, s8_st=0
track_id=5: s8_id=79,  s8_no=24, s8_sum=144, s8_sum_once=0,   commit_ticks=-1, s8_st=0
```

Track 0 is the main loop (6 sub-blocks of 72 ticks each → 432 total). Tracks 1–5 are
sub-sequences. `track_id=4` is skipped (reserved).

### Example: MAGIC_A attack chain

```
MAGIC_A_1 (track_id=0): s8_sum=102, s8_sum_once=18,  commit_ticks=-1,                         s8_st=0
MAGIC_A_2 (track_id=0): s8_sum=102, s8_sum_once=18,  commit_ticks=-1,                         s8_st=0
MAGIC_A_3 (track_id=0): s8_sum=155, s8_sum_once=99,  commit_ticks=59,  sb_id=2, sb_no=1,      s8_st=1
MAGIC_A_4 (track_id=0): s8_sum=96,  s8_sum_once=40,  commit_ticks=-1,                         s8_st=0
MAGIC_A_END (track_id=0): s8_sum=171, s8_sum_once=27, commit_ticks=27,                        s8_st=1
```

MAGIC_A_2 and MAGIC_A_3 both have 18 tick intros. MAGIC_A_3 (the climax) has a 99-tick
intro with a 59-tick commit window, and references section_b[2] for hitstop timing.

### `s8_st` — dual semantics

The `s8_st` field has different meanings depending on whether the MBS is a character
sprite or a background scene.

**Character sprites (s8_st ∈ {0,1}): "Skip first frame"**

Every sequence with `s8_st=1` starts with a generic body keyframe (s6_id=12 or equivalent)
that is shared across many animations. The flag tells the engine to skip this generic first
frame and start from the animation-specific frames at offset +1.

Sequences with `s8_st=0` start directly with animation-specific keyframes.

**Background scenes (s8_st ∈ {0..9}): "Sub-sprite index"**

For backgrounds, `s8_st` indexes sub-sprites that belong to the same parent track.
A landscape element like a grass tuft (`kusa_01`) may have sub-sprites at s8_st=0,1,2,3,4
— each a separate visual component animating at its own cadence within the same logical
element. These sub-sprites are rendered together at the same depth layer but possibly
out of phase for natural sway motion. See [Background Scenes](#background-scenes-vs-character-sprites) below.

---

## Section 8 (s8) — Animation Frames

**Struct:** `s6_id(u16), _pad0(u16), s7_id(u16), frames(u16), flags(u32),
loop_s8_id(u16), s5s3_interp(u8), interp_rate(u8), s7_interp(u8), s6_interp(u8),
s0s1s2_interp(u8), n_180(u8), _pad1(u32), _pad2(u16), sfx_mute(u16), sfx_id(u32)`

### Flag bits (u32)

| Bit | Name | Evidence |
|-----|------|----------|
| 0x01 | FLIPX | Horizontal mirror. Directional variant key |
| 0x02 | FLIPY | Vertical mirror. Rare (end-frame of turn sequences) |
| 0x04 | JUMP | Control-flow: when set, `loop_s8_id` gives relative offset to next frame |
| 0x20 | ACTIVE | Set on 7,582/7,835 frames (96.9%). Clear only on hitbox-only frames (s6_id=1) and dedicated transition frames (s6_id=7,8). Means "emit this frame to the renderer" |
| 0x80 | POSE | Set on 698/7,835 frames (8.9%). Marks frames where the character body is in a non-idle pose: GUARD, KNOCKBACK, DOWN/SIT stance. May trigger cloth/hair physics recalibration |
| 0x400 | HITBOX | Hitbox-only frame (no sprite layers rendered) |
| 0x800 | LAST | End-of-sequence marker. When combined with JUMP: loop point |
| 0x2000 | — | Purpose unclear. Rarely set (isolated frames). May be "no blend" or "hold" |

### Frame duration clustering

| Duration | Use |
|----------|-----|
| 1 | Single-tick hitbox or transition |
| 2–4 | Brief keyframe change |
| 5–7 | Short hold |
| 8–10 | Standard action frame |
| 14–18 | Generic pose frame |
| 20–25 | Extended hold |
| 30–40 | Block cycle (6 ticks × 5..7 frames) |
| 43, 47, 64, 77, 83 | Intro/end long holds |

### JUMP + loop_s8_id

`loop_s8_id` is a **relative** offset applied when `JUMP=1`:
```
next_frame_index = current_index + loop_s8_id
```
Example from D_WALK cycle: `JUMP=1, loop_s8_id=17` → jumps back 17 frames in the
current sequence to create a walk loop.

### Interpolation fields

| Field | Range | Effect |
|-------|-------|--------|
| `s5s3_interp` | 0–7 | Hitbox interpolation |
| `interp_rate` | 0–2 | Interpolation frequency/speed |
| `s7_interp` | 0–2 | Transform (move/rotate/scale) interpolation |
| `s6_interp` | 0–1 | Keyframe layer transition |
| `s0s1s2_interp` | 0–2 | Color/UV/vertex interpolation |

Typical tiers:
- **None**: all interp = 0
- **Light**: s7_interp=1, s5s3_interp=1
- **Heavy**: s7_interp=2, s0s1s2_interp=2

### Other fields

- `n_180`: 0–24, correlates with 180° turn frames. Often set when frames use mirrored
  versions of the opposite direction's poses.
- `sfx_mute` / `sfx_id`: Mostly 0. `sfx_mute` may silence the frame's SFX trigger.
  `sfx_id` selects a sound effect (rarely used — most SFX are emitted at higher level).

---

## Section B (sb) — Timing Modifiers

**Struct:** `speed_num(u32), speed_den(u16), _pad0(u32), _pad1(u16), oneshot(u16), _pad2(u32), _pad3(u16)`

Section B is referenced from section_9 (`sa_sb_set_id`/`sa_sb_set_no`) and
section_a (`sb_id`/`sb_no`). It provides per-track or per-sequence timing modifiers.

**Example data (from Scarlet_F.mbs):**

```
sb[0]:  speed_num=7,  speed_den=2,  oneshot=2    (DASH_1 reference)
sb[1]:  speed_num=3,  speed_den=1,  oneshot=1    (DASH_1 falloff)
sb[2]:  speed_num=47, speed_den=17, oneshot=0    (MAGIC_A_3 / S01–S03 MAGIC_A_3 reference)
sb[3]:  speed_num=14, speed_den=3,  oneshot=0    (MAGIC_B_END / S05–S06 MAGIC_B_END)
sb[4]:  speed_num=43, speed_den=7               (LISTED / ORDER / STANCE start)
sb[14]: speed_num=43, speed_den=7               (D_WALK_A1)
sb[15]: speed_num=43, speed_den=7               (D_WALK_BACK_A1)
sb[16]: speed_num=4,  speed_den=1,  oneshot=1    (D_DASH_A1)
sb[17]: speed_num=4,  speed_den=1,  oneshot=1    (D_DASH_SHORT_A1)
sb[18]: speed_num=4,  speed_den=1,  oneshot=1    (D_WALK_B1)
sb[19]: speed_num=4,  speed_den=1,  oneshot=1    (D_DASH_B1)
sb[20]: speed_num=4,  speed_den=1,  oneshot=1    (D_DASH_SHORT_B1)
sb[21]: speed_num=3,  speed_den=1,  oneshot=1    (D_WALK_D1)
sb[22]: speed_num=3,  speed_den=1,  oneshot=1    (D_WALK_BACK_D1)
sb[23]: speed_num=4,  speed_den=1,  oneshot=1    (D_DASH_D1)
sb[24]: speed_num=4,  speed_den=1,  oneshot=1    (D_DASH_SHORT_D1)
```

`_pad0`, `_pad1`, `_pad2`, `_pad3` are always 0 in the observed data.

**Hypothesis:** `speed_num` is a per-frame tick multiplier numerator, `speed_den` is the
denominator, and `oneshot` is a boolean for "one-shot" application. In interactions
between `s8_sum_once` and `_unk0` (section_a), the section_b values modify either
the commit window or the per-frame tick duration.

---

## Animation Hierarchy

### Character sprite data flow

```
Section 9 (Track) — named animation  "IDLE", "MAGIC_A_3"
  ├── sa_set_id → Section A (sequence list)
  │     ├── s8_id → Section 8 (frames)
  │     │     ├── s6_id → Section 6 (keyframe)
  │     │     │     ├── s4_id → Section 4 (layers: tex, UV, vertex, color)
  │     │     │     └── s5_id → Section 5 (hitbox entries)
  │     │     └── s7_id → Section 7 (transform: move, rotate, scale, fog)
  │     │
  │     ├── s8_sum_once / commit_ticks → two-phase playback timing
  │     └── sb_id → Section B (timing modifiers)
  │
  └── sa_sb_set_id → Section B (track-level timing modifiers)
```

### Background scene data flow

```
Section 9 (Track) — named element "kumo_01", "kusa_01"
  ├── sa_set_id → Section A (sequence list)
  │     ├── s8_st → sub-sprite index within this element (0..9)
  │     ├── s8_id → Section 8 (frames)
  │     │     ├── s6_id → Section 6 (keyframe)
  │     │     │     └── s4_id → Section 4 (texture region + UV + vertex)
  │     │     └── s7_id → Section 7 (world-space placement + fog depth)
  │     │
  │     └── s8_sum → loop duration (long atmospheric loops, no intro/commit)
  │
  └── Section B absent (no timing modifiers needed)
```

---

---

## Background Scenes vs. Character Sprites

MBS files come in two distinct flavours with different data profiles, semantics, and
rendering models. The evidence below is drawn from BGBtPlain_a.mbs (v77, 62 tracks)
compared against Scarlet_F.mbs (v77, 333 tracks).

### Structural comparison

| Aspect | Character Sprite | Background Scene |
|--------|-----------------|-------------------|
| Track count | ~300+ (all combat/dialogue actions) | ~60 (individual scene elements) |
| Track semantics | Animation names (IDLE, MAGIC_A_3, D_WALK) | Landscape element names in romaji (kumo, yama, oka, ki, kusa) |
| Section 0 (fog) | 64 entries — single-character atmospheric ramp | 262 entries — full-scene atmospheric perspective |
| Section 3 (hitboxes) | Present (per-attack collision data) | Absent — no collision |
| Section 5 (hitbox entries) | Present | Absent — no collision |
| Section 7 `fog` field | Per-frame limb/effect tint | Per-element depth marker (see below) |
| Section A `s8_st` | 0 or 1 (skip-first-frame flag) | 0–9 (sub-sprite index within composite element) |
| Section B (timing modifiers) | Present (hitstop, speed curves, one-shots) | Absent — no combat timing |
| Section 8 frames/jump loops | Fast 6–72 frame cycles with JUMP back-references | Very long 50–300 frame atmospheric loops |
| Section 4 layers | Thousands of per-pose body-part layer stacks | Hundreds of per-element texture region assignments |

### Track naming: landscape element romaji

Background track names identify individual 2D sprites composited into the scene. Each
track has its own world-space bounds, texture regions, and animation loop:

| Pattern | Meaning | Examples |
|---------|---------|----------|
| `kumo_` | Cloud | kumo_01, kumo_02 (drift left-right) |
| `yama_` | Mountain | yama_01 (distant mountain) |
| `oka_` | Hill | oka_01–04 (rolling hills at different depths) |
| `mori_` | Forest | mori_01 (tree-line silhouette) |
| `iwayama_` | Rocky cliff | iwayama_01–07 (cliffs with parallax) |
| `gake_` | Cliff face | gake_01, gake_02 |
| `sougen_` | Meadow/grassland | sougen_01–05 (wide grassy plains) |
| `ki_` | Tree | ki_01, ki_02 (standalone trees swaying) |
| `haikyo_` | Ruins | haikyo_01 |
| `gareki_` | Debris | gareki_01, gareki_02 |
| `eda_` | Branch | eda_01, eda_02 (overhanging branches) |
| `iwa_` | Rock | iwa_01–06 (boulders and rock formations) |
| `kareki_` | Dead tree | kareki_01 |
| `koiwa_` | Small rock | koiwa_01, koiwa_02 (pebbles) |
| `kusamiti_` | Grass path | kusamiti_01–07 (path-edge grass) |
| `kusa_` | Grass tuft | kusa_01–10 (grass in multiple sway variants) |
| `tobukusa_` | Flying grass | tobukusa_01 (particle grass blowing) |
| `effect_` | VFX | effect_01–03 (ambient particles) |

### Depth model: the Fog column in Section 7

Section 7 transforms (`move_x, move_y, move_z, rotate_x, rotate_y, rotate_z, scale_x,
scale_y, fog`) include a 32-bit `fog` field. This encodes depth via ARGB color:

```
fog = 0xAARRGGBB packed uint32

alpha = (fog >> 24) & 0xFF
```

The alpha byte is a binary depth marker:

| Fog alpha | Depth tier | Example elements |
|-----------|-----------|------------------|
| `0xFF` | **Far background** (heavy fog overlay) | kumo (clouds), yama (mountains), oka (distant hills) — tinted with atmospheric color |
| `0xC0`–`0xDF` | **Mid-far** (moderate fog) | mori (forest), some iwayama (cliffs) — partial blending |
| `0x80`–`0xBF` | **Near-mid** (light fog) | ki (trees), haikyo (ruins) — subtle tint |
| `0x00` | **Foreground** (no fog, crisp) | kusa (grass tufts), kusamiti (grass paths), gake (cliffs), effects |

The `fog` field also encodes the RGB tint applied at each depth. For example:

```
kumo_01:    fog=0xFFE1D7C5  →  warm beige, opaque fog
yama_01:    fog=0xFFD9CABC  →  warm gray, opaque  
oka_01:     fog=0xFFC0A098  →  red-brown tint, opaque
mori_01:    fog=0x00C0A098  →  same tint, fully clear (no fog applied!)
iwayama_01: fog=0x00E1D7C5  →  warm beige, clear
effect_01:  fog=0xFFFFFFFF  →  opaque white (effects rendered above everything)
```

When fog alpha is `0x00`, the RGB tint is **not applied** — the sprite renders at its
native color. When alpha is `0xFF`, the RGB tint is blended in as an atmospheric fog
overlay. Intermediate alpha values provide proportional blending.

### Compositing render order

```
1. FAR BACKGROUND  (fog α = 0xFF)
   clouds → mountains → distant hills
   
2. MID BACKGROUND  (fog α = 0xC0..0xDF)
   forests → far cliffs → midground rocks
   
3. NEAR MIDGROUND  (fog α = 0x80..0xBF)
   trees → ruins → floating debris
   
4. MIDGROUND       (fog α = 0x01..0x7F)
   meadows → boulders → dead trees
   
5. CHARACTER SPRITES rendered here
   
6. FOREGROUND      (fog α = 0x00)
   grass tufts → grass paths → foreground cliffs → rocks
   
7. FOREGROUND OVERLAY (fog α = 0x00, last in z-order)
   tall grass → overhanging branches
   
8. EFFECTS         (fog α = 0xFF with opaque white tint)
   ambient VFX → particles → flying grass
```

The engine derives this order from the fog alpha byte: higher alpha = further back,
lower alpha = further forward. Effects use alpha `0xFF` with a white/neutral tint to
indicate they render on top of everything (special case).

### Section 0 fog ramp table

Background scenes have a much larger fog color table (262 entries vs 64 for characters).
These entries form atmospheric perspective ramps — color gradients that describe how
each terrain tone transitions from near to far:

```
Section 0 rows 140-149 (green atmospheric ramp, AAD9CA):
  0x20 → 0x40 → 0x80 → 0xC0 → 0xFF  (increasing opacity)
  Adds blue-green fog overlay to far terrain

Section 0 rows 150-159 (earth atmospheric ramp, 97C8A8):
  Same structure, warmer earth-tone palette
```

The fog ramp is indexed by the `s0s1s2_interp` field in section 8 to smoothly transition
the tint as the camera moves or as time-of-day lighting changes.

### Sub-sprite animation within elements

A single named background element can contain multiple sub-sprites that animate
independently. The `s8_st` field in section A indexes which sub-sprite a sequence
belongs to:

```
kusa_01 (grass tuft): sa_set_id=55, sa_set_no=4
  s8_st=0: root grass tuft, static
  s8_st=2: second sway layer, out-of-phase animation
  s8_st=3: third sway layer, different cadence
  s8_st=4: fourth sway layer
```

Each sub-sprite has its own section 8 frame loop with different frame durations and
JUMP targets, creating natural-looking sway where the same logical grass element has
leaves moving at different speeds. The section 6 keyframes for sub-sprites reference
the same section 4 layers (shared texture regions) but apply different section 7
transforms (slightly offset positions and rotations).

### Section B absence

Background MBS files have **no timing modifiers** (section B is empty — `sb_id=0,
sb_no=0` on all sequences). The two-phase playback model (`s8_sum_once`/`commit_ticks`)
is unused: `s8_sum_once = -1` on all background sequences, meaning every sequence is
a single-pass or full-loop with no intro phase. This reflects the fact that background
animations run continuously without attack commit windows or hitstop effects.

---

## Remaining unknowns

| Section | Field | Observations |
|---------|-------|--------------|
| s4 | `_unk0` | Various values, unknown purpose |
| s4 | `attributes` | Bitmask; checked against flags in render(), exact bit meanings unconfirmed |
| s5 | `_unk0`, `_unk1` | Always 0 |
| s6 | `flags` u8 | Various values; keyframe properties |
| s8 | `0x2000` | Rarely set (isolated frames); purpose unclear |
| s8 | `n_180` | 0–24 on turn frames; exact semantics unconfirmed |
| sb | `speed_num`/`speed_den`/`oneshot` | Likely timing numerator/denominator/one-shot flag |
| sb | `_pad0`/`_pad1`/`_pad2` | Always 0; padding or unused enum fields |
