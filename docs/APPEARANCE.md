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

`viewer/data/generated/charaset.tsv` carries the final mask as its eighth
column:

```text
idx name class gender variant charaImg label appearance_mask
```

`tools/build_viewer_data.py` joins the raw `CharaSet` row with
`class_model_m/f_table.tsv` and `chara_model_table.tsv`, then ORs
`0xfe000000`. The runtime loads the column into `CharaInfo::appearance.mask`
and copies it to `RuntimeEntity::appearance.mask`.

## Colour presets

Named characters also carry a colour preset. The source table is
`g_chara_color_presets` at `0xCEAB20` (123 rows × `0x18`):

```c
uint8_t  appearance[8]; // +0x00
uint32_t color0;        // +0x08  RGB
uint32_t color1;        // +0x0c
uint32_t color2;        // +0x10
uint32_t color3;        // +0x14  not used by the clothing path
```

The preset index is `CharaSet + 0x10` (`w10` low 32 bits), unless the script's
`DrCreateChara(..., coaPreset)` argument is non-zero. `chara_color_get_preset`
(`0x11AE10`) bounds the index to 123 rows.

`chara_color_block_apply_preset` (`0x2C2E08`) copies the first three colours
(and the eight appearance bytes) into the character colour block, and
`chara_apply_color_block` (`0xD1AB8`) copies that block to `chara+0x2018`.
`color3` is retained as the preset's `w14` field. The viewer uses the observed
layer-role mapping:

| `color_id` | preset colour | role |
|---|---|---|
| 1 | `color1` | dress / skirt |
| 2 | `color3` | hair |
| 3 | `color0` | torso / sweater |

`color_id == 0` keeps the `section_0` colours. `color2` is retained for other
characters/roles but is not used by this mapping.

The viewer generates `chara_colors.tsv` from this table, reads `color_preset`
from `charaset.tsv`, and applies the three colours to layers with matching
`color_id`. `color_id == 0` keeps the `section_0` colours.

Story/placed characters can override the preset with a per-stage character-def
colour block at `def + 0x160` (looked up by `FUN_002BF900`); that path is not
implemented in the viewer yet.

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
