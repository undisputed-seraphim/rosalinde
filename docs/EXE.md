# `main` (game executable) — RE guide

A working guide to reverse-engineering the Unicorn Overlord Switch executable
(`ExeFS/main`, an NSO0). Written so a fresh agent can **recover the same facts**
and **continue the open threads** without re-deriving everything.

All addresses are **module-relative** (module base `0`) — exactly what
radare2/Ghidra show when the image is loaded at base 0. Names in `CamelCase`
(`DrSetCameraPosV`, `TrackInstance`) are recovered/assigned; names in `FUN_`/
`fcn.` are auto-generated and appear as such in a fresh project.

Status: living notes, not exhaustive. Kept uncommitted (like `docs/ASB.md`).

---

## 0. Quick reference — where to look

| Address | Name | What it is |
|---|---|---|
| `0x0` / `0x82a000` / `0x26de000` | — | text / rodata / data segment vaddrs |
| `0x82a048` | `.rela.dyn` | 98211 × `R_AARCH64_RELATIVE` entries |
| `0x28c6000` | native pointer table | slot → registered native function (via relocations) |
| `0x2d4xxx–0x2ddexx` | native registration routine | fills the table + builds signatures |
| `0x28d5200 + 0x1e8` | active **camera** | pointer to the current camera object |
| `0x2741bb8` | **CharaSet** table | stride `0x48`, 1388 entries (appearance data) |
| `0x31ff3c` | `DrSetCameraPosV` | set camera position |
| `0x320050` | `DrSetCameraZoom` | set camera zoom |
| `0x3202e0` | `DrSetCameraParam` | set camera params |
| `0x16d8f0` | `cam_update_view_rect` | recompute the camera's visible rect |
| `0x2f57c0` | `SetSpriteMoneAnimation` | script native → set a sprite's track |
| `0x4ca3c` | `sprite_set_track` | store track, flag elements |
| `0x5919f0` | `FUN_005919f0` | vtable dispatch of a track change |
| `0x590e00` | `anim_eval_track` | (re)build a track's element array |
| `0x58bf58` | `anim_init_element` | init one element from a sequence |
| `0x5acb10` | `anim_update` | per-frame updater: walk track instances |
| `0x58d1a8` | `sprite_build_matrix` | per-sprite world→clip (adds the view offset) |
| `0x58e82c` | `sprite_ease_and_build` | ease factors → `sprite_build_matrix` |
| `0x31ea0c` | `DrUiSetPosV` | UI sprite position → object `+0x1900` |
| `0x2f5fb0` | `SetSpritePosV` | sprite position → object `+0x110` |
| `0x16d7b0` | `cam_update` | camera per-frame update (vtable method) |
| `0x16d99c`…`0x16e210` | `cam_*` sub-updates | reset / **pos-lerp** / **zoom-lerp** / sway A / sway B |
| `0x16e88c` | `cam_begin_pos_tween` | set-pos tween setup (start/target/ease) |
| `0x5b26e0` | `mat_build_camera` | large camera matrix / derived-vector builder |
| `0x58bf58` | `anim_init_element` | init one `0xf0` element (`+0xa8`→TrackInstance) |
| `0x594674` | `config_singleton_get` | returns config object `*0x3b375b0` |
| `0x28a28d0` | bit table | `+4*i = 1<<i` (rodata) |
| `0x31b01c`/`0x31b0dc`/`0x31b1ac` | `DrCreateChara`×2 / `DrCreateNpc` | character spawns |
| `0x1a0dc`/`0x11a100`/`0x175be0`/`0x2bf900` | CharaSet helpers | see §7 |

Tooling: **Ghidra** (`reagent` MCP) for decompilation + typing; **radare2** for
search/xrefs. Both work on the same ELF. See §11 for gotchas and known tool bugs.

---

## 1. Getting a usable image

`ExeFS/main` is a Nintendo **NSO0** (`sdk`/`rtld` are the SDK + loader,
`main.npdm` is metadata).

```
magic "NSO0" @0, header 0x100
.text   file 0x101     vaddr 0x0        decompressed 0x829150   compressed 0x4ae751
.rodata file 0x4ae852  vaddr 0x82a000   decompressed 0x1eb33f0  compressed 0x6181a5
.data   file 0xac69f7  vaddr 0x26de000  decompressed 0x1f0048   compressed 0x3a8d3
flags   0x3f  (all three segments compressed)
compressed_sizes[] = { text, rodata, data } @ file 0x60
```

Each segment is **one** raw LZ4 block (`LZ4_decompress_safe(src,dst,comp,decomp)`).
Gotcha: the length-extension terminator is `0xFF` (standard LZ4; the naive
`0x0F` reading is wrong).

Rebuild a loadable ELF with three `PT_LOAD`s (text `r-x`, rodata `r--`,
data `rw-`):

```bash
python3 tools/nso2elf.py ~/zwei/unicorn_overlord/main -o ~/zwei/unicorn_overlord/main2.elf
```

- Permanent ELF: `~/zwei/unicorn_overlord/main2.elf` (~42.8 MB).
- Ghidra project **`unicorn_main`**, program **`main2`** (reagent MCP).
- The conductor re-reads the *original source path* at boot; keep a synced
  `/tmp/opencode/main2.elf` symlink → the permanent ELF.
- radare2: `r2 -A main2.elf` (or the MCP `radare2_open_file` + `radare2_analyze`).

---

## 2. Relocations & the mod0 header

`.mod0` @ vaddr `0x8`:

```
dynamic   = 0x28c1e00
bss       = [0x28ceff8, 0x477eb58)
eh_frame  = [0x25b40a8, 0x25eee6c)
```

`.dynamic` @ `0x28c1e08`:

```
DT_RELA     = 0x82a048
DT_RELASZ   = 0x2431c8
DT_RELAENT  = 0x18
DT_RELACOUNT= 0x17fa3   (98211 entries)
DT_JMPREL   = 0xa6d210
```

Entries are `R_AARCH64_RELATIVE` (`info == 1027<<32`), laid out
`{u64 r_offset, u64 r_info, u64 r_addend}`. **`*(r_offset) = r_addend`** (base 0).
This is how you read any BSS/global **pointer table** from the file (e.g.
`0x28c6000` and the CharaSet label pointers). The reagent tool
`apply_relocations` / `memory_read_table` does this if you have a table address.

---

## 3. The engine ↔ script native API (registration table)

The engine registers its global functions in a big routine spanning roughly
**`0x2d4xxx – 0x2ddexx`**. Per function it does:

```
adrp x1, <sig_page>; add x1, x1, <lo>     ; x1 = signature string ("void Foo(…)")
adrp x8, 0x28c6000; ldr x8, [x8, N]       ; x8 = function pointer from the table
... build {ptr, 0, 0, …} on the stack ...
ldr x8,[engine]; ldr x8,[x8,#0x50]; blr x8 ; engine->RegisterX(struct, sig, …)
```

So the **function-pointer table is at `0x28c6000`** (`.data`, zero in the file,
filled by the relocations in §2). To resolve any slot:

1. find the `ldr x8,[x8, N]` in the registration routine (N derived from the slot);
2. read the slot's relocation (`r_offset` = `0x28c6000 + N*8`) → `r_addend` = impl.

Example slots:

| slot | signature string (vaddr) | impl |
|---|---|---|
| `0x28c69c8` | `0xaa2597` `void DrCreateChara(const string&, int, int=0)` | `0x31b01c` |
| `0x28c69d0` | `0xac284d` `void DrCreateChara(const string&, const string&, int=0)` | `0x31b0dc` |
| `0x28c69d8` | `0xadd138` `void DrCreateNpc(const string&, const string&, int, int)` | `0x31b1ac` |

Signature strings live in the **rodata pool ~`0xa79xxx–0xad3xxx`** and are the
**demangled native API** — cross-reference them with the ASB `usedFunctions` to
enumerate the whole script↔engine surface. (The pool stores the *full
declaration*; code points into the middle, so `find("Foo")` misses refs — search
for the string *start*, e.g. `0xaa2597`.)

