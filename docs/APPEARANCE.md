# Character appearance (CharaSet → model mask)

How the viewer selects a character's MBS layers. See `scarlett/README.md` for
the wider drama/script picture.

## The engine rule

The renderer draws an `s4` layer when both are true:

```c
(s4.flags & 0x02) == 0                    // not the engine's SKIP bit
(s4.attributes & appearance_mask) == 0    // no suppressed attribute bit
```

This is a **suppression** test, not an allowlist: bits set in the mask remove
layers that carry them. `sprite_layer_draw` (`0x57dd40`) applies it while
walking `atlas+0xd0` (`section_4`, stride `0x14`), and reads the mask from the
render object at `+0x1cc`.

The render object mask is built in `sprite_build_matrix` (`0x58d1a8`):

```c
render_object[0x1cc] = element[0xc4] | chara[0x294];
```

`element[0xc4]` is zero on the normal character paths. `chara[0x294]` is set by
`chara_apply_model_mask` (`0x11ba28`):

```c
model_id = class_model_get_id(class_id, gender, variant);
chara[0x294] = chara_model[model_id].mask | 0xfe000000;
```

- `class_model_get_id` (`0x11b9e8`) clamps `variant` to `0..9`, reads
  `class_model_m[class_id].model[variant]` or `class_model_f[...]`, and returns
  `0` when the class has no model record.
- `chara_model` is the 229-row table at `0x2740d68`; its `mask` field is 16 bits.
- `0xfe000000` suppresses the high attribute groups (`0x4000000..0x80000000`),
  which are alternate/effect groups rather than base character parts.

Therefore the engine key is `(class, gender, variant)`, and the model mask is
the only per-character layer selector. The mask itself lives in the executable,
not in the MBS file.

## Generated data

`viewer/data/generated/charaset.tsv` carries the final mask and the colour
preset index as its eighth and ninth columns:

```text
idx name class gender variant charaImg label appearance_mask color_preset
```

`tools/build_viewer_data.py` joins the raw `CharaSet` row with
`class_model_m/f_table.tsv` and `chara_model_table.tsv`, then ORs
`0xfe000000`. The runtime loads the column into `CharaInfo::appearance.mask`
and copies it to `RuntimeEntity::appearance.mask`.

## Colour presets

Named characters also carry a colour preset. The source table is
`g_chara_color_presets` at `0xCEAB20` (123 rows × `0x18`):

```c
uint8_t  appearance[8]; // +0x00  per-part appearance index (0..56)
uint32_t color0;        // +0x08  ARGB (alpha unused; the block supplies it)
uint32_t color1;        // +0x0c
uint32_t color2;        // +0x10
uint32_t color3;        // +0x14  not copied by chara_color_block_apply_preset
```

The preset index is `CharaSet + 0x10` (`w10` low 32 bits), unless the script's
`DrCreateChara(..., coaPreset)` argument is non-zero. `chara_color_get_preset`
(`0x11AE10`) bounds the index to 123 rows. `DrCreateChara`'s second argument is
a **CharaDef name**, resolved with `charaset_find_by_label` (`0x11A100`);
`chara_create_from_charaset` (`0x175BE0`) uses that CharaSet id.

### Where the defaults come from

The stage-data tables start empty: `FUN_002C7244` (stage-data ctor) calls
`FUN_002BF670`, which initialises 500 `0x1d0`-byte character defs with
`charaSet = -1`. Nothing is baked into the executable or the CPK. Entries are
created at runtime by `FUN_002C8828` (unit create):

```c
chara_apply_charaset(def, charaSet, level, 0);   // defaults
if (colourVariant == -1)                         // -1 = game default
    preset = CharaSet->w10_low32;
else                                             // generic-unit variety
    preset = {4, 1, 2, 3}[colourVariant - 1];    // table at 0xAEDB80
chara_color_block_apply_preset(block, preset);
```

`chara_apply_charaset` (`0x2C0BA4`) is the default filler: it resolves the
class/level and calls `chara_color_block_apply_preset` with
`*(uint*)(CharaSet + 0x10)`, then copies the 0x58-byte block into
`def + 0x160`. So the defaults are the 123-row preset table, selected by
`CharaSet + 0x10`; generic units rotate presets 4/1/2/3.

### The colour block

`chara_color_block_init` (`0x2C2BE4`) and `chara_color_block_apply_preset`
(`0x2C2E08`) define a 0x58-byte block:

```
+0x00  uint32 colour[3]     preset colour0..colour2
+0x0c  uint32               spare; encounter/colour parameter
+0x10  uint8  appearance[8] preset appearance bytes (asset indices)
+0x18  8 × 8 bytes          per-part data, zero-initialised
```

The part entries are plain values, not pointers: `chara_color_block_release`
(`0x2C2DAC`) calls a bare `ret` on each, and `chara_color_block_init` zeroes
each with `str xzr`. Their shape matches the shader's
`(g_GradientMapPartsIndex, g_GradientMapPartsValue)` pair, one pair per part.

