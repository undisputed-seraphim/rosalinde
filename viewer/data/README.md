# Viewer data

The TSVs directly in this directory are raw extracted source tables. They are
the ground truth for the joins below and are not rewritten by the join step.
`viewer/data/generated/` contains the checked-in results of those joins.

## Raw inputs

- `stage_table.tsv` — stage name, base level, country
- `stage_cue_table.tsv` — stage name and `bgm_cue_id`; all 103 ids resolve in `Sound/bgm.acb`
- `facility_table.tsv` — `idx label town x y`; `town` is the string at `+0x80`
  and is a UFB object prefix (`Town_Cxx`), matching `Town_Cxx_1/_2` object names
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
  terrain_type tint w18 terrain_class snow passable_mask w22 w23 w24 w28
  w2c..w3c w40_lo attack_modifier_0 move_modifier_0 .. attack_modifier_4
  move_modifier_4 w4c`. `passable_mask` is the bitfield at `+0x21`; `snow` is
  the byte at `+0x20` and is 1 for every snow and Bastorias terrain row. The
  five signed modifier pairs at `+0x42..+0x4b` are the fields actually read by
  `FUN_0021c7f8` (attack multiplier) and `FUN_0021c878` (movement multiplier);
  each returns `(100 - byte) / 100`. Pair 0 is the default movement class,
  pair 1 is class 3, and pair 2 is class 5 in those accessors. The old
  `modifier_a`/`modifier_b` labels at `+0x22`/`+0x23` were incorrect; those
  bytes are now retained as `w22`/`w23` (zero in all rows).
- `quest_table.tsv` — 183 quest rows:
  `idx key quest_type quest_id reward0_id reward0_count reward1_id
  reward1_count reward2_id reward2_count w28 w2c stage flag reserved`.
  `quest_type` is the switch value 0..4 used by the quest setup code. The three
  reward slots are `(item id, count)`; item id 2 is money, and the quest UI
  renders each slot with an item icon and count. `w28`/`w2c` look like rank and
  recommended level but are not read by the native accessors.
- `stage_effect_table.tsv` — 185 display-effect rows:
  `idx id se_id_0 se_id_1 se_id_2 se_id_3 reserved name`. The four `se_id_*`
  fields are `Sound/se.acb` cue ids; every nonzero value resolves in the SE cue
  table, and `0xffffffff` means absent. `reserved` is zero in all rows.
- `unit_name_table.tsv` — 149 unit/character rows:
  `idx key label voice_group w18`; `voice_group` at `+0x10` is multiplied by
  100 and added to `0x124f81` to form a voice cue id (`0` is the no-voice
  sentinel). The last row's `w18` is blank because the following
  `name_pool_table.tsv` starts at `unit_name` row 148 `+0x18`.
- `name_pool_table.tsv` — 104 character/class name pointers:
  `idx label`.
- `camera_view_table.tsv` — 179 candidate camera-view rows:
  `idx key flags fov x y z yaw asset distance pitch id reserved`. The
  `CV_*` keys and float columns are consistent with camera presets, but there
  is no direct accessor yet.
- `class_stats_table.tsv` — 471 unit/class parameter rows. `id` at `+0`,
  `class_id` at `+4`, `flags` at `+8`, `track_name` at `+0xc0`, `behavior` at
  `+0xc8`; remaining words retained as `w*`. `track_name` values match the
  corresponding character MBS section-9 track names, often with the `S##_`
  prefix omitted (`S01_ATTACK_A1`, `COVER`, `COUNTER`, `FIRST_AID_1`, …).
  `behavior` values are separate AI/action keys (`COUNTER`, `COVER`, `CHASE`,
  `ACCUMULATE`, `PROVOCATION`, …).
- `class_sub_table.tsv` — 172 ability/order/effect rows. `label` at `+0xe4`
  is the Japanese name (`バリスタ`, `ブレイブオーダー`, `アシスト射撃`, …).
  The table mixes facilities, traps, orders and assist skills.
