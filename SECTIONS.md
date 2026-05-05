# V77/MBS Sections Data Analysis

## Overview

This document contains observations about the v77 animation format sections based on CSV dumps from actual game data files.

---

## Section 9 (s9) - Animation Track Definitions

**Structure:**
```
name[24], sa_set_id, sa_set_no, sa_set_main, sa_sb_set_id, sa_sb_set_no, disabled
```

### Observations:

**Track Naming Conventions:**
- **Basic Actions**: `IDLE`, `TURN`, `UPWARD`, `DOWN`, `DOWN_IDLE`, `DOWN_END`, `GUARD`, `AVOID`, `KNOCKBACK`
- **Movement**: `DASH_1`, `DASH_2`
- **Magic/Attacks**: `MAGIC_A_1` through `MAGIC_A_4` + `MAGIC_A_END`, same pattern for B/C/D
- **Character-specific**: `S01_`, `S02_`, `S03_`, `S05_`, `S06_`, `S07_`, `S18_`, `S19_`, `S22_`-`S25_` prefixes
  - S01/S02/S03 have MAGIC_A variants
  - S05/S06 have MAGIC_B variants
  - S07 has MAGIC_C variants
  - S18 has `QUICK_HEAL`
  - S19 has `LISTED` variants
  - S22-S25: `CHASE`, `COUNTER`, `ACCUMULATE`, `EX1`
- **Dialogue/Facial Animations** (D_ prefix):
  - Idle states: `D_IDLE_A` through `D_IDLE_F` (6 directions/faces)
  - Transitions: `D_IDLE_A_TO_F`, `D_IDLE_F_TO_A`, etc.
  - Movement: `D_WALK_A1/A2`, `D_WALK_B1/B2`, `D_WALK_D1/D2`, `D_DASH_*`, `D_WALK_BACK_*`
  - Expressions: `D_SURPRISED`, `D_NOD`, `D_SIGH`, `D_LAUGH`, `D_DENY`, `D_ANGER`, `D_THINK`
  - Gestures: `D_EXPLAIN`, `D_HAND`, `D_GUTS_POSE`
  - Eye/Face direction: `D_LOOK_IDLE`, `D_LOOK_TURN`, `D_LOOK_OVER`, `D_LOOK_FRONT`, `D_LOOK_UP`, `D_LOOK_AWAY`
  - Postures: `D_SIT`, `D_SIT_FRONT`, `D_SIT_PRAY`, `D_DOWN`, `D_DOWN_SOFT`, `D_DOWN_UPWARD`
  - Special: `D_VICTORY`, `D_DIE`

**Field Patterns:**
- `sa_set_id`: Starting index into section_a table (ranges from 0 to 7800+)
- `sa_set_no`: Number of section_a entries (typically 1-8, up to 26 for complex animations)
- `sa_set_main`: Usually 0 or 1; set to 1 for:
  - KNOCKBACK
  - MAGIC_B_* and S05/S06 MAGIC_B variants
  - D_SURPRISED_*2 variants (the middle part of 3-part sequences)
  - D_EXPLAIN_*2, D_HAND_*2, D_GUTS_POSE_*2
- `sa_sb_set_id`: References section_b; non-zero for:
  - DASH_1 (value=1)
  - MAGIC_A_3/C_END variants (values 2-12)
  - ORDER/STANCE/LISTED animations (values 4-13)
  - D_WALK/D_DASH sequences (values 14-24)
- `disabled`: 0=active, 1=disabled (empty/reserved slots)

**Empty Slots:**
- Many entries have empty name and all zeros with disabled=1
- These appear to be reserved/padding entries
- Found in blocks: rows 20-24, 28-30, 50, 81, 261-333

---

## Section A (sa) - Animation Sequences

**Structure:**
```
s8_id, s8_no, s8_sum, s8_sum_once, unk0, sb_id, sb_no, s8_st, unk1, unk2
```

### Observations:

**Field Analysis:**

- `s8_id`: Starting frame index in section_8 (ranges 0 to 7800+)
- `s8_no`: Number of frames in this sequence (typically 1-40)
- `s8_sum`: Total frame duration/ticks
  - When `unk0=-1`: s8_sum is also -1 (variable/indefinite length)
  - Otherwise: concrete values like 144, 164, 204, etc.
- `s8_sum_once`: Either -1 or positive value (< 255)
  - When positive, often equals s8_sum for single-frame sequences
  - May indicate "play once" duration vs loop duration