**Native call sites have no direct xrefs** (calls go through the table), so after
you resolve an impl address you cannot `xrefs_to` it — navigate by impl address.

Resolved impls so far:

| Native / function | Impl |
|---|---|
| `DrSetCameraPosV` | `0x31ff3c` |
| `DrSetCameraZoom` | `0x320050` |
| `DrSetCameraParam` | `0x3202e0` |
| `SetSpriteMoneAnimation` | `0x2f57c0` |
| `DrUiSetPosV` | `0x31ea0c` |
| `SetSpritePosV` | `0x2f5fb0` |
| `DrCreateChara`/`DrCreateNpc` | `0x31b01c`/`0x31b0dc`/`0x31b1ac` |

---

## 4. Camera

The active camera is `*(0x28d5200 + 0x1e8)` (a global manager; `0` = none).

### Object layout (offsets from the camera base)

| Offset | Type | Meaning |
|---|---|---|
| `+0xb0` | vec4 | **position** (x,y,z, `w=1.0`); `+0xbc` = `1.0f` |
| `+0xe0`/`+0xe4`/`+0xec` | vec4 | "current" copy of pos (interpolation) |
| `+0x314` | f32 | visible-rect **left** |
| `+0x318` | f32 | visible-rect **bottom** |
| `+0x31c` | f32 | visible-rect **right** |
| `+0x320` | f32 | visible-rect **top** |
| `+0x324` | f32 | (next float — see open question) |
| `+0x328` | f32 | (next float) |
| `+0x3e8` | f32 | **zoom** (=1000 decimal) |
| `+0x408` | ptr | if nonzero, `DrSetCameraPosV` is ignored (special mode) |
| `+0x468` | u8 | `DrSetCameraParam` enable |
| `+0x46c`/`+0x470` | f32 | the two `DrSetCameraParam` values |
| `+0x474` | u8 | zoom-mode flag |

### Natives

- **`DrSetCameraPosV`** `0x31ff3c(pos_vec, mode, time)`: if `mode != 0` and a state
  check fails → animated via `0x16e88c(cam+0x3e8, cam, &pos, mode, time)`; else
  stores the vec at `+0xb0` (w=`1.0`) and mirrors it to `+0xe0`, clearing
  `+0x464/+0x44c/+0x43c/+0x444`.
- **`DrSetCameraZoom`** `0x320050(z, ?, mode, time)`: `mode != 0` → animated
  (target `+0x3f4`, span `+0x458`(end)/`+0x454`(start), ease `+0x45c`/`+0x460`,
  flag `+0x465`); else `+0x3e8 = z`.
- **`DrSetCameraParam`** `0x3202e0(a,b)`: `+0x46c=a`, `+0x470=b`, `+0x468=1`.

### The view rect — `cam_update_view_rect` `0x16d8f0`

Recomputes the visible **rect** in place (`w8` = a float passed in; `zoom` =
`cam+0x3e8`):

```
cx = (rect.left + rect.right )/2      // +0x314, +0x31c
cy = (rect.top  + rect.bottom)/2      // +0x320, +0x318
half_w = (w8 / zoom) / 2
half_h = (rect.top - rect.bottom) / 2
rect   = { cx ± half_w, cy ± half_h } // written back
```

So the camera is an **orthographic rect**, not an eye: the updater preserves the
rect **centre** and derives only the *x* half-width from `screen_w/(2*zoom)`.
It is the **only** function that writes `+0x314/0x318/0x31c/0x320` with a literal
offset.

### How to reproduce the typing (reagent)

1. `types_create` struct **`TrackInstance`** (see §5 for the layout; the camera
   fields alias into it because `+0x320` is shared).
2. On `0x16d8f0`: `function_variables` → `unaff_x19` (x19) → `type_set` it to
   `TrackInstance *`.
3. On `0x58d1a8`: `function_variables` → `plVar41` (x19) → retype likewise.
4. `field_xrefs(TrackInstance, offset_x)` → shows the read in the renderer and
   the read/write in `cam_update_view_rect`.

### Recorded opening camera keys (`RUNTIME_DUMP=1`)

`pos=(0,-1700)→(0,-2800)→(200,-1000)→(0,-960)→(-1900,0)→(-100,-2000)`,
`zoom 1.4→1.6→1.0`.

### Per-frame update pipeline — `cam_update` `0x16d7b0(cam, mode)`

The camera is an object with a **vtable**; `0x16d7b0` is its method `+0x30`. It
runs, in order: `FUN_0016d99c` (mode / follow-driver reset), `FUN_0016da6c`
(**position**), `FUN_0016dbd4` (**zoom**), `FUN_0016dc7c` (sway A), `FUN_0016e210`
(sway B), then the rect fit. It reads the config singleton via
`func_0x0011ddd0(...)`.

- **`FUN_0016da6c` — position lerp.** Target = `+0xe0..0xec` (the "current" pos);
  when a follow object is bound (`+0x408 != 0`) the target is that object's position
  (its `vtable +0x118`) plus `+0x420/0x424/0x428`. Then
  `+0xb0 = +0x350 + (target − +0x350) * t`, `t = func_0x000307cc(+0x43c)` (tween
  progress); snaps to `target` when the tween reports done (`func_0x000306f8(+0x43c)`)
  or when the global tick `0x28cf100 & 1` fires.
- **`FUN_0016dbd4` — zoom lerp.** `+0x3e8 = +0x3f0 + t * (+0x3f4 − +0x3f0)`,
  `t = func_0x000307cc(+0x450)`, tween end tick `+0x458`.
- **`FUN_0016e88c` — begin pos tween** (`DrSetCameraPosV` with `mode!=0` calls it).
  Records `+0xb0` into `+0x350` (start); sets `+0xe0 = pos * (1/zoom) + cam+0xd0`
  (target); tween at `+0x43c`, end `+0x444 = start_tick + dur`. Ease ramps: if
  `+0x468` is set (`DrSetCameraParam`) → `+0x448=(int)(+0x46c*dur)`,
  `+0x44c=(int)(+0x470*dur)`; else `+0x448=clamp(dur*0.1,0,60)`,
  `+0x44c=clamp(dur*0.9,0,120)`.
- **`DrSetCameraParam(a,b)`** `0x3202e0`: `+0x46c=a, +0x470=b, +0x468=1` — i.e. it
  **overrides the ease-ramp factors** consumed by the position tween (not a free
  "camera param").
- **Sway** `FUN_0016dc7c` / `FUN_0016e210`: periodic 3-channel sine/noise writing
  `+0x488/+0x48c` and `+0x498/+0x49c` (the latter scaled by `1.7/zoom`); gated by
  `+0x480`/`+0x490`, driven by config indices `0x8b4..0x8cc`.
- **`mat_build_camera` `0x5b26e0`** — a large 3D matrix builder (NEON/TBL) on the
  camera object; reads `+0xe0` (pos) and matrices at `+0x240..`, writes many derived
  corner/basis vectors `+0x304..+0x338` (incl. the rect slots). Called from the
  animator/matrix code (`FUN_005b1f34` …), **not** from `cam_update`.

### The view rect is `set_rect(center = pos, …)` — helper `0x57b788`

The trailing calls in `cam_update` are `FUN_0057b788(out, cx, cy, w, h)` which writes
`out = {cx−w/2, cy−h/2, cx+w/2, cy+h/2}` (min/max-normalised) — i.e. **`set_rect`**.
`cam_update` calls it three times on the rect (`+0x314`):

1. `center = (left+right)/2, (top+bottom)/2`, `w = 1840/zoom`, `h = top−bottom`
2. same centre, `w = 1000/zoom`
3. **`center = pos (+0xb0)`**, `w = right−left`, `h = top−bottom`