`chara_apply_color_block` (`0xD1AB8`) copies the block to `chara+0x2018`.
The four logical colour channels are named by the script API
`DrSetCharaColor(objectName, common, hair, main, sub)`. The only script that
calls it (`Dr_OW_Z_Epilog_0_A.asb`) passes small values
(`DrSetCharaColor("レックス", 11, 0, 1, 0)`), consistent with indices into the
`UcCharaColorList` names rather than ARGB. The block itself only stores three
colours plus the spare word at `+0x0c` (init 1), so a fourth channel has to be
carried elsewhere.

### The gradient-map path

`GradientMap/GradientMap00.ftx` is a 512×2048 BC7 texture: 512 ramps of 512
horizontal samples, stacked as 512 bands of four identical rows. Bands 0–44 are
a warm ramp family, 45–225 are an unused grey ramp, and 226–511 are colour
ramps. The FTX V axis is bottom-up relative to the decoded image.

The engine's NVN shader text (see `docs/GRADIENT_MAP.md`; extractor
`tools/extract_shaders.py`) gives the exact formula:

```
base = texture * vs_COLOR0
U    = dot(base.rgb, (0.223499998, 0.631299973, 0.0588000007))
V    = g_GradientMapPartsValue          // CPU-supplied, normalised
ramp = texture(GradientMap00, vec2(U, V)).rgb
out  = base + g_GradientMapPartsIndex * (ramp - base)
```

`Index == 0` leaves the flat tint and `Index == 1` is a full ramp replace.

The default path leaves `Index == 0`. Neither
`chara_color_block_apply_preset` nor `chara_color_block_apply_encounter`
writes the eight part pairs, so the preset colours alone never trigger a ramp
recolour. The writer is the per-unit override path
(`SetCharaGradientMapCommon/Hair/Main/Sub`, keyed by CharaDef name), which we
have not located; the reference frame's blonde hair is consistent with a saved
override rather than a static default.

The appearance bytes are model/asset indices: `FUN_0052CFC4` loads eight asset
tables with them. They are not a ramp selector, and the earlier
`band = 226 + appearance` experiment has been removed.

The viewer keeps the gradient path wired (LUT upload, exact shader maths,
per-vertex V) but inactive, so uncustomised characters keep their flat preset
tints.

The viewer uses this observed layer-role mapping, validated with an A/B/C/D
slot-order sweep against the reference frame at `Dr_OW_C_0` frame 420:

| `color_id` | preset colour | role |
|---|---|---|
| 1 | `color1` | dress / skirt |
| 2 | `color3` | hair |
| 3 | `color0` | torso / sweater |

`color_id == 0` keeps the `section_0` colours; `color2` is retained for other
characters/roles but is not used by this mapping. Note that `color3` is not
copied by `chara_color_block_apply_preset`, so the hair slot must be supplied by
the saved/per-unit block in the engine; the preset alone only gives three
colours.

The viewer generates `chara_colors.tsv` with `color0..color3` and
`appearance0..appearance7`, reads `color_preset` from `charaset.tsv`, and copies
the appearance bytes into `CharaInfo::appearance.parts` and
`RuntimeEntity::appearance.parts`. Story/placed characters can override the
preset with a per-stage character-def colour block at `def + 0x160` (looked up
by `FUN_002BF900`); that path is not implemented in the viewer yet.

## Viewer behaviour

`SpriteInstance::attribute_mask` receives the entity mask. `sprite.cpp` counts
and draws a layer only when:

```c
!(layer.s4_flags & 0x02)
&& (layer.attributes & attribute_mask) == 0
&& (layer.attributes & ~layer_mask) == 0   // viewer-only debug filter
```

Entities without a CharaSet (backgrounds, NPC records without a model row) use
mask `0`, so every non-SKIP layer is drawn, matching the engine's zeroed
`+0x294`.

## `CharaOp` needs no special case

`asbrun scan --scene-ir` over all 680 `Script/*.asb` modules found 253 files
mentioning `CharaOp`, but every `DrUiCreateChara` with a `CharaOp/Op_*.mbs`
asset is in `Script/Dr_OW_0_Opening.asb` (48 entities). Those characters still
resolve a CharaSet (`class`, `gender`, `variant`) through the normal name path,
so the model mask applies independently of the asset. The earlier class-MBS
group heuristic and its 229 exception rows are gone.

## Replaced data

- `viewer/data/generated/variants.tsv` — removed.
- `viewer/data/variants_seed.tsv` — removed.
- `viewer/data/generated/variant_overrides.tsv` (and its reviewed source) —
  removed.
- Runtime `group(mbs) | (1 << variant)` computation — removed.

## Caveats

- `element[0xc4]` is not modelled as a second mask source; it is zero on the
  observed character paths.
- The character colour preset block (`chara+0x2018`) is separate from layer
  selection and is not applied by this rule.
