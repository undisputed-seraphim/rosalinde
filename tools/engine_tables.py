#!/usr/bin/env python3
"""Catalog and dump the engine's static data tables.

Known tables are described here with their row layout. The discovery script
finds additional candidates; this script only emits layouts that have been
reviewed or explicitly marked as candidates.
"""

from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path

from engine_data import EngineImage, load_enum, write_tsv

DEFAULT_ELF = Path.home() / "zwei/unicorn_overlord/main2.elf"
DEFAULT_ENUMS = Path("viewer/data/generated/uc_enums.tsv")


@dataclass(frozen=True)
class Field:
    name: str
    offset: int
    kind: str = "u32"


@dataclass(frozen=True)
class TableSpec:
    name: str
    base: int
    stride: int
    count: int
    fields: tuple[Field, ...]
    kind: str = "rows"
    enum: str | None = None
    file: str | None = None
    confidence: str = "confirmed"
    source: str = "docs/DATA_SOURCES.md"
    notes: str = ""


def raw_fields(start, count, prefix="w"):
    return tuple(Field(f"{prefix}{start + i * 4:02x}", start + i * 4, "i32") for i in range(count))


def raw_hex_fields(start, count, prefix="w"):
    return tuple(Field(f"{prefix}{start + i * 4:02x}", start + i * 4, "hex32") for i in range(count))


CLASS_PART_ARRAYS = (
    (0, 0x275A218, 72),
    (1, 0x275A458, 69),
    (2, 0x275A680, 74),
    (3, 0x275A8D0, 73),
    (4, 0x275AB18, 74),
    (5, 0x275AD68, 73),
    (6, 0x275AFB0, 74),
    (7, 0x275B200, 73),
    (8, 0x275B448, 74),
    (9, 0x275B698, 73),
)

CLASS_PART_DEFAULTS = ((10, 0xD00990), (11, 0xD01680))

COAT_OF_ARMS = (
    ("Compartment", 0x2895B48, 38),
    ("Escutcheon", 0x2896348, 33),
    ("EscutcheonBG", 0x2896B48, 33),
    ("Charge", 0x2897348, 46),
    ("Helmet", 0x2897B48, 46),
    ("Crown", 0x2898348, 44),
    ("Supporter", 0x2898B48, 57),
    ("Scroll", 0x2899348, 39),
)