So the view rect ends up **centred on the camera `pos`**; the first two passes
(rescaling by `1840/zoom` then `1000/zoom`) are why the naive read looked
"centre-preserving". Two widths exist: **1840** (single `.data` constant `0x27c8948`)
and **1000**; the y extent is taken from the previous rect, not zoom.

**Screen width = 1840** (`half_w = 1840/(2*zoom)`); a second pass uses `1000/zoom`.

### View size & clamp — `FUN_000c3380(obj, pos_vec4, &zoom)`

The field view window is **`1920/zoom × 1080/zoom`**, centred on `pos`, then
**clamped** so it stays inside the object's content bounds (`obj+0x33c` l,
`+0x340` b, `+0x344` r, `+0x348` t; sentinels ±100000). `zoom` is floored at
`max(1920/(r−l), 1080/(t−b))`. So the design space is **1920×1080**, and the field
camera is a zoomable window over it. (`cam_update`'s `1840`/`1000` are a different,
narrower pass.)

### OPEN (camera)

1. **`pos → view`** — *solved*: the rect is `set_rect`'d centred on `pos` (above).
   Remaining: what the two widths (`1840` vs `1000`) select, and how the UI/sprite
   layer consumes the rect (the draw-time transform, §7).
2. **The sprite offset is per-`TrackInstance`, not the camera** — `sprite_build_matrix`
   adds `inst->offset_{x,y,z}` (`+0x320/324/328`, §6) where `inst = [element+0xa8] =
   the owning TrackInstance` (§5). So the sprite pan is a per-track-instance
   translation; its **writer is unlocated** (vector stores, no literal `str`).

---

## 5. Animation engine

Call chain for `SetSpriteMoneAnimation(name, track)`:

```
0x2f57c0  SetSpriteMoneAnimation   set track (+0x230), flag all elements
  -> 0x4ca3c  sprite_set_track      FUN_005919f0(dt, obj+0x230); FUN_00592010(obj+0x230,1)
    -> 0x5919f0 FUN_005919f0        TrackInstance vtable: +0x78 (get), +0x80 (set/rebuild),
                                    +0x88 (post), then anim_eval_track
      -> 0x590e00 anim_eval_track   (re)build the track's element array
        -> 0x58bf58 anim_init_element  init one 0xf0 element from a sequence
0x5acb10  anim_update(dt, obj)      per-frame walk of [obj+0x70 .. obj+0x78] (stride 0x340)
                                    -> inst->vtable[+0xb8](inst)   (per-track element advance)
                                    else FUN_00591c10(inst, inst->track); anim_eval_track
```
Note the **TrackInstance lives at `obj+0x230`** (`sprite_set_track` operates on it),
while `anim_update` walks a *separate* array at `[obj+0x70]`. The per-track update is
the **virtual `inst->vtable[+0xb8]`** — its implementation is the element advance
(where JUMP/loop/once live) and is **not yet located** (the TrackInstance vtable's
methods are wrappers outside the anim module; a `+0xb8` search is open). `anim_update`'s
caller is `FUN_005ace40` (the large per-sprite update/render).

### `TrackInstance` (struct, size `0x340`) — the per-track runtime object

| Offset | Type | Field |
|---|---|---|
| `+0x248` | u32 | `track` (current track index) |
| `+0x24c` | u32 | `prev_track` |
| `+0x268` | ptr | `elements` (array of `0xf0`-byte sequence elements) |
| `+0x270` | u16 | `count` (element count) |
| `+0x2f4` | u32 | `flags` — **render-mode / visibility bitfield** (see §5.3) |
| `+0x320` | f32 | `offset_x` (added to sprite translation at render, §6) |
| `+0x324` | f32 | `offset_y` |
| `+0x328` | f32 | `offset_z` |

`anim_eval_track` (`0x590e00`) indexes **section_9** (stride `0x30`:
`sa_sequence_start@+0x28`, `sa_sequence_count@+0x2a`, `disabled@+0x2f`) and, **if `disabled==0`**,
allocates `count*0xf0` bytes and creates **one element per section_a sequence**
(`sa_sequence_start + i`). `anim_init_element` (`0x58bf58`) then indexes **section_a**
(stride `0x18`) and **section_8** (stride `0x20`), reading `s8_start_offset@0x13`,
`sa_track_slot@0x14`; `s8_duration_ticks@+6`, `flags@+8`.

`anim_update` (`0x5acb10`) takes `dt` in `s0`, accumulates a timer (`obj+0xd0`),
derives an integer **frame step** (`>>6` + magic multiply = `dt / frame_ms`),
then walks an array of track instances (`arr = [obj+0x70]`, stride `0x340`) and
calls `anim_eval_track(inst, inst->track)` for each.

**Consequences for our viewer**
- The engine materialises one element **per sequence of the selected track**;
  `sa_sequence_count` drives the count.
- Multi-track applies to **true backgrounds only** (`BattleBG/`, `FieldBG/`): the
  engine animates several `TrackInstance`s (windmill+grass+sky) and the viewer must
  instantiate all their enabled tracks (fixed the empty capitals montage). **Do not**
  generalise this to all non-character assets — empirically it freezes the
  GUI/opening emblem and suppresses the title sequence; GUI assets use the
  script-selected track only.
- Whether the per-track elements advance in parallel or one is selected is still
  open; the per-frame updater is `anim_update` + `anim_eval_track`.

### MBS section field offsets the engine reads (cross-check our parser)

```
section_9  (track)    stride 0x30
  sa_sequence_start    u16 @0x28
  sa_sequence_count    u8  @0x2a
  sa_main_sequence  u8  @0x2b
  sa_sb_start u16 @0x2c
  sa_sb_count u8  @0x2e
  disabled     u8  @0x2f
section_a  (sequence) stride 0x18
  s8_frame_start u16@0x0  s8_frame_count u16@0x2  s8_total_ticks u32@0x4  s8_intro_ticks i32@0x8
  commit_ticks i32@0xc  sb_id u16@0x10  sb_no u8@0x12  s8_start_offset u8@0x13
  sa_track_slot u16@0x14
section_8  (frame)    stride 0x20
  s6_keyframe_id u16@0x0  s7_transform_id u16@0x4  s8_duration_ticks u16@0x6  flags u32@0x8  s8_loop_target u16@0xc
```

### The `0xf0` element — `anim_init_element` `0x58bf58(elem, seq_idx, inst)`

```
element (0xf0 bytes):
  +0x80/0x88/0x90/0x98 = 0
  +0xa8 = owner TrackInstance*        (== the "inst" the renderer offsets!)
  +0xb0 = s8_duration_ticks * 1000               (u32; ms per frame at 1x)
  +0xb4/+0xbc/+0xc4 = 0
  +0xc8 = section_8.s8_duration_ticks            (u16, @s8+6)
  +0xca = sequence index              (u16)
  +0xcc = section_a.sa_track_slot          (u16, @sa+0x14)
  +0xd0 = element index               (set by anim_eval_track)
  +0xd2 = section_a.s8_start_offset             (u8,  @sa+0x13)
  +0xd4 = 0xffffffffffffffff
  +0xdc = 0xffffffff ; +0xe0 = -1.0
  +0xec = element flags
```

Indexing: the **atlas/MBS object** holds the section bases —
`atlas+0xf0 = section_8` (stride `0x20`), `atlas+0xf8 = section_9` (stride `0x30`),
`atlas+0x100 = section_a` (stride `0x18`). `anim_init_element` resolves
`section_a[seq_idx]` → `section_8[s8_frame_start + s8_start_offset]`.

`anim_eval_track` also: sets `track = -1` sentinel, `count = section_9.sa_sequence_count`,
allocates `count * 0xf0`, stamps each element's index at `+0xd0`, and
`field_0x2f0 = count * max(track+0x71, mirror+0x71)`; `elements` is `[inst+0x268]`.

### `TrackInstance.flags` (`+0x2f4`) — bitfield

Mask convention is **`mask(i) = 1 << i`**, from the rodata bit table
**`0x28a28d0`** (`0x28a28d0 + 4*i = 1<<i`, verified). A **runtime** table
`0x28c30d0` (BSS) is indexed the same way and used by both `anim_eval_track` and
`sprite_build_matrix`.