- `class_relation_table.tsv` — 74 per-class ability sets:
  `idx class_id assist_id type order_id order_id_2 order_id_3 order_id_4
  class_stats_id attack_list_kind attack_motion_id_0 attack_type_0 ...
  attack_motion_id_3 support_list_kind w44 w48 w4c w50 support_motion_id_0
  support_type_0 ... support_motion_id_3 extra_list_kind w74 w78 w7c
  class_stats_id_2 w84 w88`. `assist_id` and the four `order_id*` fields index
  `class_sub_table.tsv`; `class_stats_id`/`class_stats_id_2` and the two
  action blocks index `class_stats_table.tsv`; `type` is 0/23/24/25 and is
  checked by `FUN_0011d710`/`FUN_0011d820`. `attack_motion_id_0..3` and
  `support_motion_id_0..3` resolve through `class_stats.track_name` to the
  character MBS section-9 track name (212 exact matches plus 44 `S##_`-prefix
  matches across the 378 action slots). `attack_list_kind`/`support_list_kind`/
  `extra_list_kind` are 1 in valid rows and are not entry counts. The
  `attack_type_*`/`support_type_*` words are small unresolved action-kind
  selectors.
- `story_name_table.tsv` — 436 story/character/location labels. `facility_id`
  at `+0xc` is `-1` or a `facility_table` index; `map_id` at `+0x10` is the
  field-map index (`OW_C0=0`, `OW_C1=1`, …; TK/special maps use higher ids).
- `friendship_talk_table.tsv` — 376 `Script/FT_*.asb` conversation rows.
  `talk_part` is 1/2/3 within an FT script, `facility_id` indexes
  `facility_table`, and `x`/`y` are the trigger position near that facility.
- `item_table.tsv` — 983 item rows, row base `0x26ea1e8`:
  `idx name category w0c w14 stat_a stat_b max_stack w1e price weight
  unit_param_id w2a w2c w30 w34 effect0_id effect0_value w40 w44 w48 w4c
  w50 w54 effect1_id effect1_value w60 w64 w68 w6c w70 w74 effect2_id
  effect2_value w80..wb4`.
  `idx` is the item id. `stat_a`/`stat_b` are the two signed equipment stats
  at `+0x18`/`+0x1a`; `max_stack` is the u16 at `+0x1c` (0 = unstackable);
  `price` is the u32 at `+0x20`, and `weight` is the capacity cost at `+0x24`
  (default 100 when zero). The three 0x20-byte effect blocks start at `+0x2c`,
  `+0x4c`, and `+0x6c`; each stores its effect id at block `+0xc` and its
  float value at block `+0x10`.
- `bbs_table.tsv` — 185 BattleBG rows: `idx path label w10 f14`. The table
  lives at `0x2737ae8` with stride `0x18`; the old `0x8827c8` base was inside
  `.rela.dyn` and misread relocation triples.
- `encounter_table.tsv` — 2859 encounter/unit rows (`0x27e3100`, stride
  `0x88`): `idx id w04 name w10..w2c label w38..w84`. `name` is the
  `SAMPLE_E_*`/`OW_*` key and `label` is the Japanese unit name.
- `encounter_slot_table.tsv` — 17,154 rows (2859 encounters × 6 slots):
  `encounter slot charaset_id w4 w8 enabled`. Each slot is a CharaSet
  reference; byte `+9` bit 0 is the enable flag, and normally exactly one slot
  is enabled per encounter.
- `map_node_table.tsv` — 30 world-map quest markers (`0x27e2660`, stride
  `0x20`): `idx x0 y0 quest_id icon marker_id x1 y1 scale`. `x0/y0` is the
  map position, `x1/y1` the label position, and `quest_id` is the quest row id.
- `country_path_table.tsv` — 175 points across the five engine point lists
  (`0x27e2a20`; labels end in `採掘場`): `country idx x y`. Each row of the
  five-row table at `0x27e2fa8` points at the first point of its list.
- `deliver_list_table.tsv` — 20 shop/deliver rows (`0x27c4460`, stride
  `0x28`): `idx key label w10..w24`. Row 0 is the `UNKNOWN` sentinel; the
  remaining keys are `WEAPON_START`, `C0_WEAPON_0`, ...
- `shop_asset_table.tsv` — 20 shop asset rows (`0x27c4280`, stride `0x18`):
  `idx mbs ftx dsp`. Row 0 is an empty sentinel.
- `asset_manifest_table.tsv` — 104 global asset paths (`0x26e5470`): fonts,
  effects, GUI, message sheets, OTM, and sound files.
