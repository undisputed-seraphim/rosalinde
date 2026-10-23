# Viewer data

The TSVs directly in this directory are raw extracted source tables. They are
the ground truth for the joins below and are not rewritten by the join step.
`viewer/data/generated/` contains the checked-in results of those joins.

## Raw inputs

- `stage_table.tsv` — stage name, base level, country
- `stage_cue_table.tsv` — stage name and cue id
- `facility_table.tsv` — `idx label town x y`; `town` is the string at `+0x80`
- `class_table.tsv` — `idx base_class_id w04 mbs_m ftx_m mbs_f ftx_f
  icon_mbs_m icon_ftx_m icon_mbs_f icon_ftx_f w48 w4c w50 w54`.
  `base_class_id` is the promotion/base class (0 = none); M/F model and icon
  assets are stored as recovered, and `w04`/`w48..w54` retain opaque fields.
- `npctype_table.tsv` — `idx label mbs ftx icon_mbs icon_ftx w28 w30 w38
  w40 w48`. The `w*` columns retain opaque record data as recovered.
- `charaset_table.tsv` — `idx label class_id gender_id variant pad w0c w10
  w18 w20 reserved28 chara_img w38 w40`. `class_id` is a `CLASSTYPE` index;
  `reserved28` is zero in all rows; the `w*` columns retain opaque record data.
- `battlebg_variant_table.tsv` — 80 `BattleBG` variant rows:
  `idx id scale bbs label`. The BBS table points into this table at `+0x08`
  and `+0x10`.
- `chara_model_table.tsv` — 229 appearance/model rows:
  `idx label mask reserved0 reserved4`. `mask` is the u16 at `+0x08` ORed
  into the character object at `+0x294`; the two reserved fields are zero.
- `class_model_m_table.tsv` / `class_model_f_table.tsv` — 73/74 class
  pointer arrays: `idx record class_id model0..model9 variant_count`.
  `record` is the pointed-to `0x30`-byte class model record; null entries are
  retained as empty cells.
- `class_part_table.tsv` — the ten contiguous class part pointer arrays plus
  the two defaults: `array idx record class_id part00..part16`. Each record is
  18 u32; `part*` values index `chara_part_table.tsv`.
- `chara_part_table.tsv` — 2168 part rows: `idx id0 scale0 ... id9 scale9`.
  Each pair is `{i16 part_id, i16 scale_percent}`; `-1` means absent.
- `field_terrain_table.tsv` — 269 terrain rows: `idx key label
  terrain_type tint w18 terrain_class w20 passable_mask modifier_a modifier_b
  w24 w28 w2c..w4c`. `passable_mask` is the bitfield at `+0x21`; the two
  modifiers are the signed bytes at `+0x22`/`+0x23` used by
  `FUN_0021c7f8`/`0x0021c878`.
- `quest_table.tsv` — 183 quest rows:
  `idx key quest_type quest_id w10 w14 req0_a req0_b req1_a req1_b req2_a
  req2_b stage flag reserved`. `quest_type` is the switch value 0..4 used by the
  quest setup code; the three requirement pairs are the `FUN_0021ce74` /
  `FUN_0021ce10` getter outputs.
- `stage_effect_table.tsv` — 185 display-effect rows:
  `idx id w04 w08 w0c w10 reserved name`; `reserved` is zero in all rows.
- `unit_name_table.tsv` — 149 unit/character rows:
  `idx key label w10 w18`; the last row's `w18` is blank because the following
  `name_pool_table.tsv` starts at `unit_name` row 148 `+0x18`.
- `name_pool_table.tsv` — 104 character/class name pointers:
  `idx label`.
- `camera_view_table.tsv` — 179 candidate camera-view rows:
  `idx key flags fov x y z yaw asset distance pitch id reserved`. The
  `CV_*` keys and float columns are consistent with camera presets, but there
  is no direct accessor yet.
- `class_stats_table.tsv` — 471 unit/class parameter rows. `id` at `+0`,
  `class_id` at `+4`, `flags` at `+8`, `motion` at `+0xc0`, `behavior` at
  `+0xc8`; remaining words retained as `w*`. `motion` values are action names
  such as `S01_ATTACK_A1`; `behavior` values are `COUNTER`, `COVER`, `CHASE`,
  `ACCUMULATE`, `PROVOCATION`, …