- `unk0`: Usually -1, sometimes 0 or small positive (observed: -1, 0, 1, 2, 3, 4, 5, 6, 7)
  - When 0: s8_sum has concrete value
  - When -1: s8_sum is also -1
- `sb_id/sb_no`: Section B references (mostly 0, occasionally non-zero)
- `s8_st`: Start offset flag (0 or 1)
  - 0 = start from beginning
  - 1 = offset start (skip first frame? subtract one?)
- `unk1`: Sequence counter within track (increments: 0, 1, 2, 3, 4...)
  - Resets for each new s9 track
  - Useful for identifying which sequences belong together
- `unk2`: Always 0

**Pattern Examples:**

IDLE track (s9 row 1):
```
sa rows 2-6: unk1=0,1,2,3,5 (note: skips 4)
- s8_no: 55, 1, 5, 18, 24
- s8_sum: 432, 1, 144, 144, 144
```

MAGIC_A_1 track (s9 row 15):
```
sa rows 45-47: unk1=0,1,2
- s8_no: 3, 3, 4
```

**Sequence Grouping by unk1:**
Within each s9 track, sa entries are grouped by consecutive unk1 values:
- unk1=0: Usually the main/primary sequence
- unk1=1,2,3...: Continuation/transitional sequences
- Higher values (10-24): Found in complex dialogue animations

---

## Section 8 (s8) - Animation Frames

**Structure:**
```
s6_id, _pad0, s7_id, frames, FLIPX, FLIPY, JUMP, 0x20, 0x80, HITBOX, LAST, 0x2000,
loop_s8_id, s5s3_interp, interp_rate, s7_interp, s6_interp, s0s1s2_interp, n_180, _pad1, _pad2, sfx_mute, sfx_id
```

### Observations:

**Frame Duration (`frames` field):**
Common values observed:
- **1**: Single-tick frames (transitions, quick states)
- **2-4**: Very short animations
- **5-7**: Short animations
- **8-10**: Medium duration
- **14-18**: Standard animation frames
- **20-25**: Longer holds
- **30-40**: Extended sequences
- **43, 47, 64, 77**: Special long holds (often intro/end frames)

**Flag Patterns:**

| Flag | Meaning | Observations |
|------|---------|--------------|
| FLIPX (0x01) | Horizontal flip | Used for directional variants; often appears in pairs with matching non-flipped frames |
| FLIPY (0x02) | Vertical flip | Rare; seen on last frame of some sequences |
| 0x20 | Unknown | Set on most frames (~95%); may indicate "active" or default state |
| 0x80 | Unknown | Never observed set |
| HITBOX (0x400) | Skip rendering | When set, frame is skipped in render(); used for hitbox-only frames |
| LAST (0x800) | End marker | Often paired with JUMP; indicates sequence end/loop point |
| 0x2000 | Unknown | Rarely set; purpose unclear |

**JUMP + loop_s8_id Pattern:**
When `JUMP=1`, the `loop_s8_id` field contains a relative offset:
```
Example from D_WALK cycle:
- Frame at index N: JUMP=1, loop_s8_id=17
  → Next frame = current_position + 17 (jumps back in sequence)
```

This creates loops within animation sequences.

**Interpolation Fields:**

`s5s3_interpolation` (hitbox interpolation):
- 0: No interpolation
- 1-7: Interpolation enabled (higher = more frames?)

`interp_rate`:
- Appears to control interpolation frequency
- Common values: 0, 1, 2
- Often correlates with s7_interp

`s7_interpolation` (transform interpolation):
- 0: No interpolation
- 1-2: Interpolation enabled
- When non-zero, transform (move/rotate/scale) interpolates between keyframes

`s6_interpolation` (keyframe interpolation):
- Usually 0 or 1
- Controls layer/keyframe transitions

`s0s1s2_interpolation` (color/uv/vertex interpolation):
- 0: No interpolation
- 1-2: Interpolation enabled
- Affects visual smoothness of texture/color changes

**Interpolation Pattern Example:**
```
No interp:  s5s3=0, s7=0, s6=0, s0s1s2=0
Light interp: s5s3=1, s7=1, s6=0, s0s1s2=0
Full interp: s5s3=1, s7=2, s6=0, s0s1s2=2
```

**n_180 Field:**
- Usually 0
- When non-zero: values 1-24 observed
- May relate to 180-degree turn animations
- Often appears on frames with LAST flag

**sfx_mute / sfx_id:**
- Mostly 0 (unused)
- `sfx_mute`: Possibly mutes sound effects for this frame
- `sfx_id`: Sound effect identifier