- `coat_of_arms_table.tsv` — 336 coat-of-arms asset names:
  `part idx path`. The eight arrays are Compartment (38), Escutcheon (33),
  EscutcheonBG (33), Charge (46), Helmet (46), Crown (44), Supporter (57),
  and Scroll (39).
- `chara_level_curve_table.tsv` — 51 rows × 3 floats (`0xce972c`,
  stride `0xc`); accessor `FUN_0011a058`.
- `chara_stat_curve_a_table.tsv` / `chara_stat_curve_b_table.tsv` — 10 rows
  × 11 floats (`0xce9990`/`0xce9b48`, stride `0x2c`); the accessors average
  two rows (`FUN_0011ab38`/`FUN_0011ac24`).
- `chara_stat_group_table.tsv` — 7 groups × 4 slots × 12 floats
  (`0xce9d00`, accessor `FUN_0011ad10`).
- `chara_class_values_table.tsv` — 56 rows × 6 u16 (`0xcea240`, stride
  `0xc`); accessor `FUN_0011ad84`.
- `chara_level_stats_table.tsv` — 50 rows × 5 u32 (`0xcea4e0`, stride
  `0x14`); accessor `FUN_0011adb0`, used by the damage paths.
- `chara_level_growth_table.tsv` — 50 rows × 6 u16 (`0xcea8c8`, stride
  `0xc`); accessor `FUN_0011ade0`, same callers as `chara_level_stats`.
- `drop_table.tsv` — `idx key label start count`; `drop_entry_table.tsv` —
  drop entries
- `country_regions.tsv` — country-region quads extracted from the executable
- `variants_seed.tsv` — reviewed per-character appearance masks
- `variants_overrides.tsv` — multi-group appearance masks that cannot be
  expressed by the single-group fallback

Every raw table row carries the full header column count. Empty trailing
fields are written as empty cells, not omitted.

### Relocation views

The former `npc_appearance_table.tsv`, `stage_map_table.tsv`, and
`enum_name_table.tsv` were removed: their bases pointed inside `.rela.dyn`,
so every `flags`/`w*` value was the relocation type `0x403` and the row
grouping was an artefact of the relocation triples. NPC appearance is already
covered by `npctype_table` + `chara_model_table`; the stage-map graph remains
open. Raw tables no longer retain `r_offset` or `0x403` columns.

## Generated outputs

- `stages.tsv` — `stage_table` joined with `stage_cue_table` by stage name;
  its third column is the verified `bgm_cue_id`.
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
- `field_assets.tsv` — 39 `FieldBG/Field.ufb` asset pairs with normalized
  main/texture paths and parsed `ACxx_yy[P|F]` area/variant/layer fields.
  `area_label` follows `FieldCatalog::area_for_entry`/`area_for_country`:
  `00=C`, `01=A`, `02=B`, `03=D`, `04=E`, `10=Z` (`AC10` is the final-stage area).
- `field_names.tsv` — 337 UFB object names, 179 tile names, and 4 region names.
- `field_objects.tsv` — UCB object census joined to UFB object names:
  cell count and grid bbox per used `object_index`. `object_index` is the
  direct entry into the UFB object-name list; UCB index 0 is the empty sentinel.
- `field_entries.tsv` — UCB `entry_index` census with cell counts and bboxes.
  The used range is `0..1646`, matching UFB `count_54 = 1647`.
- `field_ucb.tsv` — one-row UCB grid summary.
- `otm_files.tsv` — per-OTM counts, cue-id range, frame/value maxima, and
  track occupancy.
- `otm_enums.tsv` — corpus-wide histograms for OTM event kind/flags and cue
  type/flags/params, with the number of contributing files.
- `uc_enums.tsv` — generated from CPK `Debug/_UcEnum_*.inc`; used for the
  enum joins above.
- `country_regions.tsv` — a relative symlink to the raw executable-derived table; this is a passthrough, not a generated join.

## Regenerating

Run the join step with an explicit CPK path:

```text
python3 tools/build_viewer_data.py --cpk /path/to/Unicorn.CPK
```

The script writes to `viewer/data/generated/` by default. It delegates the
field/audio census to `tools/cpk_census.py`, which uses rosalinde `d --stats`.
It does not rewrite raw inputs, does not assume a CPK location, and fails if
the expected row counts or source tables are missing.