- `anim_eval_track` composes: `flags = (mask0 | flags & ~mask16) & ~(mask12|mask13)`;
  transiently `|= mask24` around element rebuild; `|= mask16` when
  `section_9.disabled & 1` → **bit 16 = disabled**.
- `sprite_build_matrix` gates render passes on `flags & mask(18) / mask(22) /
  mask(25) / mask(26)` and on `0x28c30d0[1]/[2]/[0x1a]`; one block copies material
  slots (`inst+0xc0..0xcb` selecting pointers at `[inst+0xa0..]`) when the bit is set.
- `anim_init_element` tests `flags & mask(23)`; if clear it sets element flag
  `mask(4)` on `elem+0xec`.

### OPEN (animation)

1. Resolve the runtime table `0x28c30d0` (find its initializer) to **name** the
   flags bits (0/1/2/12/13/24 live only there).
2. The `inst->offset_{x,y,z}` (`+0x320..`) writer (per-instance render offset).
3. `sa_main_sequence`, `commit_ticks` — still never observed being read.

---

## 6. Sprite render path

`sprite_build_matrix` `0x58d1a8` (per-sprite world→clip) is the one to know:

```
@sprite_build_matrix+0x14 (0x58d7bc):
  s3 = [inst+0x320]; s4 = [inst+0x324]; s5 = [inst+0x328]   ; inst = [element+0xa8]
  out.x += s3; out.y += s4; out.z += s5
```

So the renderer adds a **uniform translation to every sprite**, read from the
object at `element+0xa8` — the element's owning **transform node**.

### Re-derivation: sprites are translated in the camera's node graph

- `sprite_build_matrix` (`0x58d1a8`) begins `plVar41 = *(TrackInstance**)(element + 0xa8)`
  (confirmed: `anim_init_element` stores `elem+0xa8 = anim_inst`) and offsets every
  vertex by `plVar41->+0x320/324/328`.
- The **same** `+0x320..0x328` triple is (re)written as a *derived world translation*
  by **`mat_build_camera` `0x5b26e0`** — the generic transform-node updater (reads the
  node's `pos (+0xe0)` and 4×4 basis `+0x170/+0x190`; reached via the setter
  `FUN_005b37ac` = `b FUN_005b26e0`, and the node ctor `FUN_005b1f34` ← `FUN_00583c00`).
- `cam_update` writes its own `+0x320` (rect top) and `FUN_0057b788` centres its rect
  on `pos` (§4).

So the sprite's translation is **its node's derived world transform**, produced by the
same updater the camera runs — i.e. the sprite is transformed in the camera's node
graph (translated *relative to the camera*), rather than by a literal camera
subtraction in the draw code. That is why `sprite_build_matrix` "looks camera-agnostic":
the camera contribution is already baked into each node's `+0x320`.

> Remaining: confirm the node hierarchy (element-owner node as a child of the camera
> node). The translation mechanism itself (node `+0x320` from `mat_build_camera`) is
> established.

### Hitbox interpolation and the `0x50` table

`FUN_0058e82c` selects adjacent `section_8` frames and calls
`sprite_build_matrix` (`0x58d1a8`). Interpolation is split by section:

| Tier | Function | Meaning |
|---|---|---|
| `s7_interp` | `FUN_0058c0c0` / `FUN_0058c1e8` | transform interpolation (light / heavy) |
| `s6_interp` | inline in `sprite_build_matrix` | keyframe bounds interpolation |
| `s5s3_interp` | `FUN_0058c560` / `FUN_0058c700` / `FUN_0058cab0` | **hitbox** interpolation |

At the call site (`0x58d8d8`–`0x58d938`):

```asm
ldr x2,[atlas + 0xd8]     ; section_5, 8-byte hitbox references
ldr x3,[atlas + 0xc8]     ; section_3, 0x50-byte hitboxes
...
ldrb w8,[x21, #0xe]       ; section_8.s5s3_interp: 0, 1 or 2
bl 0x58c560               ; variant 0
bl 0x58c700               ; variant 1
bl 0x58cab0               ; variant 2
```

The functions are gated by `section_6.s5_hitbox_count` (`+0x18`) and index
`section_5[entry].s3_id` into the `0x50`-byte `section_3` rows. They do **not**
read `section_4`. For the opening title `s6.s5_hitbox_count == 0`, so this path never runs.

`FUN_0058bc9c` confirms the asset header order. It copies:

```text
+0xb0  section_0  count +0x54  stride 0x18
+0xb8  section_1  count +0x58  stride 0x30
+0xc0  section_2  count +0x5c  stride 0x30
```

so the pointer table continues:

```text
+0xc8  section_3   (0x50 hitboxes)
+0xd0  section_4   (0x14 keyframe layers)
+0xd8  section_5   (0x08 hitbox references)
+0xe0  section_6   (0x1c keyframes)
+0xe8  section_7   (0x24 transforms)
+0xf0  section_8   (0x20 frames)
+0xf8  section_9   (0x30 tracks)
+0x100 section_a   (0x18 sequences)
```

**Transform hand-off (corrected).** `sprite_build_matrix` does not build a draw
object from `section_4` directly. It obtains a pointer from the global draw group
(`+0x280`) or, as fallback, `TrackInstance + 0x20`, then calls
`render_object_set_material` (`0x58e6ec`). That function copies **0x60 bytes** into
the render object. `TrackInstance + 0x20` is initialized as an identity 4x4 matrix
by `FUN_0058ed4c`, so this fallback is a transform/constant block, not a
`section_4` material selector. The global `+0x280` path still needs tracing, but
the earlier "embedded material" reading of `TrackInstance + 0x20` was wrong.

**`section_4.s4_source_tile_id` is not an index into the `0x50` table.** The earlier
material-table reading was wrong: the `0x50` rows are `section_3` hitboxes, and
`s4` is a separate 0x14-byte layer row.

### Mone / text sprite path (`UcSpriteMone`, `UcSpriteText`)

`UcSpriteMone` is constructed at `0x4c874` (object size `0x870`, vtable at
`0x2716e80+0x10`, name string `"UcSpriteMone"`) and constructs one TrackInstance
at `this + 0x230`. `UcSpriteText` derives from it at `0x4c19c` and additionally
constructs a TrackInstance at `this + 0x3f0` (`FUN_00041fc8(this+0x7e)`) and a
text-layer container at `this + 0x1f0`.

The old "Mone update/render" names cover the **text** path:

```text
0x4c618  UcSpriteText update (not plain Mone)
  -> 0x5acd34  build design-space transform
    -> 0x5ace40  UcSpriteText render/update
       -> 0x5acb10  anim_update (text elements)
```

`UcSpriteMone_base_ctor` zeroes `this + 0x154`; `UcSpriteMone_render` writes
`element + 0x154 = this + 0xb0` for each 0x340-byte element. That is the
render-queue priority source used by `sprite_build_matrix`, not `s4.s4_source_tile_id`.

`UcSpriteMone` embeds a renderer subobject at `+0x40` (destroyed by
`FUN_0004ef00`). `UcSpriteMone_render` iterates that renderer's **layer array at
subobject `+0x40` / `+0x48`**. Each record is **0x64 bytes** (the loop uses
`lVar22 += 100`) and carries at least:

```text
+0x3c, +0x40, +0x44, +0x48, +0x4c, +0x50   virtual getters (UV/colour/aux)
+0x60                                      texture/slot released by 0x5aa630
```

The renderer calls virtual layer methods at vtable `+0x50`/`+0x58` (draw) and
`+0x60`/`+0x68`/`+0x70`/`+0x78` (layer data). `FUN_005aa630` is the layer-array
destructor; it releases `*(u16 *)(layer+0x60)` for each 0x64-byte record.