---

## Animation Hierarchy Summary

```
┌─────────────────────────────────────────────────────────────┐
│ Section 9 (Track) - Named animation set                     │
│ e.g., "IDLE", "MAGIC_A_1", "D_IDLE_A"                       │
│ Fields: sa_set_id, sa_set_no                                │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ Section A (Sequence) - Ordered list of frame groups         │
│ Fields: s8_id, s8_no, s8_sum, unk1 (sequence counter)       │
│ Typically 3-5 sequences per track                           │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ Section 8 (Frame) - Individual animation frame              │
│ Fields: s6_id, s7_id, frames (duration), flags              │
│ Controls: keyframe reference, transform, timing, looping    │
└─────────────────────────────────────────────────────────────┘
                            │
            ┌───────────────┴───────────────┐
            ▼                               ▼
┌───────────────────────┐       ┌───────────────────────┐
│ Section 6 (Keyframe)  │       │ Section 7 (Transform) │
│ - s4 layers           │       │ - move (x,y,z)        │
│ - s5 hitboxes         │       │ - rotate (x,y,z)      │
│ - bounding box        │       │ - scale (x,y)         │
│ - depth ordering      │       │ - fog                 │
└───────────────────────┘       └───────────────────────┘
            │
            ▼
┌───────────────────────┐
│ Section 4 (Layer)     │
│ - tex_id              │
│ - s0_id (color/fog)   │
│ - s1_id (UV coords)   │
│ - s2_id (vertices)    │
│ - attributes          │
│ - flags               │
└───────────────────────┘
```

---

## Unknown Fields Requiring Further Investigation

| Section | Field | Observations | Hypothesis |
|---------|-------|--------------|------------|
| s4 | `_unk0` | Various values | Unknown attribute/classification |
| s4 | `attributes` | Bitmask; checked against flags in render() | Layer visibility/condition flags |
| s5 | `_unk0`, `_unk1` | Always 0 | Padding or unused |
| s6 | `flags` | Various values | Keyframe properties |
| s8 | `0x20` flag | Set on ~95% of frames | Default/active state indicator |
| s8 | `0x80` flag | Never observed set | Unknown/rarely used |
| s8 | `0x2000` flag | Rarely set | "Ignore next frame"? |
| s8 | `n_180` | 0-24, correlates with turns | 180-degree turn indicator |
| sa | `unk0` | -1 or 0-7 | Sequence type/mode |
| sa | `s8_sum_once` | -1 or positive | Single-play duration |
| sa | `sb_id`, `sb_no` | Usually 0 | Section B linkage |
| sb | All fields | Unknown purpose | Timing data? Physics? |

---

## Notable Patterns

### 1. Three-Part Animation Structure
Many animations follow a 3-part pattern visible in section_9:
```
MAGIC_A_1 → MAGIC_A_2 → MAGIC_A_3 → MAGIC_A_4 → MAGIC_A_END
  (start)   (continue)  (climax)   (follow)    (return)
```

### 2. Dialogue Animation Symmetry
Directional dialogue animations come in sets:
```
D_IDLE_A, D_IDLE_B, D_IDLE_D, D_IDLE_E, D_IDLE_F  (5 directions)
D_TURN_A, D_TURN_B, D_TURN_D, D_TURN_E, D_TURN_F
```
Note: No direction C - possibly front-facing default?

### 3. Frame Duration Clustering
Frame durations cluster around specific values suggesting fixed timing buckets:
- 1, 2, 3, 4, 5, 6, 7 (short)
- 8, 10, 12 (medium)
- 14, 15, 16, 17, 18 (standard)
- 20, 24, 25, 30, 40 (long)

### 4. Interpolation Tiers
Three interpolation "tiers" observed:
- **None**: All interp fields = 0
- **Light**: s7_interp=1, s5s3_interp=1
- **Heavy**: s7_interp=2, s0s1s2_interp=2

### 5. Loop Markers
LAST + JUMP flags together mark loop points:
```
Frame N: LAST=1, JUMP=1, loop_s8_id=X → Loop back X frames
```

---

## TODO / Questions

1. What is the exact purpose of section_b (sb)? Referenced but never read in render()
2. What does the 0x20 flag on s8 mean? It's set almost universally
3. How does `sa_set_main` in s9 affect playback?
4. What is the relationship between `s8_sum` and actual frame timing?
5. Does `n_180` relate to rotation interpolation or something else?
6. Are there parent-child transform relationships between s7 matrices?
7. What do s4 `attributes` bits control exactly?
