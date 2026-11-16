# Gradient map shader (NVN text)

Extraction and reading of the engine's gradient-map pixel shaders. This is a
shader-text report: it does not change the viewer.

> **Status.** The formula below is implemented in the viewer
> (`viewer/src/shader.cpp`, `viewer/src/sprite.cpp`): exact luminance weights,
> the tinted base kept for U, `mix(base, ramp, Index)` and V passed already
> normalised. The V value itself is CPU-supplied and its default for an
> uncustomised character has not been located, so the gradient path is wired but
> inactive. The colour block's eight 8-byte part entries are plain data,
> zero-initialised, and no preset/encounter path writes them, which points at a
> saved per-unit override rather than a static default.

## Extraction

Source: `~/zwei/unicorn_overlord/main2.elf`, PT_LOAD 1 (`vaddr 0x82a000`,
file `0x82b000`). A program starts at the ASCII marker `!!NVvp5.0` /
`!!NVfp5.0` and ends at the first `END\n`. Reflection metadata is the
NUL-separated name run in the gap after the program and before the next
`!!NV...` marker.

- **2188 programs** were extracted: 642 vertex and 1546 fragment.
- The earlier count of 679 `CBUFFER buf0[]` prologues is only the subset whose
  *first* buffer is `buf0`; the other 1509 programs begin with a different
  buffer (`buf9[]`, `buf14[]`, no `CBUFFER` at all, …).
- Extractor: `tools/extract_shaders.py` (`--elf`, `--out`). Work directory:
  `mbs_dump/shaders/<vaddr>.asm`, `<vaddr>.symbols`, and `index.txt`.
  `mbs_dump/` is git-ignored.
- 176 programs have a `ConstantBufferGradientMapParts` reflection block.
  812 programs use the same `DP3` luminance constants as the gradient path.
- Canonical parts program: `0x11139a0`. Canonical single-texture variant:
  `0x2251da0`.

## `~/nvdisasm` evaluation

`~/nvdisasm` is a checkout of **envytools** (`envydis`, `easm`, `nva`, …).
`envydis` disassembles *binary* NVIDIA SASS for Tesla/Maxwell/etc.; `easm`
assembles envytools' own SASS syntax. Neither reads the NVN text assembly used
here (`!!NVfp5.0`, `LDC.F32X4`, `TEX.F … handle(D0.x)`), and there is no NVN
text parser in the tree. The compiled SASS blobs interleaved in PT_LOAD 1
could be disassembled with `envydis` if a binary copy were needed, but the text
is the exact program and is more readable. The tools were not used.

## Reflection and constant buffers

`0x11139a0.symbols`:

```
ConstantBufferGradientMapParts
g_GradientMapPartsIndex
g_GradientMapPartsValue
g_GradientMapTexture
g_Texture
vs_COLOR0
vs_TEXCOORD0
SV_Target0
```

Buffer `buf9` is `ConstantBufferGradientMapParts`; `buf14` is the bindless
texture-handle buffer. The same program shows the exact locations:

| location | type | name | use |
|---|---|---|---|
| `buf9[0]` | `float` | `g_GradientMapPartsIndex` | blend weight |
| `buf9[4]` | `float` | `g_GradientMapPartsValue` | V coordinate |
| `buf14[0]` | `u64` handle | `g_Texture` | sprite texture |
| `buf14[32]` | `u64` handle | `g_GradientMapTexture` | `GradientMap00.ftx` |

`buf9` is a two-scalar, 8-byte block, **not** an array. All 176
`GradientMapParts` programs load only `buf9[0]` and `buf9[4]`; none performs an
indexed constant load (`LDC … buf9[R…]`). The eight per-part entries in the
0x58-byte character colour block are a CPU-side structure, not elements of this
shader buffer.

The buffer number is local to a program. Other variants bind the same pair at
`buf4` or `buf13` (`ConstantBufferGradientMap` / `ConstantBufferGlobal`), but
the relative layout is stable: the first scalar (`Index`) is the blend weight
and the second (`Value`) is V.