The **builder that fills the renderer subobject's layer array from `section_4`**
has not been located yet.
`UcSpriteMone_clear_layers` clears the old layer array
(`0x5aa260` → `0x5aa630`) when the track changes, so the build must happen around
the track-change path or the Mone constructor. This is the next concrete RE
target for the colored title.

Ghidra names applied during this pass:

```text
0x4c874  UcSpriteMone_ctor
0x4c618  UcSpriteMone_update
0x4ca3c  sprite_set_track
0x4b7f0  UcSpriteMone_base_ctor
0x4b9c0  UcSpriteMone_bind_task
0x4c9dc  UcSpriteMone_init_track
0x4dcac  UcSpriteMone_factory
0x2f4f10 CreateSpriteMone_impl
0x5acd34 UcSpriteMone_build_transform
0x5ace40 UcSpriteMone_render
0x5aa184 UcSpriteMone_clear_layers
0x5aa260 UcSpriteMone_destroy_layers_thunk
0x5aa630 UcSpriteMone_destroy_layers
0x5a9314 text_layer_base_ctor
0x5a9460 text_layer_container_dtor
0x58e82c sprite_ease_and_build
0x58d1a8 sprite_build_matrix
0x58c0c0 s7_interp_linear
0x58c1e8 s7_interp_cubic
0x58c560 s5s3_interp_linear
0x58c700 s5s3_interp_light
0x58cab0 s5s3_interp_cubic
0x58bf58 anim_init_element
0x590e00 anim_eval_track
0x5acb10 anim_update
0x58bc9c mbs_asset_bind_s0s1s2
0x58e034 sprite_apply_rotation_scale
0x58d040 sprite_compute_screen_bounds
0x58e5d8 render_object_set_matrix
0x58e65c sprite_render_object_ctor
0x58e6ec render_object_set_material
0x58e7a8 render_object_set_element_bounds
0x57bf78 renderer_global
0x582684 renderer_get_draw_group
0x5b4320 render_queue_submit
0x57b7c8 bounds_transform
0x57f464 render_object_set_texture_region
0x57f474 render_object_set_aux
0x57f480 draw_object_ctor
0x57f848 render_object_ctor
0x57fb58 render_object_set_rect
0x5ae4a0 track_instance_apply_child_offset
0x5afb54 render_object_ctor_2
0x5914f0 track_instance_update
0x591940 track_instance_sync
0x5919f0 track_instance_rebuild
0x5917d0 track_instance_rebuild_if_changed
0x591b4c track_instance_eval_current
0x591b54 track_instance_set_time
0x591c10 track_instance_get_sequence_length
0x592abc track_instance_update_visible
0x592b60 track_instance_set_matrix
0x58be54 anim_element_ctor
0x58e990 anim_element_update
0x591190 anim_element_apply
0x5a16f0 draw_quad_submit
0x5a2c24 draw_quad_batch_1
0x5a2e34 draw_quad_batch_4
0x5c73c4 texture_get_width
0x5c73cc texture_get_height
0x581cf4 matrix4_identity
0x5b1e78 render_object_base_ctor
``` For the opening title, the colored layer is
`s4_source_tile_id=104`; its white-mask companion is `105`; nearby `103`/`109`
layers are additional masks. Nothing in the `sprite_build_matrix` path reads
`atlas+0xd0` or `s4.s4_source_tile_id`, so the engine's exact material selector
remains open.

The opening title is a useful discriminator:

- The colored title layer is `s4_source_tile_id=104` (`flags=1`, `blend=0`) and
  samples the **lower** half of `GUI_Opening010.ftx`.
- Its white-silhouette companion is `s4_source_tile_id=105` (`flags=1`,
  `blend=1`) and samples the **upper** half.
- The viewer’s raw `s4`/`s1` path draws both; additive compositing of the white
  layer erases the colored title. Skipping the `flags==1`/`blend==1` mask
  companion exposes the colored layer correctly at 16 s.
- A vertical UV flip or a constant V offset reaches the colored pixels but does
  not preserve the title layout; the colored composition is not a vertical
  translation of the mask.

Viewer diagnostics for this work:

- `s4_source_tile_id`, `s4_flags`, `color_id`, `alt_color` in `draw-<frame>.ndjson`
- `FTX_GPU_DUMP_DIR` / `FTX_GPU_DUMP_MATCH` for atlas inspection
- `VIEWER_RUN_ORDER` and `VIEWER_FLIP_V` for ordering/orientation experiments

Rejected experiments: mapping `s4_source_tile_id` directly to raw `section_4`; a raw
`s6`/`s8_render_value` layer crossfade; a global/per-object V flip; and a constant V
offset. None produced a correctly aligned colored title, and the raw crossfade
introduced black interpolation polygons.

---

## 7. Position systems (two of them!)

| Native | impl | target | writes |
|---|---|---|---|
| `DrUiSetPosV` | `0x31ea0c(vec, dur, …)` | the **UI singleton** (`func_0x001759a0(*0x28c31e8)`) | `+0x1900`/`+0x1904`/`+0x1908`; animated: start `+0x2390/0x2394`, target `+0x2398/0x239c`, tween `+0x23a8`/`+0x23ac`/`+0x23b0` |
| `SetSpritePosV` | `0x2f5fb0(name, …, vec, …)` | a sprite found by name (`func_0x0004da40(func_0x002e40a0(), name)`) | `+0x110`/`+0x114`/`+0x118` (immediate) or tween via `func_0x0004c024` |
| `DrUiSetScale` | `0x31e99c(s, …, dur)` | same UI singleton | **scale `+0x1970`**; animated start/target `+0x23b4`/`+0x23b8`, tween `+0x23c4`/`+0x23c8`/`+0x23cc` |

The key distinction is the **target**, not the field: `DrUiSetPosV` drives one
shared UI object; `SetSpritePosV` addresses a named sprite. Both are gated by the
game-state tick (`0x28cf100`): if a tween is in flight the position is queued
rather than written. If the view looks wrong, suspect a mix-up between the two —
and note the script's positions may be screen-relative.

### The UI element object

`func_0x001759a0(manager, name)` resolves a named element from the manager's list
(`manager+0x270`, `0x20` stride; name at `obj+0x23e8`). The element is an
**anim/sprite object** (ctor `FUN_000f26ec`; primary vtable `0x2736680`, whose
`+0x28`/`+0x30` are anim-state ops and `+0xa0` the per-sequence material set).

| Offset | Meaning |
|---|---|
| `+0x1900` | **position** (2–3 floats) |
| `+0x1970` | **scale** (2 floats) |
| `+0x170`/`+0x190` | node 4×4 basis (read by `mat_build_camera`) |
| `+0x320` | node derived world offset (render translation, §6) |
| `+0x23e8` | element name |

`+0x1900`/`+0x1970` are only written by the setters (`DrUiSetPosV`/`DrUiSetScale`,
and the shared body `0x1788b4`); the draw reads them **without** a literal
`#0x1900`, so the world→screen container transform (the path that bakes pos+scale
into the node matrix `+0x170`) is still not located.

> Tooling: applying `.rela.dyn` (98211 entries at `0x82a048`) to the session makes
> every vtable/pointer table readable (`reagent apply_relocations`).

### The UI element is a `TrackInstance` + UI fields (container-transform progress)

The UI element (ctor `FUN_000f26ec`) **embeds a `TrackInstance` at `+0xa0`** — e.g.
its `flags` land at element `+0x394` (`+0xa0 + 0x2f4`), which is where the UI
update writes `*(g_render_flag_mask_table+0x14)` masks. So the element *is* an anim node.

Per-frame update — `FUN_00179a44(elem)`:

```
plVar6 = [elem+0x1ce0]                       ; follow/inherit target (may be null)
switch ([elem+0x1d18]) {                     ; position source mode
  case 1:  [+0x1900] = plVar6[0x33c]; [+0x1904] = [plVar6+0x19e4]
  case 2:  FUN_00179d88(elem)
  case 3/4: hit-test the UI list (FUN_000d1f3c) at a probed point
  default: (*plVar6)[0x118](&pt, plVar6)     ; ask the target for its position
}
[elem+0x1900..] = pt                         ; UI position
; effective node position = pos + pivot(+0x1920..)
[elem+0x250] = [+0x1900] + [+0x1920]
[elem+0x254] = [+0x1904] + [+0x1924]
[elem+0x258] = [+0x1908] + [+0x1928]
; effective scale
if (flags(+0x1c70) & (1<<7)) scale = FUN_00032ca4(+0x1900); else scale = [+0x1970..]
[elem+0x280] = scale.x ; [elem+0x284] = scale.y ; [elem+0x288] = {1.0, 1.0}
if (flags & (1<<6)) FUN_00179e88(elem)       ; corner projection (see below)
FUN_00592abc(elem+0xa0)                      ; TrackInstance update: vtable +0x58,
                                             ;   FUN_005914f0 / FUN_00591190
```

`FUN_00179e88(elem)` computes the element's **screen-space corner rect** by probing
the UI list with `pos ± 50000` through `FUN_00104670` (list walk) → `FUN_000fe3f0`
(occlusion/containment) — that path is a **hit-test/bounds** funnel, not the render
matrix.

`TrackInstance` vtable methods seen so far: **`+0x58`** (called by the UI-node
update), **`+0x78`** (get), **`+0x80`** (set/rebuild), **`+0x88`** (post), **`+0xb8`**
(per-track advance, §5). Mask indices (relative to the `1<<i` table `0x28a28d0`):
`DAT_028a28ec=1<<7`, `0x28a28f0=1<<8`, `0x28a28f8=1<<10`, `0x28a28fc=1<<11`.

> Still open: the actual **screen projection matrix** (design `+0x1900`/`+0x250` →
> clip). `+0xa0` node → `mat_build_camera` → `+0x320` (§6) is the render translation;
> the design→screen scale factor is not yet isolated.

---

## 8. Character appearance data (a solved thread)

**CharaSet record table** — indexed by the `CHARASET` enum (1-based):

```
base 0x2741bb8 (.data)   stride 0x48 (9 × u64, 1388 entries)
```

| word | meaning |
|---|---|
| `[0]` | pointer → Japanese label (`主人公`, `タレ目クレリック`, …) |
| `[1]` | packed `{u8 class_id, u8 gender(1=M,2=F), u8 variant(0..8), u8 pad}` plus `w0c` at `+0x0c` |
| `[2]`,`[3]` | CoA preset / model ids (`record+0x10` = default CoA preset) |
| `[6]` | pointer → `CharaImg<ABBR>` name |
| `[7]`,`[8]` | flags |

| helper | role |
|---|---|
| `FUN_0011a0dc(u32 charaSet)` | `&0x2741bb8 + charaSet*0x48`; 0 if `<=0` |
| `FUN_0011a100(const char*)` | defName → CharaSet index |
| `FUN_00175be0(engine, charaSet, coaPreset)` | the real create |
| `FUN_002bf900(engine, charaSet)` | ~`0x1d0`-byte expanded per-chara def struct |

`class_id` is a `CLASSTYPE` index (`Debug/_UcEnum_Class.inc`, 74 entries).
`w0c` is a recovered per-record value (0..127) whose meaning is not yet known.
Used to generate `viewer/data/charaset_table.tsv` and the build-time `viewer/data/generated/variants.tsv`.

**CLASSTYPE record table** — `g_class_table` at `0x2738ff8` (`.data`):

```
stride 0x58, 74 entries
```

| offset | meaning |
|---|---|
| `+0x00` | `base_class_id` (i32; 0 = none, promoted class → base class) |
| `+0x04` | `w04` (u32; observed 0) |
| `+0x08` | male MBS pointer (`Chara/*.mbs`) |
| `+0x10` | male FTX pointer (`Chara/*.ftx`) |
| `+0x18` | female MBS pointer |
| `+0x20` | female FTX pointer |
| `+0x28` | male icon MBS pointer (`GUI/Icon_*`) |
| `+0x30` | male icon FTX pointer (`GUI/Icon_01/02/03.ftx`) |
| `+0x38` | female icon MBS pointer |
| `+0x40` | female icon FTX pointer |
| `+0x48..+0x54` | four opaque u32 fields (retained raw) |

`class_table_relation_matches(class, target)` returns true when
`class == target` or `g_class_table[class].base_class_id == target`; the
script-facing callers use it for class-family/promotion checks. The raw dump is
`viewer/data/class_table.tsv`.

**NPCTYPE record table** — `g_npctype_table` at `0x273a968` (`.data`):

```
stride 0x50, 320 entries
```

| offset | meaning |
|---|---|
| `+0x00` | pointer → Japanese label |
| `+0x08` | model MBS pointer (`CharaNpc/*.mbs`) |
| `+0x10` | model FTX pointer (`CharaNpc/*.ftx`) |
| `+0x18` | icon MBS pointer (`GUI/IconNpc_*`) |
| `+0x20` | icon FTX pointer (`GUI/IconNpc.ftx`) |
| `+0x28..+0x48` | opaque record data (often `0xffffffff` sentinels; retained raw) |

`npctype_get_record(index)` returns `&g_npctype_table + index*0x50`;
`npctype_find_by_label` compares the Japanese label at `+0`. The raw dump is
`viewer/data/npctype_table.tsv`.

### 8a. Appearance/model tables adjacent to the CharaSet cluster

These sit immediately before/after `g_class_table`, `g_npctype_table`, and
`g_charaset_table` and were recovered while checking the raw-table coverage.

| Table | Base | Stride | Count | Layout / accessor |
|---|---|---|---|---|
| `battlebg_variant` | `0x27384b8` | `0x18` | 80 | `{u32 id, f32 scale, char* bbs, char* label}`; referenced by `bbs_table` `ptr_a`/`ptr_b` |
| `chara_model` | `0x2740d68` | `0x10` | 229 | `{char* label, u16 mask, u16 w0a, u32 w0c}`; `FUN_0011ba28` reads the mask at `+8` |
| `class_part` | `0x275a218` | `0x08` | 729 | ten contiguous pointer arrays -> `0x48` records `{u32 class_id, u32 part_id[17]}`; getters `FUN_00119d0c`, `0x00119de4`, `0x00119e34`, `0x0011ae2c`, `0x0011b3d0` |
| `class_model_m` | `0x275bb30` | `0x08` | 73 | pointer array -> `0x30` records `{u32 class_id, u32 model_id[10], u32 variant_count}`; `FUN_0011b998(param,1)` |
| `class_model_f` | `0x275b8e0` | `0x08` | 74 | same record layout; `FUN_0011b998(param,2)`; many entries are null |
| `chara_part` | `0xceb6a8` | `0x28` | 2168 | ten `{i16 part_id, i16 scale_percent}` pairs; `FUN_001197bc` applies them (scale = value / 100) |
| `class_stats` | `0x275bd78` | `0x130` | 471 | unit/class parameter rows; `id` at `+0`, `class_id` at `+4`, `flags` at `+8`, ASCII `track_name` at `+0xc0`, ASCII `behavior` at `+0xc8`; accessors `FUN_0011d148`, `0x0011c55c`, `0x0011d68c`; record 0 is a sentinel, valid ids 1..470 |
| `class_sub` | `0x277ecec` | `0x100` | 172 | ability/order/effect rows; Japanese `label` at `+0xe4`; `FUN_0011d900`/`0x0011d860`/`0x0011d894`; record 0 sentinel, valid ids 1..171 |
| `class_relation` | `0xd0d15c` | `0x8c` | 74 | per-class ability set; `class_id` at `+0`, `assist_id` and `order_id`..`order_id_4` index `class_sub`, `class_stats_id` at `+0x1c` and `class_stats_id_2` at `+0x80` index `class_stats`, `type` at `+8` is 0/23/24/25; `FUN_0011c140`/`0x0011c260`/`0x0011c4c0` scan the rows |
| `item` | `0x26ea1e8` | `0xb8` | 983 | item rows: `{u32 id, char* name, i32 category, ...}` at `+0/+8/+0x10`; `stat_a`/`stat_b` are i16 at `+0x18`/`+0x1a`; `max_stack` is u16 at `+0x1c`; `price` at `+0x20`; `weight` at `+0x24`; `unit_param_id` at `+0x28`; three 0x20-byte effect records at `+0x2c`/`+0x4c`/`+0x6c`, each with id at `+0xc` and f32 value at `+0x10`; accessors `item_get_record`, `item_get_price`, `item_get_weight`, `item_get_unit_param_id`, `item_get_effect0..2` |

