# V77/MBS Sections Data Analysis

## Overview

This document describes the v77 animation format sections based on CSV dumps from actual
game data (Scarlet_F.mbs, v77, 333 tracks).

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
| `s8_st` | "Skip first frame": 0 = play frames [s8_id .. s8_id+s8_no-1]. 1 = play frames [s8_id+1 .. s8_id+s8_no-1]. Set on sequences whose first frame reuses the generic body keyframe (s6_id=12 or equivalent) |
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

### `s8_st` evidence

Every sequence with `s8_st=1` starts with a generic body keyframe (s6_id=12 or equivalent)
that is shared across many animations. The flag tells the engine to skip this generic first
frame and start from the animation-specific frames at offset +1.

Sequences with `s8_st=0` start directly with animation-specific keyframes.

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