TABLES = (
    TableSpec(
        "stage",
        0x28920D0,
        0x50,
        103,
        (Field("name", 0, "strptr"), Field("base_lv", 0xC, "i32"), Field("country", 0x10, "i32")),
        kind="stage",
        file="stages.tsv",
        source="tools/dump_stage_tables.py; DATA_SOURCES.md",
    ),
    TableSpec(
        "stage_cue",
        0x285D9F8,
        0x10,
        103,
        (Field("name", 0, "strptr"), Field("bgm_cue_id", 8, "i32")),
        kind="stage_cue",
        file="stage_cues.tsv",
        source="tools/dump_stage_tables.py; DATA_SOURCES.md",
    ),
    TableSpec(
        "facility",
        0x27B31B0,
        0x90,
        485,
        (Field("label", 8, "strptr"), Field("x", 0x1C, "f32"), Field("y", 0x20, "f32")),
        kind="facility",
        enum="ST_FACILITYID",
        file="facility_pos.tsv",
        source="tools/dump_stage_tables.py; DATA_SOURCES.md",
    ),
    TableSpec(
        "drop_table",
        0x26E5D30,
        0x18,
        733,
        (Field("start", 0x10, "i32"), Field("count", 0x14, "i32")),
        kind="drop",
        file="drop_tables.tsv",
        source="tools/dump_stage_tables.py; DATA_SOURCES.md",
    ),
    TableSpec(
        "drop_entry",
        0xAEE6D0,
        0x10,
        846,
        (
            Field("item_id", 0, "i32"),
            Field("weight", 4, "i32"),
            Field("count", 8, "i32"),
            Field("add_index", 0xC, "i32"),
        ),
        kind="drop_entry",
        file="drop_entries.tsv",
        source="tools/dump_stage_tables.py; DATA_SOURCES.md",
    ),
    TableSpec(
        "class",
        0x2738FF8,
        0x58,
        74,
        (
            Field("base_class_id", 0, "i32"),
            Field("w04", 4, "u32"),
            Field("mbs_m", 8, "strptr"),
            Field("ftx_m", 0x10, "strptr"),
            Field("mbs_f", 0x18, "strptr"),
            Field("ftx_f", 0x20, "strptr"),
            Field("icon_mbs_m", 0x28, "strptr"),
            Field("icon_ftx_m", 0x30, "strptr"),
            Field("icon_mbs_f", 0x38, "strptr"),
            Field("icon_ftx_f", 0x40, "strptr"),
            Field("w48", 0x48, "i32"),
            Field("w4c", 0x4C, "i32"),
            Field("w50", 0x50, "i32"),
            Field("w54", 0x54, "i32"),
        ),
        kind="class",
        enum="CLASSTYPE",
        file="classes.tsv",
        source="tools/dump_chara_table.py; class_table_relation_matches",
        notes="+0 is the base/promotion class; icon assets are at +0x28/+0x30 (M) and +0x38/+0x40 (F).",
    ),
    TableSpec(
        "npctype",
        0x273A968,
        0x50,
        320,
        (
            Field("label", 0, "strptr"),
            Field("mbs", 8, "strptr"),
            Field("ftx", 0x10, "strptr"),
            Field("icon_mbs", 0x18, "strptr"),
            Field("icon_ftx", 0x20, "strptr"),
            Field("w28", 0x28, "hex64"),
            Field("w30", 0x30, "hex64"),
            Field("w38", 0x38, "hex64"),
            Field("w40", 0x40, "hex64"),
            Field("w48", 0x48, "hex64"),
        ),
        kind="npctype",
        enum="NPCTYPE",
        file="charanpc_assets.tsv",
        source="tools/dump_charanpc_table.py; npctype_get_record",
        notes="+0x18/+0x20 are icon assets; +0x28..+0x48 retain opaque record data.",
    ),
    TableSpec(
        "charaset",
        0x2741BB8,
        0x48,
        1388,
        (
            Field("label", 0, "strptr"),
            Field("class_id", 8, "u8"),
            Field("gender_id", 9, "u8"),
            Field("variant", 10, "u8"),
            Field("pad", 11, "u8"),
            Field("w0c", 0x0C, "u32"),
            Field("w10", 0x10, "hex64"),
            Field("w18", 0x18, "hex64"),
            Field("w20", 0x20, "hex64"),
            Field("reserved28", 0x28, "hex64"),
            Field("chara_img", 0x30, "strptr"),
            Field("w38", 0x38, "hex64"),
            Field("w40", 0x40, "hex64"),
        ),
        kind="charaset",
        enum="CHARASET",
        file="charaset.tsv",
        source="docs/EXE.md section 8; viewer/data/charaset_table.tsv; charaset_get_label",
        notes="+0x10..+0x28 and +0x38/+0x40 retain opaque record data.",
    ),
    TableSpec(
        "battlebg",
        0x27384B8,
        0x18,
        80,
        (
            Field("id", 0, "u32"),
            Field("scale", 4, "f32"),
            Field("bbs", 8, "strptr"),
            Field("label", 0x10, "strptr"),
        ),
        kind="battlebg",
        file="battlebg_variant_table.tsv",
        confidence="confirmed",
        source="BattleBG variant rows at 0x27384b8, selected by DrCreateStage",
        notes="BattleBG variant rows referenced by the BBS catalog; 80 entries.",
    ),
    TableSpec(
        "chara_model",
        0x2740D68,
        0x10,
        229,
        (
            Field("label", 0, "strptr"),
            Field("mask", 8, "hex16"),
            Field("reserved0", 0xA, "hex16"),
            Field("reserved4", 0xC, "hex32"),
        ),
        kind="chara_model",
        file="chara_model_table.tsv",
        confidence="confirmed",
        source="FUN_0011ba28: u16 mask at +8; index bound 0xe4",
        notes="Model/appearance rows selected by class model records; mask ORed into object+0x294.",
    ),
    TableSpec(
        "class_model_m",
        0x275BB30,
        8,
        73,
        (),
        kind="class_model",
        file="class_model_m_table.tsv",
        confidence="confirmed",
        source="FUN_0011b998(param_1,1); 73 pointers",
        notes="Class -> 10 model ids + variant count. Records are 0x30 bytes.",
    ),
    TableSpec(
        "class_model_f",
        0x275B8E0,
        8,
        74,
        (),
        kind="class_model",
        file="class_model_f_table.tsv",
        confidence="confirmed",
        source="FUN_0011b998(param_1,2); 74 pointers",
        notes="Female class -> model ids; many entries are null.",
    ),
    TableSpec(
        "class_part",
        0x275A218,
        8,
        729,
        (),
        kind="class_part",
        file="class_part_table.tsv",
        confidence="confirmed",
        source="FUN_00119d0c/de4/e34/ae2/b3d; ten contiguous pointer arrays",
        notes="Class -> 17 part-table ids. Ten arrays (five slots x M/F), 729 pointers; the TSV also carries the two default rows.",
    ),
    TableSpec(
        "chara_part",
        0xCEB6A8,
        0x28,
        2168,
        tuple(
            Field(name, pair * 4 + member * 2, "i16")
            for pair in range(10)
            for member, name in ((0, f"id{pair}"), (1, f"scale{pair}"))
        ),
        kind="chara_part",
        file="chara_part_table.tsv",
        confidence="confirmed",
        source="FUN_001197bc: ten {i16 id, i16 scale} pairs; class part ids bound 0..2167",
        notes="Scale is stored as percent (id value * 0.01 in FUN_001197bc). -1 = absent.",
    ),
    TableSpec(
        "chara_color",
        0xCEAB20,
        0x18,
        123,
        tuple(Field(f"w{i:02x}", i, "u8") for i in range(8))
        + tuple(Field(f"color{i}", 8 + i * 4, "hex32") for i in range(4)),
        kind="chara_color",
        file="chara_color_table.tsv",
        confidence="confirmed",
        source="FUN_0011ae10 bounds the index to 0x7b; FUN_002c0ba4 passes CharaSet+0x10 to FUN_002c2e08",
        notes="Character colour preset: eight appearance bytes followed by four RGB colours (low 24 bits; alpha comes from DAT_028c3950). FUN_002c2e08 copies only the first three colours, so +0x14 is retained as w14.",
    ),
    TableSpec(
        "field_terrain",
        0x27CF660,
        0x50,
        269,
        (
            Field("key", 0, "strptr"),
            Field("label", 8, "strptr"),
            Field("terrain_type", 0x10, "hex32"),
            Field("tint", 0x14, "hex32"),
            Field("w18", 0x18, "hex32"),
            Field("terrain_class", 0x1C, "hex32"),
            Field("snow", 0x20, "u8"),
            Field("passable_mask", 0x21, "hex8"),
            Field("w22", 0x22, "hex8"),
            Field("w23", 0x23, "hex8"),
            Field("w24", 0x24, "hex32"),
            Field("w28", 0x28, "hex32"),
            Field("w2c", 0x2C, "hex32"),
            Field("w30", 0x30, "hex32"),
            Field("w34", 0x34, "hex32"),
            Field("w38", 0x38, "hex32"),
            Field("w3c", 0x3C, "hex32"),
            Field("w40_lo", 0x40, "hex16"),
            Field("attack_modifier_0", 0x42, "i8"),
            Field("move_modifier_0", 0x43, "i8"),
            Field("attack_modifier_1", 0x44, "i8"),
            Field("move_modifier_1", 0x45, "i8"),
            Field("attack_modifier_2", 0x46, "i8"),
            Field("move_modifier_2", 0x47, "i8"),
            Field("attack_modifier_3", 0x48, "i8"),
            Field("move_modifier_3", 0x49, "i8"),
            Field("attack_modifier_4", 0x4A, "i8"),
            Field("move_modifier_4", 0x4B, "i8"),
            Field("w4c", 0x4C, "hex32"),
        ),
        kind="field_terrain",
        file="field_terrain_table.tsv",
        confidence="confirmed",
        source="FUN_0021c780/c794/c7d8/c7f8/c860/c878/c8e0; 269 rows",
        notes="`snow` is 1 for all snow and Bastorias terrain rows. `passable_mask` at +0x21 is a bit per movement class. Five signed byte pairs at +0x42..+0x4b are per-movement-class modifiers: FUN_0021c7f8 returns (100-attack_modifier)/100, FUN_0021c878 returns (100-move_modifier)/100; pair 0 is the default, pair 1 is movement class 3, pair 2 is class 5. Pairs 3/4 are retained but not read by those two accessors.",
    ),
    TableSpec(
        "quest",
        0x27D4AA8,
        0x40,
        183,
        (
            Field("key", 0, "strptr"),
            Field("quest_type", 8, "u32"),
            Field("quest_id", 0xC, "u32"),
            Field("w10", 0x10, "hex32"),
            Field("w14", 0x14, "hex32"),
            Field("req0_a", 0x18, "hex32"),
            Field("req0_b", 0x1C, "hex32"),
            Field("req1_a", 0x20, "hex32"),
            Field("req1_b", 0x24, "hex32"),
            Field("req2_a", 0x28, "hex32"),
            Field("req2_b", 0x2C, "hex32"),
            Field("stage", 0x30, "strptr"),
            Field("flag", 0x38, "hex32"),
            Field("reserved", 0x3C, "hex32"),
        ),
        kind="quest",
        file="quest_table.tsv",
        confidence="confirmed",
        source="FUN_0021ca88/cab0/cb08/ce10/ce74/cf0c; 183 rows",
        notes="quest_type at +8, quest_id at +0xc; three paired requirement words (req0/1/2_a,b) at +0x18..+0x2c; stage id string at +0x30; flag byte at +0x38.",
    ),
    TableSpec(
        "stage_effect",
        0x285E640,
        0x20,
        185,
        (
            Field("id", 0, "u32"),
            Field("se_id_0", 4, "hex32"),
            Field("se_id_1", 8, "hex32"),
            Field("se_id_2", 0xC, "hex32"),
            Field("se_id_3", 0x10, "hex32"),
            Field("reserved", 0x14, "hex32"),
            Field("name", 0x18, "strptr"),
        ),
        kind="stage_effect",
        file="stage_effect_table.tsv",
        confidence="confirmed",
        source="FUN_00327e30 return +0x18 string; FUN_00327d10 id search; FUN_0032a468 playback; 185 rows",
        notes="dsp_out_* display-effect rows. `se_id_0..3` are Sound/se.acb cue ids; every nonzero value resolves in the SE cue table. se_id_0 is always played, se_id_1/se_id_2 are optional (-1 = absent), and se_id_3 is played by the conditional FUN_00327e1c path.",
    ),
    TableSpec(
        "unit_name",
        0x2860108,
        0x20,
        149,
        (
            Field("key", 0, "strptr"),
            Field("label", 8, "strptr"),
            Field("voice_group", 0x10, "u32"),
            Field("w18", 0x18, "hex32"),
        ),
        kind="unit_name",
        file="unit_name_table.tsv",
        confidence="confirmed",
        source="FUN_0032edd8 / FUN_0032f268 / FUN_0032e90c ...; rows 0..148",
        notes="Short unit key at +0, JP name at +8. `voice_group` at +0x10 is multiplied by 100 and added to 0x124f81 to form a voice cue id in the FUN_0032e90c/FUN_0032f268/FUN_0032e760 family (voice_group 0 is the no-voice sentinel). The last row's +0x18 overlaps name_pool and is blanked.",
    ),
    TableSpec(
        "name_pool",
        0x28613A0,
        8,
        104,
        (Field("label", 0, "strptr"),),
        kind="name_pool",
        file="name_pool_table.tsv",
        confidence="confirmed",
        source="string-pointer array immediately after unit_name; 104 pointers",
        notes="Character/class name pool. Starts at unit_name row 148 +0x18.",
    ),
    TableSpec(
        "camera_view",
        0x279F5C0,
        0x38,
        179,
        (
            Field("key", 0, "strptr"),
            Field("flags", 8, "hex32"),
            Field("fov", 0xC, "hex32"),
            Field("x", 0x10, "hex32"),
            Field("y", 0x14, "hex32"),
            Field("z", 0x18, "hex32"),
            Field("yaw", 0x1C, "hex32"),
            Field("asset", 0x20, "strptr"),
            Field("distance", 0x28, "hex32"),
            Field("pitch", 0x2C, "hex32"),
            Field("id", 0x30, "hex32"),
            Field("reserved", 0x34, "hex32"),
        ),
        kind="camera_view",
        file="camera_view_table.tsv",
        confidence="candidate",
        source="CV_* keys; float columns; no direct accessor recovered (FUN_00181070)",
        notes="Likely camera/cutscene presets; `fov`, `x/y/z`, `yaw`, `distance`, `pitch` are inferred from the CV_* keys and float values.",
    ),
    TableSpec(
        "class_stats",
        0x275BD78,
        0x130,
        471,
        (
            Field("id", 0, "u32"),
            Field("class_id", 4, "u32"),
            Field("flags", 8, "hex32"),
        )
        + raw_hex_fields(0xC, 45)
        + (Field("track_name", 0xC0, "strptr"), Field("behavior", 0xC8, "strptr"))
        + raw_hex_fields(0xD0, 24),
        kind="class_stats",
        file="class_stats_table.tsv",
        confidence="candidate",
        source="FUN_0011d148/c55c/d68c: base+id*0x130, valid ids 1..470",
        notes="Unit/class parameter rows. `class_id` is 0..73; `track_name` at +0xc0 matches the character MBS section-9 track name (often without the `S##_` prefix); `behavior` at +0xc8 is a separate ASCII behaviour key (COUNTER, COVER, ...).",
    ),
    TableSpec(
        "class_sub",
        0x277ECEC,
        0x100,
        172,
        raw_hex_fields(0, 0xE4 // 4)
        + (Field("label", 0xE4, "strptr"),)
        + raw_hex_fields(0xE8, (0x100 - 0xE8) // 4),
        kind="class_sub",
        file="class_sub_table.tsv",
        confidence="candidate",
        source="FUN_0011d900/d860/d894: base+id*0x100, valid ids 1..171",
        notes="Ability/order/effect table. `label` at +0xe4 is the Japanese ability/effect name; the table includes facilities, traps, orders and assist skills.",
    ),
    TableSpec(
        "class_relation",
        0xD0D15C,
        0x8C,
        74,
        (
            Field("class_id", 0, "u32"),
            Field("assist_id", 4, "u32"),
            Field("type", 8, "u32"),
            Field("order_id", 0xC, "u32"),
            Field("order_id_2", 0x10, "u32"),
            Field("order_id_3", 0x14, "u32"),
            Field("order_id_4", 0x18, "u32"),
            Field("class_stats_id", 0x1C, "u32"),
            Field("attack_list_kind", 0x20, "u32"),
            Field("attack_motion_id_0", 0x24, "u32"),
            Field("attack_type_0", 0x28, "hex32"),
            Field("attack_motion_id_1", 0x2C, "u32"),
            Field("attack_type_1", 0x30, "hex32"),
            Field("attack_motion_id_2", 0x34, "u32"),
            Field("attack_type_2", 0x38, "hex32"),
            Field("attack_motion_id_3", 0x3C, "u32"),
            Field("support_list_kind", 0x40, "u32"),
            Field("w44", 0x44, "hex32"),
            Field("w48", 0x48, "hex32"),
            Field("w4c", 0x4C, "hex32"),
            Field("w50", 0x50, "hex32"),
            Field("support_motion_id_0", 0x54, "u32"),
            Field("support_type_0", 0x58, "hex32"),
            Field("support_motion_id_1", 0x5C, "u32"),
            Field("support_type_1", 0x60, "hex32"),
            Field("support_motion_id_2", 0x64, "u32"),
            Field("support_type_2", 0x68, "hex32"),
            Field("support_motion_id_3", 0x6C, "u32"),
            Field("extra_list_kind", 0x70, "u32"),
            Field("w74", 0x74, "hex32"),
            Field("w78", 0x78, "hex32"),
            Field("w7c", 0x7C, "hex32"),
            Field("class_stats_id_2", 0x80, "u32"),
        )
        + raw_hex_fields(0x84, (0x8C - 0x84) // 4),
        kind="class_relation",
        file="class_relation_table.tsv",
        confidence="candidate",
        source="class_relation_find/get_stats_value/get_stats; FUN_0011c140/c260/c4c0/d820: 74 rows of 0x8c",
        notes="Per-class ability set. assist_id and order_id..order_id_4 index class_sub (g_ability_table); class_stats_id/class_stats_id_2 index class_stats. The two action blocks store class_stats motion ids: attack_motion_id_0..3 and support_motion_id_0..3 resolve to class_stats.track_name, which is the corresponding MBS section-9 track name. `attack_list_kind`/`support_list_kind`/`extra_list_kind` are always 1 in valid rows and are not entry counts. attack_type_*/support_type_* are small unresolved action-kind selectors.",
    ),
    TableSpec(
        "bbs",
        0x2737AE8,
        0x18,
        185,
        (Field("path", 0, "strptr"), Field("label", 8, "strptr"), Field("w10", 0x10, "u32"), Field("f14", 0x14, "f32")),
        kind="bbs",
        file="bbs.tsv",
        confidence="confirmed",
        source="relocation slots at 0x2737ae8; DrCreateStage",
        notes="Real {path,label,w10,f14} rows. The previous 0x8827c8 base was inside .rela.dyn; only path and label were valid there.",
    ),
    TableSpec(
        "field_cue",
        0x2400478,
        0x14,
        7,
        (
            Field("id", 0, "i32"),
            Field("day", 4, "u32"),
            Field("night", 8, "u32"),
            Field("time_a", 0xC, "u32"),
            Field("time_b", 0x10, "u32"),
        ),
        kind="field_cue",
        file="field_cues.tsv",
        source="docs/DATA_SOURCES.md; field_cue_get_day accessor",
    ),
    TableSpec(
        "item",
        0x26EA1E8,
        0xB8,
        983,
        (
            Field("name", 8, "strptr"),
            Field("category", 0x10, "i32"),
            Field("w0c", 0xC, "hex32"),
            Field("w14", 0x14, "hex32"),
            Field("stat_a", 0x18, "i16"),
            Field("stat_b", 0x1A, "i16"),
            Field("max_stack", 0x1C, "u16"),
            Field("w1e", 0x1E, "hex16"),
            Field("price", 0x20, "u32"),
            Field("weight", 0x24, "u32"),
            Field("unit_param_id", 0x28, "u16"),
            Field("w2a", 0x2A, "hex16"),
            Field("w2c", 0x2C, "hex32"),
            Field("w30", 0x30, "hex32"),
            Field("w34", 0x34, "hex32"),
            Field("effect0_id", 0x38, "hex32"),
            Field("effect0_value", 0x3C, "f32"),
            Field("w40", 0x40, "hex32"),
            Field("w44", 0x44, "hex32"),
            Field("w48", 0x48, "hex32"),
            Field("w4c", 0x4C, "hex32"),
            Field("w50", 0x50, "hex32"),
            Field("w54", 0x54, "hex32"),
            Field("effect1_id", 0x58, "hex32"),
            Field("effect1_value", 0x5C, "f32"),
            Field("w60", 0x60, "hex32"),
            Field("w64", 0x64, "hex32"),
            Field("w68", 0x68, "hex32"),
            Field("w6c", 0x6C, "hex32"),
            Field("w70", 0x70, "hex32"),
            Field("w74", 0x74, "hex32"),
            Field("effect2_id", 0x78, "hex32"),
            Field("effect2_value", 0x7C, "f32"),
        )
        + raw_hex_fields(0x80, (0xB8 - 0x80) // 4),
        enum="ITEMID",
        file="items.tsv",
        confidence="confirmed",
        source="item_get_record / item_get_price / item_get_weight / item_get_unit_param_id / item_get_effect0..2",
        notes="Rows start at 0x26ea1e8; idx == item id. +0x18/+0x1a are the two signed equipment stats used by item_get_sort_stats. `max_stack` is the u16 at +0x1c (0 = unstackable, 99/999/9999 common); `weight` is the capacity cost at +0x24 (default 100). `unit_param_id` at +0x28 is the signed short returned by item_get_unit_param_id and indexes the class_stats/unit-param table. Each 0x20-byte effect block at +0x2c/+0x4c/+0x6c stores its id at block+0xc and float value at block+0x10.",
    ),
    TableSpec(
        "story_name",
        0x27A7D70,
        0x20,
        436,
        (
            Field("w00", 0, "i32"),
            Field("w04", 4, "i32"),
            Field("w08", 8, "i32"),
            Field("facility_id", 0xC, "i32"),
            Field("map_id", 0x10, "i32"),
            Field("reserved", 0x14, "i32"),
            Field("name", 0x18, "strptr"),
        ),
        enum=None,
        file="story_names.tsv",
        confidence="candidate",
        source="story_name_table_find bound 0x1b4; string column at +0x18",
        notes="Labels run from the prologue through the final chapter. `facility_id` (-1 when absent) indexes facility_table; `map_id` is the field-map/location index (OW_C0=0, OW_C1=1, ...), with TK/special maps using higher values.",
    ),
    TableSpec(
        "friendship_talk",
        0x27CBBB0,
        0x28,
        376,
        (
            Field("path", 0, "strptr"),
            Field("talk_part", 8, "u32"),
            Field("facility_id", 0xC, "u32"),
            Field("x", 0x10, "f32"),
            Field("y", 0x14, "f32"),
            Field("w18", 0x18, "u32"),
            Field("w1c", 0x1C, "u32"),
            Field("w20", 0x20, "u32"),
            Field("reserved", 0x24, "u32"),
        ),
        file="friendship_talk.tsv",
        confidence="candidate",
        source="376 Script/FT_*.asb rows; matches FRIENDSHIP_TALK enum family",
        notes="`talk_part` is the 1/2/3 conversation part within an FT script; `facility_id` indexes facility_table; `x`/`y` are the conversation trigger position near that facility. `w18` is the global talk order; w1c/w20 remain unresolved.",
    ),
    TableSpec(
        "encounter",
        0x27E3100,
        0x88,
        0xB2B,
        (Field("id", 0, "u32"), Field("w04", 4, "u32"), Field("name", 8, "strptr"))
        + tuple(Field(f"w{i:02x}", i, "u32") for i in range(0x10, 0x30, 4))
        + (Field("label", 0x30, "strptr"),)
        + tuple(Field(f"w{i:02x}", i, "u32") for i in range(0x38, 0x88, 4)),
        kind="rows",
        file="encounter_table.tsv",
        confidence="confirmed",
        source="FUN_002a7358: bound 0xb2b, base + idx * 0x88",
        notes="Encounter/unit records start with SAMPLE_E_* and a Japanese label at +0x30; the remaining words retain numeric data.",
    ),
    TableSpec(
        "deliver_list",
        0x27C4460,
        0x28,
        20,
        (Field("key", 0, "strptr"), Field("label", 8, "strptr"))
        + tuple(Field(f"w{i:02x}", i, "u32") for i in range(0x10, 0x28, 4)),
        kind="rows",
        file="deliver_list_table.tsv",
        confidence="confirmed",
        source="FUN_001a91c8: base + id * 0x28; row 0 is the UNKNOWN sentinel",
        notes="Shop/deliver entries. Keys are WEAPON_START/C0_WEAPON_0/...; w10..w24 retain the numeric columns.",
    ),
    TableSpec(
        "shop_asset",
        0x27C4280,
        0x18,
        20,
        (Field("mbs", 0, "strptr"), Field("ftx", 8, "strptr"), Field("dsp", 0x10, "strptr")),
        kind="rows",
        file="shop_asset_table.tsv",
        confidence="confirmed",
        source="FUN_001a91b8 region; 20 rows at 0x27c4280",
        notes="Shop asset pairs {mbs, ftx, dsp}. Row 0 is an empty sentinel.",
    ),
    TableSpec(
        "chara_level_curve",
        0xCE972C,
        0xC,
        51,
        (Field("f0", 0, "f32"), Field("f1", 4, "f32"), Field("f2", 8, "f32")),
        kind="rows",
        file="chara_level_curve_table.tsv",
        confidence="confirmed",
        source="FUN_0011a058: base + level * 0xc, levels 1..50",
        notes="Three floats per level; row 0 is the level-0 row.",
    ),
    TableSpec(
        "chara_stat_curve_a",
        0xCE9990,
        0x2C,
        10,
        tuple(Field(f"f{i:02d}", i * 4, "f32") for i in range(11)),
        kind="rows",
        file="chara_stat_curve_a_table.tsv",
        confidence="confirmed",
        source="FUN_0011ab38: averages two 0x2c rows",
        notes="Eleven floats per row; used by the stat/pose midpoint helper.",
    ),
    TableSpec(
        "chara_stat_curve_b",
        0xCE9B48,
        0x2C,
        10,
        tuple(Field(f"f{i:02d}", i * 4, "f32") for i in range(11)),
        kind="rows",
        file="chara_stat_curve_b_table.tsv",
        confidence="confirmed",
        source="FUN_0011ac24: averages two 0x2c rows",
        notes="Second eleven-float curve table, same shape as chara_stat_curve_a.",
    ),
    TableSpec(
        "chara_stat_group",
        0xCE9D00,
        0x30,
        28,
        (),
        kind="chara_stat_group",
        file="chara_stat_group_table.tsv",
        confidence="confirmed",
        source="FUN_0011ad10: group * 0xc0 + slot * 0x30, 7 groups x 4 slots",
        notes="Twelve floats per slot.",
    ),
    TableSpec(
        "chara_class_values",
        0xCEA240,
        0xC,
        56,
        tuple(Field(f"v{i}", i * 2, "u16") for i in range(6)),
        kind="rows",
        file="chara_class_values_table.tsv",
        confidence="confirmed",
        source="FUN_0011ad84: base + row * 0xc + column * 2",
        notes="Six u16 values per row.",
    ),
    TableSpec(
        "chara_level_stats",
        0xCEA4E0,
        0x14,
        50,
        tuple(Field(f"v{i}", i * 4, "u32") for i in range(5)),
        kind="rows",
        file="chara_level_stats_table.tsv",
        confidence="confirmed",
        source="FUN_0011adb0: base + (level - 1) * 0x14 + stat * 4; used by damage paths",
        notes="Five u32 values per level; damage code scales them by 0.2 and 0.8.",
    ),
    TableSpec(
        "chara_level_growth",
        0xCEA8C8,
        0xC,
        50,
        tuple(Field(f"v{i}", i * 2, "u16") for i in range(6)),
        kind="rows",
        file="chara_level_growth_table.tsv",
        confidence="confirmed",
        source="FUN_0011ade0: base + (level - 1) * 0xc + column * 2; used by damage paths",
        notes="Six u16 values per level; same callers as chara_level_stats.",
    ),
    TableSpec(
        "coat_of_arms",
        0x2895B48,
        0x800,
        8,
        (),
        kind="coat_of_arms",
        file="coat_of_arms_table.tsv",
        confidence="confirmed",
        source="eight string-pointer arrays at 0x2895b48..0x2899348; parts are 0x800 apart",
        notes="Compartment, Escutcheon, EscutcheonBG, Charge, Helmet, Crown, Supporter and Scroll asset-name arrays.",
    ),
    TableSpec(
        "asset_manifest",
        0x26E5470,
        8,
        104,
        (Field("path", 0, "strptr"),),
        kind="rows",
        file="asset_manifest_table.tsv",
        confidence="confirmed",
        source="104 consecutive string pointers at 0x26e5470",
        notes="Global asset list: fonts, effects, GUI, message sheets, OTM and sound files.",
    ),

)

TABLE_BY_NAME = {table.name: table for table in TABLES}

RAW_FILES = {
    "stage": "stage_table.tsv",
    "stage_cue": "stage_cue_table.tsv",
    "facility": "facility_table.tsv",
    "drop_table": "drop_table.tsv",
    "drop_entry": "drop_entry_table.tsv",
    "class": "class_table.tsv",
    "npctype": "npctype_table.tsv",
    "charaset": "charaset_table.tsv",
    "battlebg": "battlebg_variant_table.tsv",
    "chara_model": "chara_model_table.tsv",
    "class_model_m": "class_model_m_table.tsv",
    "class_model_f": "class_model_f_table.tsv",
    "class_part": "class_part_table.tsv",
    "chara_part": "chara_part_table.tsv",
    "chara_color": "chara_color_table.tsv",
    "field_terrain": "field_terrain_table.tsv",
    "quest": "quest_table.tsv",
    "stage_effect": "stage_effect_table.tsv",
    "unit_name": "unit_name_table.tsv",
    "name_pool": "name_pool_table.tsv",
    "camera_view": "camera_view_table.tsv",
    "class_stats": "class_stats_table.tsv",
    "class_sub": "class_sub_table.tsv",
    "class_relation": "class_relation_table.tsv",
    "bbs": "bbs_table.tsv",
    "field_cue": "field_cue_table.tsv",
    "item": "item_table.tsv",
    "story_name": "story_name_table.tsv",
    "friendship_talk": "friendship_talk_table.tsv",
    "encounter": "encounter_table.tsv",
    "deliver_list": "deliver_list_table.tsv",
    "shop_asset": "shop_asset_table.tsv",
    "chara_level_curve": "chara_level_curve_table.tsv",
    "chara_stat_curve_a": "chara_stat_curve_a_table.tsv",
    "chara_stat_curve_b": "chara_stat_curve_b_table.tsv",
    "chara_stat_group": "chara_stat_group_table.tsv",
    "chara_class_values": "chara_class_values_table.tsv",
    "chara_level_stats": "chara_level_stats_table.tsv",
    "chara_level_growth": "chara_level_growth_table.tsv",
    "coat_of_arms": "coat_of_arms_table.tsv",
    "asset_manifest": "asset_manifest_table.tsv",
}


def enum_map(path):
    groups = {}
    for kind in {
        "CHARASET",
        "CLASSTYPE",
        "DROPTABLE",
        "ITEMID",
        "NPCTYPE",
        "ST_FACILITYID",
    }:
        groups[kind] = load_enum(path, kind)
    return groups


def cell(value):
    return str(value).replace("\t", " ").replace("\n", " ").replace("\r", " ")


def read_field(image, address, field):
    if field.kind == "strptr":
        return image.read_cstr(image.read_ptr(address + field.offset))
    if field.kind == "ptr":
        return f"0x{image.read_ptr(address + field.offset):x}"
    if field.kind == "u8":
        return image.read_u8(address + field.offset)
    if field.kind == "i8":
        return struct.unpack("<b", image.read(address + field.offset, 1))[0]
    if field.kind == "u16":
        return image.read_u16(address + field.offset)
    if field.kind == "i16":
        return struct.unpack("<h", image.read(address + field.offset, 2))[0]
    if field.kind == "u32":
        return image.read_u32(address + field.offset)
    if field.kind == "i32":
        return image.read_i32(address + field.offset)
    if field.kind == "u64":
        return image.read_u64(address + field.offset)
    if field.kind == "hex8":
        return f"0x{image.read_u8(address + field.offset):x}"
    if field.kind == "hex16":
        return f"0x{image.read_u16(address + field.offset):x}"
    if field.kind == "hex32":
        return f"0x{image.read_u32(address + field.offset):x}"
    if field.kind == "hex64":
        return f"0x{image.read_u64(address + field.offset):x}"
    if field.kind == "f32":
        return format(image.read_f32(address + field.offset), ".6g")
    raise ValueError(f"unknown field kind: {field.kind}")


def enum_columns(index, table, enums):
    entries = enums.get(table.enum or "", [])
    if 0 <= index < len(entries):
        return entries[index]
    return "", ""


def asset_name(value):
    return value[6:] if value.startswith("Chara/") else value


def dump_stage(image, spec, enums):
    cues = {}
    cue_spec = TABLE_BY_NAME["stage_cue"]
    for i in range(cue_spec.count):
        record = cue_spec.base + i * cue_spec.stride
        name = image.read_cstr(image.read_ptr(record))
        cues[name] = image.read_i32(record + 8)
    rows = []
    for i in range(spec.count):
        record = spec.base + i * spec.stride
        name = image.read_cstr(image.read_ptr(record))
        rows.append((name, image.read_i32(record + 0xC), cues.get(name, -1), image.read_i32(record + 0x10)))
    return "# stage\tbase_lv\tbgm_cue_id\tcountry", rows


def dump_class(image, spec, enums):
    classes = enums.get("CLASSTYPE", [])
    rows = []
    for i in range(spec.count):
        record = spec.base + i * spec.stride
        if not image.read_ptr(record + 8):
            continue
        key = classes[i][0] if i < len(classes) else f"CLASS_{i}"
        label = classes[i][1] if i < len(classes) else ""
        for suffix, mbs_field, ftx_field in (("M", 8, 0x10), ("F", 0x18, 0x20)):
            mbs = asset_name(image.read_cstr(image.read_ptr(record + mbs_field)))
            if not mbs:
                continue
            ftx = asset_name(image.read_cstr(image.read_ptr(record + ftx_field)))
            rows.append((f"{key}_{suffix}", label, mbs, ftx))
    return "# key\tjp_name\tmbs\tftx", rows


def dump_charaset(image, spec, enums):
    names = enums.get("CHARASET", [])
    classes = enums.get("CLASSTYPE", [])
    rows = []
    for i in range(1, spec.count):
        record = spec.base + i * spec.stride
        name = names[i][0] if i < len(names) else f"CHARASET_{i}"
        class_id = image.read_u8(record + 8)
        class_name = classes[class_id][0] if class_id < len(classes) else f"CLASS_{class_id}"
        gender_id = image.read_u8(record + 9)
        gender = "M" if gender_id == 1 else "F" if gender_id == 2 else "?"
        variant = image.read_u8(record + 10)
        chara_img = image.read_cstr(image.read_ptr(record + 0x30))
        label = image.read_cstr(image.read_ptr(record))
        rows.append((i, name, class_name, gender, variant, chara_img, label))
    return "# idx\tname\tclass\tgender\tvariant\tcharaImg\tlabel", rows


def dump_npctype(image, spec, enums):
    names = enums.get("NPCTYPE", [])
    rows = []
    for i in range(spec.count):
        record = spec.base + i * spec.stride
        mbs = image.read_cstr(image.read_ptr(record + 8))
        if not mbs:
            continue
        name = names[i][0] if i < len(names) else f"NPCTYPE_{i}"
        label = names[i][1] if i < len(names) else image.read_cstr(image.read_ptr(record))
        ftx = image.read_cstr(image.read_ptr(record + 0x10))
        rows.append((i, name, label, mbs, ftx))
    return "# idx\tname\tjp\tmbs\tftx", rows


def dump_facility(image, spec, enums):
    facilities = enums.get("ST_FACILITYID", [])
    rows = []
    for i in range(spec.count):
        record = spec.base + i * spec.stride
        key = facilities[i][0] if i < len(facilities) else f"FAC_{i}"
        label = image.read_cstr(image.read_ptr(record + 8))
        x, y = image.read_f32x2(record + 0x1C)
        rows.append((key, label, x, y))
    return "# key\tlabel\tx\ty", rows


def dump_drop(image, spec, enums):
    drops = enums.get("DROPTABLE", [])
    items = enums.get("ITEMID", [])
    rows = []
    entry_base = 0xAEE6D0
    entry_stride = 0x10
    for i in range(spec.count):
        record = spec.base + i * spec.stride
        start = image.read_i32(record + 0x10)
        count = image.read_i32(record + 0x14)
        drop_name = drops[i][0] if i < len(drops) else f"DROP_{i}"
        for j in range(max(0, count)):
            entry = entry_base + (start + j) * entry_stride
            item_id = image.read_i32(entry)
            weight = image.read_i32(entry + 4)
            amount = image.read_i32(entry + 8)
            add_index = image.read_i32(entry + 0xC)
            item = items[item_id][0] if 0 <= item_id < len(items) else f"ITEM_{item_id}"
            rows.append((drop_name, item, weight, amount, add_index))
    return "# drop_table\titem\tweight\tcount\tadd_index", rows


def dump_bbs(image, spec, _enums):
    rows = []
    for i in range(spec.count):
        record = spec.base + i * spec.stride
        path = image.read_cstr(image.read_ptr(record))
        if not path:
            continue
        label = image.read_cstr(image.read_ptr(record + 8))
        rows.append((i, path, label))
    return "# stage_type\tpath\tlabel", rows


def dump_field_cue(image, spec, _enums):
    rows = []
    for i in range(spec.count):
        record = spec.base + i * spec.stride
        rows.append(
            (
                image.read_i32(record),
                image.read_u32(record + 4),
                image.read_u32(record + 8),
                image.read_u32(record + 0xC),
                image.read_u32(record + 0x10),
            )
        )
    return "# id\tday\tnight\ttime_a\ttime_b", rows


def dump_rows(image, spec, enums):
    header = ["idx"] + [field.name for field in spec.fields]
    if spec.enum:
        header += [f"{spec.enum.lower()}", "enum_label"]
    rows = []
    for i in range(spec.count):
        record = spec.base + i * spec.stride
        row = [i]
        row.extend(read_field(image, record, field) for field in spec.fields)
        if spec.enum:
            row.extend(enum_columns(i, spec, enums))
        rows.append(row)
    return "\t".join(header), rows


def render_table(image, spec, enums):
    if spec.kind == "stage":
        return dump_stage(image, spec, enums)
    if spec.kind == "class":
        return dump_class(image, spec, enums)
    if spec.kind == "charaset":
        return dump_charaset(image, spec, enums)
    if spec.kind == "npctype":
        return dump_npctype(image, spec, enums)
    if spec.kind == "facility":
        return dump_facility(image, spec, enums)
    if spec.kind == "drop":
        return dump_drop(image, spec, enums)
    if spec.kind == "bbs":
        return dump_bbs(image, spec, enums)
    if spec.kind == "field_cue":
        return dump_field_cue(image, spec, enums)
    if spec.kind in ("chara_stat_group", "coat_of_arms"):
        return dump_raw(image, spec)
    return dump_rows(image, spec, enums)


def dump_raw(image, spec):
    if spec.kind == "stage":
        header = "idx\tname\tbase_lv\tcountry"
        rows = [
            (i, image.read_cstr(image.read_ptr(spec.base + i * spec.stride)), image.read_i32(spec.base + i * spec.stride + 0xC), image.read_i32(spec.base + i * spec.stride + 0x10))
            for i in range(spec.count)
        ]
        return header, rows
    if spec.kind == "stage_cue":
        header = "idx\tname\tbgm_cue_id"
        rows = [
            (i, image.read_cstr(image.read_ptr(spec.base + i * spec.stride)), image.read_i32(spec.base + i * spec.stride + 8))
            for i in range(spec.count)
        ]
        return header, rows
    if spec.kind == "facility":
        header = "idx\tlabel\ttown\tx\ty"
        rows = []
        for i in range(spec.count):
            record = spec.base + i * spec.stride
            x, y = image.read_f32x2(record + 0x1C)
            rows.append(
                (
                    i,
                    image.read_cstr(image.read_ptr(record + 8)),
                    image.read_cstr(image.read_ptr(record + 0x80)),
                    x,
                    y,
                )
            )
        return header, rows
    if spec.kind == "class":
        header = (
            "idx\tbase_class_id\tw04\tmbs_m\tftx_m\tmbs_f\tftx_f\t"
            "icon_mbs_m\ticon_ftx_m\ticon_mbs_f\ticon_ftx_f\tw48\tw4c\tw50\tw54"
        )
        rows = []
        for i in range(spec.count):
            record = spec.base + i * spec.stride
            rows.append(
                (
                    i,
                    image.read_i32(record),
                    image.read_u32(record + 4),
                    image.read_cstr(image.read_ptr(record + 8)),
                    image.read_cstr(image.read_ptr(record + 0x10)),
                    image.read_cstr(image.read_ptr(record + 0x18)),
                    image.read_cstr(image.read_ptr(record + 0x20)),
                    image.read_cstr(image.read_ptr(record + 0x28)),
                    image.read_cstr(image.read_ptr(record + 0x30)),
                    image.read_cstr(image.read_ptr(record + 0x38)),
                    image.read_cstr(image.read_ptr(record + 0x40)),
                    image.read_i32(record + 0x48),
                    image.read_i32(record + 0x4C),
                    image.read_i32(record + 0x50),
                    image.read_i32(record + 0x54),
                )
            )
        return header, rows
    if spec.kind == "npctype":
        header = "idx\tlabel\tmbs\tftx\ticon_mbs\ticon_ftx\tw28\tw30\tw38\tw40\tw48"
        rows = []
        for i in range(spec.count):
            record = spec.base + i * spec.stride
            rows.append(
                (
                    i,
                    image.read_cstr(image.read_ptr(record)),
                    image.read_cstr(image.read_ptr(record + 8)),
                    image.read_cstr(image.read_ptr(record + 0x10)),
                    image.read_cstr(image.read_ptr(record + 0x18)),
                    image.read_cstr(image.read_ptr(record + 0x20)),
                    f"0x{image.read_u64(record + 0x28):x}",
                    f"0x{image.read_u64(record + 0x30):x}",
                    f"0x{image.read_u64(record + 0x38):x}",
                    f"0x{image.read_u64(record + 0x40):x}",
                    f"0x{image.read_u64(record + 0x48):x}",
                )
            )
        return header, rows
    if spec.kind == "charaset":
        header = (
            "idx\tlabel\tclass_id\tgender_id\tvariant\tpad\tw0c\t"
            "w10\tw18\tw20\treserved28\tchara_img\tw38\tw40"
        )
        rows = []
        for i in range(spec.count):
            record = spec.base + i * spec.stride
            rows.append(
                (
                    i,
                    image.read_cstr(image.read_ptr(record)),
                    image.read_u8(record + 8),
                    image.read_u8(record + 9),
                    image.read_u8(record + 10),
                    image.read_u8(record + 11),
                    image.read_u32(record + 0x0C),
                    f"0x{image.read_u64(record + 0x10):x}",
                    f"0x{image.read_u64(record + 0x18):x}",
                    f"0x{image.read_u64(record + 0x20):x}",
                    f"0x{image.read_u64(record + 0x28):x}",
                    image.read_cstr(image.read_ptr(record + 0x30)),
                    f"0x{image.read_u64(record + 0x38):x}",
                    f"0x{image.read_u64(record + 0x40):x}",
                )
            )
        return header, rows
    if spec.kind == "bbs":
        header = "idx\tpath\tlabel\tw10\tf14"
        rows = []
        for i in range(spec.count):
            record = spec.base + i * spec.stride
            rows.append(
                (
                    i,
                    image.read_cstr(image.read_ptr(record)),
                    image.read_cstr(image.read_ptr(record + 8)),
                    image.read_u32(record + 0x10),
                    image.read_f32(record + 0x14),
                )
            )
        return header, rows
    if spec.kind == "field_cue":
        header = "idx\tid\tday\tnight\ttime_a\ttime_b"
        rows = []
        for i in range(spec.count):
            record = spec.base + i * spec.stride
            rows.append((i, image.read_i32(record), image.read_u32(record + 4), image.read_u32(record + 8), image.read_u32(record + 0xC), image.read_u32(record + 0x10)))
        return header, rows
    if spec.kind == "drop":
        header = "idx\tkey\tlabel\tstart\tcount"
        rows = []
        for i in range(spec.count):
            record = spec.base + i * spec.stride
            rows.append(
                (
                    i,
                    image.read_cstr(image.read_ptr(record)),
                    image.read_cstr(image.read_ptr(record + 8)),
                    image.read_i32(record + 0x10),
                    image.read_i32(record + 0x14),
                )
            )
        return header, rows
    if spec.kind == "drop_entry":
        header = "idx\titem_id\tweight\tcount\tadd_index"
        rows = []
        for i in range(spec.count):
            record = spec.base + i * spec.stride
            rows.append((i, image.read_i32(record), image.read_i32(record + 4), image.read_i32(record + 8), image.read_i32(record + 0xC)))
        return header, rows
    if spec.kind == "class_model":
        header = "\t".join(
            ["idx", "record", "class_id"]
            + [f"model{i}" for i in range(10)]
            + ["variant_count"]
        )
        rows = []
        for i in range(spec.count):
            record = image.read_ptr(spec.base + i * spec.stride)
            if not record:
                rows.append((i, "0x0") + ("",) * 12)
                continue
            values = [image.read_u32(record + j * 4) for j in range(12)]
            rows.append((i, f"0x{record:x}", *values))
        return header, rows
    if spec.kind == "class_part":
        header = "\t".join(
            ["array", "idx", "record", "class_id"]
            + [f"part{i:02d}" for i in range(17)]
        )
        rows = []
        for array, base, count in CLASS_PART_ARRAYS:
            for i in range(count):
                record = image.read_ptr(base + i * 8)
                if not record:
                    rows.append((array, i, "0x0") + ("",) * 18)
                    continue
                values = [image.read_u32(record + j * 4) for j in range(18)]
                rows.append((array, i, f"0x{record:x}", *values))
        for array, record in CLASS_PART_DEFAULTS:
            values = [image.read_u32(record + j * 4) for j in range(18)]
            rows.append((array, -1, f"0x{record:x}", *values))
        return header, rows
    if spec.kind == "unit_name":
        header = "idx\tkey\tlabel\tvoice_group\tw18"
        rows = []
        for i in range(spec.count):
            record = spec.base + i * spec.stride
            w18 = "" if i == spec.count - 1 else f"0x{image.read_u32(record + 0x18):x}"
            rows.append(
                (
                    i,
                    image.read_cstr(image.read_ptr(record)),
                    image.read_cstr(image.read_ptr(record + 8)),
                    image.read_u32(record + 0x10),
                    w18,
                )
            )
        return header, rows
    if spec.kind == "chara_stat_group":
        header = "idx\tgroup\tslot\t" + "\t".join(f"f{i:02d}" for i in range(12))
        rows = []
        index = 0
        for group in range(7):
            for slot in range(4):
                record = spec.base + group * 0xC0 + slot * 0x30
                rows.append([index, group, slot] + [image.read_f32(record + j * 4) for j in range(12)])
                index += 1
        return header, rows
    if spec.kind == "coat_of_arms":
        header = "part\tidx\tpath"
        rows = []
        for part, base, count in COAT_OF_ARMS:
            for i in range(count):
                rows.append((part, i, image.read_cstr(image.read_ptr(base + i * 8))))
        return header, rows
    header = "\t".join(["idx"] + [field.name for field in spec.fields])
    rows = []
    for i in range(spec.count):
        record = spec.base + i * spec.stride
        rows.append([i] + [read_field(image, record, field) for field in spec.fields])
    return header, rows


def catalog_rows():
    for table in TABLES:
        yield (
            table.name,
            f"0x{table.base:x}",
            f"0x{table.stride:x}",
            table.count,
            table.kind,
            table.confidence,
            table.file or f"{table.name}.tsv",
            table.source,
            table.notes,
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", nargs="?", default=str(DEFAULT_ELF), help="loadable ELF")
    parser.add_argument("--enums", default=str(DEFAULT_ENUMS), help="enum TSV")
    parser.add_argument("--out-dir", default=None)
    parser.add_argument("--list", action="store_true", help="list the catalog")
    parser.add_argument("--raw", action="store_true", help="write raw tables instead of viewer projections")
    parser.add_argument("--dump", action="append", choices=["all", *TABLE_BY_NAME], help="table to dump (repeatable)")
    args = parser.parse_args()

    if args.list or not args.dump:
        for row in catalog_rows():
            print("\t".join(cell(value) for value in row))
        return

    image = EngineImage(args.elf)
    enums = {} if args.raw else enum_map(args.enums)
    out_dir = Path(args.out_dir or ("viewer/data" if args.raw else "viewer/data/generated"))
    out_dir.mkdir(parents=True, exist_ok=True)

    selected = TABLES if "all" in args.dump else [TABLE_BY_NAME[name] for name in args.dump]
    if args.raw:
        for table in selected:
            header, rows = dump_raw(image, table)
            filename = RAW_FILES.get(table.name, f"{table.name}.tsv")
            path = out_dir / filename
            write_tsv(path, header, ([cell(value) for value in row] for row in rows))
            print(f"wrote {path} ({len(rows)} rows)", flush=True)
        return

    write_tsv(
        out_dir / "index.tsv",
        "# table\tbase\tstride\tcount\tkind\tconfidence\tfile\tsource\tnotes",
        catalog_rows(),
    )

    for table in selected:
        header, rows = render_table(image, table, enums)
        filename = table.file or f"{table.name}.tsv"
        path = out_dir / filename
        write_tsv(path, header, ([cell(value) for value in row] for row in rows))
        print(f"wrote {path} ({len(rows)} rows)", flush=True)


if __name__ == "__main__":
    main()