`chara_model` is the model/appearance record selected by the class model
arrays: `FUN_0011b998` returns a class record, `FUN_0011ba28` indexes its
10 model ids by variant (`+4 + variant*4`) and then ORs
`chara_model[id].mask | 0xfe000000` into the character object at `+0x294`.
The model ids are in `0..228`; `chara_model[0]` is the `なし` sentinel.

`class_sub` is the readable ability/effect table. Its `+0xe4` names include
facility effects (`施設：…`), traps, orders (`ブレイブオーダー`, `ロイヤルオーダー`),
and assist skills (`アシスト射撃`, `アシスト魔法`, `飛行`). `class_relation`
stores class_sub indices for `assist_id` (148..171) and
`order_id`..`order_id_4` (74..147), plus two class_stats indices
(`class_stats_id` at `+0x1c`, `class_stats_id_2` at `+0x80`). The two
16-byte-ish action blocks at `+0x20` and `+0x40` are also class_stats indices:
`attack_motion_id_0..3` and `support_motion_id_0..3` resolve through
`class_stats.track_name` to character MBS section-9 track names such as
`S01_ATTACK_*`, `S02_GUARD_*`, `COVER`, `COUNTER`, `FIRST_AID_*`, and
`SKILL_*`. The action ids are class_stats row ids, not direct MBS track
indices. The adjacent small words (`attack_type_*`, `support_type_*`) are
unresolved action-kind selectors.

`class_part` has five part slots (two arrays per slot, M/F) and 729 pointers in
total. The arrays are laid out contiguously from `0x275a218` to `0x275b8e0`:
`{72,69,74,73,74,73,74,73,74,73}` entries. The two defaults are the records at
`0xd00990` (M) and `0xd01680` (F). Each class record's first u32 is the class
id; the remaining 17 u32 are `chara_part` indices. `chara_part` rows are
applied as 10 optional `{model_id, scale_percent}` attachments.

`item` rows start at `0x26ea1e8` (the row id is at `+0`, the name pointer at
`+8`, and the `ITEMCATEGORY` value at `+0x10`). The two signed equipment stats
at `+0x18`/`+0x1a` are category-dependent: weapons use attack/magic values,
shields use defence/evasion-like values, and the item comparison/UI code
(`item_get_sort_stats`) consumes them as two independent stats. `+0x1c` is
the maximum stack (`0` means unstackable), `+0x20` is the price, `+0x24` is
the capacity weight (`0` means the default 100), and `+0x28` is a signed
`class_stats`/unit-param id (`item_get_unit_param_id`). Three 0x20-byte effect
records at `+0x2c`, `+0x4c`, and `+0x6c` store effect id/value pairs at
`+0xc`/`+0x10`; their display path is `item_get_effect0..2`.

### 8a.1 World/UI tables found while scanning the same regions

| Table | Base | Stride | Count | Layout / accessor |
|---|---|---|---|---|
| `field_terrain` | `0x27cf660` | `0x50` | 269 | `key`, `label`, `terrain_type` at `+0x10`, `tint` at `+0x14`, `snow` at `+0x20`, `passable_mask` at `+0x21`, five signed `{attack_modifier, move_modifier}` byte pairs at `+0x42..+0x4b`; accessors `field_terrain_get_record`/`0x0021c794`/`0x0021c7d8`/`0x0021c7f8`/`0x0021c860`/`0x0021c878`/`0x0021c8e0` |
| `quest` | `0x27d4aa8` | `0x40` | 183 | `{char* key, u32 quest_type, u32 quest_id, u32 w10, u32 w14, req0_a, req0_b, req1_a, req1_b, req2_a, req2_b, char* stage_id, u32 flag, u32 w3c}`; `FUN_0021ca88`/`0x0021cab0`/`0x0021cb08`/`0x0021ce10`/`0x0021ce74`/`0x0021cf0c`; `quest_type` is the 0..4 switch value |
| `camera_view` | `0x279f5c0` | `0x38` | 179 | `CV_*` keys plus `flags`, `fov`, `x/y/z`, `yaw`, `asset`, `distance`, `pitch`, `id`, `reserved`; no direct accessor recovered (`FUN_00181070` is a thunk), so the float names are inferred |
| `stage_effect` | `0x285e640` | `0x20` | 185 | `{u32 id, ... , char* name}` with `dsp_out_*` names; `FUN_00327e30` returns the name, `FUN_00327d10` searches by id |
| `unit_name` | `0x2860108` | `0x20` | 149 | rows `0..148`; `{char* key, char* label, u32 w10, u32 w18}`; `FUN_0032edd8`/`0x0032f268`/`0x0032e90c` etc. `w10` is used as a voice/sound index |
| `name_pool` | `0x28613a0` | `0x08` | 104 | string-pointer array immediately after `unit_name`; starts at `unit_name[148]+0x18` |

`quest` rows use a stage-id string at `+0x30` (e.g. `UC_QUESTID_OW_C0` ->
`ST_OW_C_0`), so they join directly to `stage_table`/`stage_cue_table` names.
`field_terrain` keys (`NONE`, `SEA`, `WAR_PLAIN`, `ELF_PLAIN`, ...) are
terrain/battlefield selectors, not stage ids. The `+0x42..+0x4b` modifier
bytes are read as attack and movement multipliers: `FUN_0021c7f8` is used by
damage code, `FUN_0021c878` by movement/pathfinding code, and both return
`(100 - signed_byte) / 100`. `snow` is set on all `SNOW_*` and `*_B`
(Bastorias) rows.

All of the above are dumped raw into `viewer/data`; the exact TSV headers are
listed in `viewer/data/README.md`. Unknown words are intentionally retained as
`w*` columns and will be named one at a time.

> Coverage note: the `encounter`/`stage_map` region around `0x27e2ff8` is a
> graph of stage edges and encounter records, not a clean uniform row table.
> The current `encounter_table.tsv`/`stage_map_table.tsv` are candidate
> projections; re-derive them from the graph before treating their row stride
> as authoritative.

---

## 8b. Global tables, singletons & config

| Address | Role |
|---|---|
| `0x28d5200` | **main game manager** — camera at `+0x1e8`; charas created via `func_0x00175be0(manager, charaSet, preset)` |
| `0x28c31e8` | **UI manager** (the `DrUiSetPosV` target) |
| `0x28cf100` | main game-state/tick object (low bit = "advance this frame") |
| `0x3b375b0` | **config singleton**; `func_0x00594674()` returns it, `func_0x0011ddd0(i)` reads `config[8][i]` |
| `0x28a28d0` | rodata bit table: `+4*i = 1<<i` (so `0x28a2918=1<<18`, `0x28a2928=1<<22`, `0x28a2934=1<<25`, `0x28a2938=1<<26`) |
| `0x28c30d0` | runtime bit-mask table (BSS, initializer unknown) |

`func_0x0011ddd0(i)` is the universal **config getter** — every tuning constant
(camera sway `0x8b4..0x8cc`, scales, etc.) comes from `config[8][i]`. Enumerate the
indices to recover the game's tuning table.

### `DrCreateChara` / `DrCreateNpc`

`DrCreateChara` (`0x31b01c`, `(string, int charaSet, int preset=0)`) →
`func_0x00175be0(*0x28d5200, charaSet, preset)` returns the chara object, then a
name string (≤ `0x3f` bytes) is copied to `+0x2390`. `DrCreateNpc` is `0x31b1ac`.
The chara object shares the big `+0x2390` region with the UI object (common base).