| family | gradient buffer | blend / V | LUT handle | base extra |
|---|---|---|---|---|
| `ConstantBufferGradientMapParts` | `buf9` | `[0]` / `[4]` | `buf14[32]` | none |
| `ConstantBufferGradientMap` | `buf4` | `[0]` / `[4]` | `buf14[32]` | `g_MaterialColor` at `buf0[80]` |
| `ConstantBufferGlobal` | `buf13` | `[16]` / `[20]` | `buf14[32]` | `g_MaterialColor` at `buf0[80]` |

## Exact formula

`0x11139a0.asm` (`GradientMapParts`, one sprite texture):

```
12  LDC.U64 D0.x, buf14[0];
13  TEX.F R2, fragment.attrib[2], handle(D0.x), 2D;      // tex
14  MUL.F32 R1, R2, fragment.attrib[1];                 // base = tex * vs_COLOR0
15  LDC.F32 R0.x, buf9[4];                             // Value
16  DP3.F32 R0.z, R1, {0.223499998, 0.631299973, 0.0588000007, 0};
17  MOV.F R0.w, R0.x;                                  // V = Value
18  LDC.U64 D0.x, buf14[32];
19  TEX.F R0.xyz, R0.zwzw, handle(D0.x), 2D;           // ramp = (U,V)
20  MAD.F32 R2.xyz, R2, -fragment.attrib[1], R0;       // ramp - base
21  LDC.F32 R0.x, buf9[0];                             // Index
22  MAD.F32 result_color0.xyz, R0.x, R2, R1;           // base + Index*(ramp-base)
23  MOV.F result_color0.w, R1;                         // alpha from base
```

So:

```
base.rgb = vs_COLOR0.rgb * texture(g_Texture, uv).rgb
U        = dot(base.rgb, vec3(0.223499998, 0.631299973, 0.0588000007))
V        = g_GradientMapPartsValue
ramp.rgb = texture(g_GradientMapTexture, vec2(U, V)).rgb
out.rgb  = base.rgb + g_GradientMapPartsIndex * (ramp.rgb - base.rgb)
out.a    = vs_COLOR0.a * texture(g_Texture, uv).a
```

`Index == 1` is a full replace with the ramp; `Index == 0` leaves the sprite
colour unchanged. The ramp's alpha is never read (`TEX.F R0.xyz`).

The single-texture `ConstantBufferGradientMap` variant (`0x2251da0`) has the
same U/V/combine instructions, but its base also mixes a material colour:

```
15  MUL.F32 R2, fragment.attrib[1], R1;                 // vs_COLOR0 * tex
16  LDC.F32X4 R0, buf0[80];                             // g_MaterialColor
17  MAD.F32 R0.xyz, -fragment.attrib[1], R1, R0;        // g_MaterialColor - tex*vs_COLOR0
18  MAD.F32 R1.xyz, R0.w, R0, R2;                       // base = mix(tex*vs_COLOR0, material, material.a)
```

Thus, in variants that have `g_MaterialColor`, the luminance `U` is computed on
the *post-material-mix* colour, not on the raw texture. The multi-texture
variants (`0x111b2b0`) additionally multiply a second `g_MultiTexture` sample
after the gradient, but do not change the U/V/combine path.

## Band, appearance, part index, colour channels

### Band V

The shader uses `g_GradientMapPartsValue` **directly as the texture V
coordinate**. It does not multiply by 4, divide by 2048, or read
`appearance[]`. Therefore the CPU must supply the normalised band-centre
coordinate:

```
V = (band * 4 + 2) / 2048
```

if `band` is the 0..511 LUT band. A raw band number in `buf9[4]` would be
sampled as V in `[0,1]`, i.e. in the first band.