- `class_sub_table.tsv` — 172 ability/order/effect rows. `label` at `+0xe4`
  is the Japanese name (`バリスタ`, `ブレイブオーダー`, `アシスト射撃`, …).
  The table mixes facilities, traps, orders and assist skills.
- `class_relation_table.tsv` — 74 per-class ability sets:
  `idx class_id assist_id type order_id order_id_2 w14 w18 class_stats_id
  w20..w7c class_stats_id_2 w84 w88`. `assist_id`, `order_id` and
  `order_id_2` index `class_sub_table.tsv`; `class_stats_id` and
  `class_stats_id_2` index `class_stats_table.tsv`; `type` is 0/23/24/25 and
  is checked by `FUN_0011d710`/`FUN_0011d820`.
- `bbs_table.tsv` — `idx path ptr_a flags_a label ptr_b flags_b`
- `drop_table.tsv` — `idx key label start count`; `drop_entry_table.tsv` —
  drop entries
- `country_regions.tsv` — country-region quads extracted from the executable
- `variants_seed.tsv` — reviewed per-character appearance masks
- `variants_overrides.tsv` — multi-group appearance masks that cannot be
  expressed by the single-group fallback

Every raw table row carries the full header column count. Empty trailing
fields are written as empty cells, not omitted.

### Pointer columns in raw tables

These are real fields, not joins:

- `bbs.ptr_a` points at `battlebg_variant.label` (`battlebg +0x10`);
  `bbs.ptr_b` points at `battlebg_variant.bbs` (`battlebg +0x08`).
- `npc_appearance.ptr_a`/`ptr_b` point into `npctype` or `chara_model`
  records.
- `stage_map.ptr08` points into an `encounter` row at `+0x10`/`+0x28`/
  `+0x40`; `stage_map.ptr20` points into an encounter row at `+0x0`/
  `+0x18`/`+0x30`.
- `enum_name_table.ptr08` points into a `drop_table` row.

These references are useful for resolving the remaining `w*` columns without
treating the pointer values themselves as new data.

## Generated outputs

- `stages.tsv` — `stage_table` joined with `stage_cue_table` by stage name.
- `facility_pos.tsv` — `facility_table` with `ST_FACILITYID` keys by row index.
- `drop_tables.tsv` — `drop_table` ranges expanded through `drop_entry_table`,
  then joined with `DROPTABLE` and `ITEMID`.
- `classes.tsv` — `class_table` joined with `CLASSTYPE`; emits separate M/F
  rows and drops the `Chara/` prefix expected by the runtime loader.
- `charanpc_assets.tsv` — `npctype_table` joined with `NPCTYPE`; skips rows
  without an MBS.
- `charaset.tsv` — `charaset_table` joined with `CHARASET` and `CLASSTYPE`;
  maps class/gender ids to names and skips index 0.
- `bbs.tsv` — `bbs_table` with empty paths omitted.
- `variants.tsv` — `key character flags label`, produced by joining
  `charaset_table` with `class_table` and each class MBS `s4.attributes` group.
  The fallback mask is `group(class) | (1 << variant)`; `variants_seed.tsv` and
  `variants_overrides.tsv` take precedence. The full table is retained; rows
  with the same class key are intentional because they identify different
  characters sharing the same mask. `label` is the CharaSet Japanese label.
- `chara_table.tsv` — `character display_name mbs ftx`, generated by scanning
  CPK `Script/*.asb` for `DrUiCreateChara`.
- `battlebgs.tsv` — generated by scanning the CPK `BattleBG/*` catalog.
- `uc_enums.tsv` — generated from CPK `Debug/_UcEnum_*.inc`; used for the
  enum joins above.
- `country_regions.tsv` — a relative symlink to the raw executable-derived table; this is a passthrough, not a generated join.

## Regenerating

Run the join step with an explicit CPK path:

```text
python3 tools/build_viewer_data.py --cpk /path/to/Unicorn.CPK
```

The script writes to `viewer/data/generated/` by default. It does not rewrite
raw inputs, does not assume a CPK location, and fails if the expected row
counts or source tables are missing.