## 9. Strings, symbols, RTTI (navigation aids)

- **AngelScript 2.34.0** embedded (`…/AngelScript/2.34.0/sdk/add_on/…`,
  `RegisterStdString_Generic`, `RegisterScriptMath_Generic`, …) — confirms `.asb`.
- Game source paths leak the tree: `C:/Project2/Unicorn/Soft/Src/Script/
  UcScriptClass.cpp`, `Soft/Build/NX64/Master`, `Soft/Data_JP/DataSheet/*.xlsx`.
- RTTI class names: `UcDrChara`, `UcBtUnitMan`, `UcFlMan`, `UcSpriteMone`,
  `UcUI_*`, `UcSaveDataThumbnailRenderer`, …
- Big string pool (`~0xa79xxx–0xad3xxx`): signatures + JP labels; `Chara/…`
  asset names near `0x1310xxx`.

---

## 10. What we are currently looking for (and how to attack each)

1. **`TrackInstance.offset_{x,y,z}` (`+0x320..`) writer** — the per-instance render
   offset the sprite layer consumes. Attack: `field_xrefs(TrackInstance,
   field_offset=0x320)` after typing more animator functions (the writer uses vector
   stores, so an instruction scan for a literal `str [x,#0x320]` will miss it).
2. **`0x28c30d0`** — *not* an unknown runtime table: it is a **pointer to the
   static `1<<i` mask table `0x28a28d0`** (`[0x28c30d0] == 0x28a28d0`;
   `anim_eval_track` does `puVar7 = g_render_flag_mask_table` then uses `puVar7[i]`). So every
   `TrackInstance.flags` bit is just `1<<i`; naming them needs reading the code the
   bits gate (`sprite_build_matrix` / `anim_init_element`), not a data table.
3. **Render-pass bits** `flags & mask(18)/mask(22)/mask(25)/mask(26)` and bit 23 —
   read the `sprite_build_matrix` / `anim_init_element` blocks they gate.
4. **s8 `flags` bits** (our parser's `0x40`/`0x400`) — cross-check with
   `anim_init_element`: reads `s8_duration_ticks@+6`, `flags@+8`; bit 31 (`< 0`) routes to an
   alternate element type via vtable `+0xa0`.
5. **`sa_main_sequence` / `commit_ticks` / `sb.oneshot`** — still never observed being
   read; confirm the engine ignores them.
6. **`pos → 2D view`** — decide whether `pos` ever reaches the sprite layer (best
   supported answer today: **no**; the pan is `inst->offset_*`), else locate the
   consumer of `mat_build_camera`'s `+0x304..0x338`.

---

## 11. Tooling notes

**reagent (Ghidra MCP)** — `decompile`, `function_variables`, `types_create`,
`type_set` (params **and** decompiler locals), `field_xrefs`, `code_create`,
`rename`, `save`. Workflow that cracked the camera:

1. `code_create(addr, function=true)` (required before `decompile`/`type_set`).
2. `types_create` the struct; `type_set` a function's param/local to `Struct *`.
3. `field_xrefs(Struct, field)` → every read/write across typed functions.

**radare2** — good for `list_functions(filter=…)`, `xrefs_to`, decompile; the MCP
`radare2_*` tools work on the same ELF.

**Sprite-section trace (2026-10-18).** The `unicorn_main` project is healthy;
program `main2.elf`. Recipe for the sprite interpolation path:

1. `xrefs_to` `0x58c560` / `0x58c700` / `0x58cab0` gives the sole caller:
   `sprite_build_matrix` at `0x58d1a8`.
2. Disassemble `0x58d8b0..0x58d944` to see `[atlas+0xd8]` (section_5),
   `[atlas+0xc8]` (section_3), and the `ldrb w8,[x21,#0xe]` variant branch.
3. `decompile` `0x58e82c` for adjacent-frame selection,
   `0x58d1a8` for the s7/s6 interpolation and material hand-off, and
   `0x58c560`/`c700`/`cab0` for **s5/s3 hitbox** interpolation.
4. `decompile` `0x58bc9c` to confirm the asset pointer order
   (`+0xb0` s0, `+0xb8` s1, `+0xc0` s2, `+0xc8` s3, `+0xd0` s4, `+0xd8` s5,
   `+0xe0` s6, ...).
5. `instructions_search_scalars` is useful for `0xc8` / `0xd8` / `0x50` field
   uses; `xrefs_to` remains empty for vtables, so use direct decompilation there.
   Restart the worker if a scan that should have many hits returns zero.

Known issues (see `~/reverse_engineering/feature-request.md`)
- `field_xrefs` only sees **typed** functions → a writer in an untyped function is
  invisible. No "scan all functions for `[reg+off]`" mode.
- `field_xrefs` occasionally reports a **bogus access address** (e.g. `0x57b7c0`,
  a `stp/ret` stub) with an empty function name — trust the `context` line.
- `xrefs_to` is **empty for natives** (table-dispatched) and vtables.
- `analysis_run` + `decompile` concurrently **wedges the worker**; serialize.
- The boot **re-reads the original source path** (`/tmp/opencode/main2.elf`) and
  re-analyses; keep the symlink valid, and re-apply typing if the program is lost
  (the project's program is not always persisted on reload).
- **Do not trust a scan taken from a just-restarted / wedged session.** During one
  turn `instructions_search_scalars #0x1900` returned **3** hits; after restarting the
  worker it returned **270**, and a full capstone disassembly finds **318** (mem
  operands, `#0x1970`: 38 vs 50). So the tool is fine — the tiny result was a stale
  session state. Restart (`session_stop`/`session_start`) before believing a
  suspicious scan.
- Offsets **> 4095** cannot be an `add`/`ldr` immediate, so code reaches them via
  `mov wN,#imm; add xN,xM,xN` (e.g. `#0x23e8`). `instructions_search_scalars` *does*
  cover the `mov` form; an external `[reg,#off]` scan must add it explicitly.
- Vtables / pointer tables in `.data` are **zero in the file**; apply `.rela.dyn`
  (98211 entries at `0x82a048`) first (`reagent apply_relocations`) or they read as 0.

**Offset-access scan (external cross-check).** `/tmp/opencode/scan.py <elf> <off>…`
disassembles the executable PT_LOAD with capstone (`md.skipdata=True`; skip `.byte`
and guard `insn.operands`) and prints every `... [reg, #off]` mem access. Handy to
confirm coverage: `+0x1900` is a *generic* field (hundreds of uses across object
types), so an offset alone does not identify the UI element.

---

## 12. Recipe: recover the camera view (target state)

1. Import the ELF (§1); `code_create` the addresses in §0.
2. `types_create` `TrackInstance` (§5); `type_set` `0x16d8f0`'s `unaff_x19`,
   `0x58d1a8`'s `plVar41`, and `0x590e00`'s `inst` param.
3. `field_xrefs(TrackInstance, offset_x)` → confirm the two accessors
   (`cam_update_view_rect`, `sprite_build_matrix`).
4. Read `cam_update_view_rect` → the rect model (§4).
5. **Still needed:** the rect-centre writer → then the sprite space vs camera
   space question is answered, and the viewer can place the game's camera exactly.

---

## 13. Recipe: recover a native by name

1. Search the string pool for the signature start (`strings` / `search`), e.g.
   `DrCreateChara` → `0xaa2597`.
2. Find the registration code that `add`resses that string (in `0x2d4xxx–0x2ddexx`).
   Next instruction loads `[0x28c6000 + N]`.
3. Resolve the slot (`0x28c6000 + N*8`) via `.rela.dyn` → `r_addend` = impl.
4. `code_create` + `decompile` the impl.

The generator for the appearance tables (`/tmp/opencode/gen_appearance.py`, not
yet in the repo) is the worked example of this recipe end-to-end.