The shader cannot confirm or refute the viewer's `gradient = 226 +
appearance[2]`; that value is produced on the CPU. The appearance bytes are
also used as **asset indices**: `chara_color_block` copies them to
`+0x10..+0x17`, and `FUN_0052cfc4` (0x52cfc4) loads eight different asset
tables with `appearance[0..7]` as indices. That does not rule out a dual use as
a ramp selector, but the shader itself only sees the already-computed
`g_GradientMapPartsValue`.

### Part index

There is no element index in the shader. `g_GradientMapPartsIndex` is the
scalar blend weight at `buf9[0]`; `s4.color_id` is never used as a shader array
subscript.

On the CPU, `sprite_layer_draw` (0x57dd40) reads the section-4 entry byte at
`+4` (`s4.color_id`) and uses it to index the per-part colour table at
`render_object + 0x1f0 + color_id*4`. `chara_part_apply` (0x1197bc) writes
**11** entries (not ten) at `chara + 0x448 + i*0xc`:

```
+0x00  float  scale = percent * 0.01
+0x04  short  part_id (0 when the source pair is -1)
+0x08  int    slot number 1..11
```

Those are mesh-part selectors, not gradient-map elements.

### The 0x58-byte colour block and the fourth channel

`chara_color_block_init` / `chara_color_block_apply_preset` (0x2c2be4 /
0x2c2e08) define:

```
+0x00  uint32  colour0        preset +0x08 (low 24 bits; alpha from DAT_028c3950+3)
+0x04  uint32  colour1        preset +0x0c
+0x08  uint32  colour2        preset +0x10
+0x0c  uint32  spare          init 1, not touched by the preset path
+0x10  uint8[8] appearance    preset +0x00..+0x07
+0x18  8 × pointer            lazily built part entries
```

The preset row has **four** colours at `+0x08`, `+0x0c`, `+0x10`, `+0x14`.
`chara_color_block_apply_preset` copies only the first three. The fourth
(`+0x14`, colour3) is returned by `FUN_00035b78` (0x35b78) and is stored in a
separate ARGB field, not in the 0x58-byte block: e.g. `FUN_003f523c` (0x3f523c)
writes it to `object + 0x23bf` with a valid byte at `+0x23be`, and
`FUN_00376fec` (0x376fec) writes it to `object + 0x749f` with a valid byte at
`+0x749e`. The block's `+0x0c` spare is a separate value (initialised to 1 and
overwritten from an encounter/colour parameter in `FUN_002c2f5c`); it is not
preset colour3.

Matching the script names to the observed layer roles (the viewer's validated
A/B/C/D sweep at `Dr_OW_C_0` frame 420) gives:

| script channel | storage |
|---|---|
| `main` | colour0, block `+0x00` |
| `sub` | colour1, block `+0x04` |
| `common` | colour2, block `+0x08` |
| `hair` | colour3, preset `+0x14`, separate field (not in the block) |

`color_id 1` selects colour1 (dress/sub), `color_id 2` selects colour3
(hair), `color_id 3` selects colour0 (torso/main); `color_id 0` keeps the
section-0 colours. The preset row's physical order is therefore **main, sub,
common, hair**, not the script parameter order `(common, hair, main, sub)`.
This is consistent with the block/preset layout and the separate colour3 path.

## Viewer change

The viewer's current path in `viewer/src/sprite.cpp` / `shader.cpp` should be
changed as follows.

1. **Luminance weights.** Replace `vec3(0.299, 0.587, 0.114)` with the exact
   shader constants:

   ```
   const vec3 W = vec3(0.223499998, 0.631299973, 0.0588000007);
   float lum = dot(base.rgb, W);
   ```

   The `0.299/0.587/0.114` constants occur in only one of the 2188 programs
   (`0x2444470`, a blur/colour pass), not in the gradient family.

2. **Use the real pre-gradient colour for `U`.** The shader computes `U` from
   `base = tex * vs_COLOR0` (and, in material-colour variants,
   `mix(tex*vs_COLOR0, g_MaterialColor, g_MaterialColor.a)`). Forcing the
   vertex colour white before the luminance dot changes `U` and therefore
   selects a different ramp sample. Keep the actual flat/vertex colour for
   the dot; the flat colour does not multiply the ramp in the shader.

3. **Combine, do not blindly replace.** At `Index == 1` the result is the
   ramp, but the general form is `out = base + Index*(ramp - base)`. Use the
   Parts `Index` value (normally 1.0) rather than assuming replacement.

4. **Pass V, not the band.** If the viewer keeps `band = 226 +
   appearance[2]`, upload `V = (band*4 + 2)/2048`; the shader samples V
   directly. The shader does not prove the `226 + appearance` band; the CPU
   side that fills `g_GradientMapPartsValue` still needs to be traced (the
   candidate native is `SetCharaGradientMapCommon`).

5. **Material colour variants.** If the viewer models `g_MaterialColor`, mix it
   into `base` before the luminance dot as above; otherwise the single-texture
   `base = tex * colour` path is the correct reduced form.
