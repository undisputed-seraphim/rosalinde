#!/usr/bin/env python3
"""Catalog and dump the engine's static data tables.

Known tables are described here with their row layout. The discovery script
finds additional candidates; this script only emits layouts that have been
reviewed or explicitly marked as candidates.
"""

from __future__ import annotations

import argparse
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
        (Field("name", 0, "strptr"), Field("cue", 8, "i32")),
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
            Field("class_id", 0, "i32"),
            Field("mbs_m", 8, "strptr"),
            Field("ftx_m", 0x10, "strptr"),
            Field("mbs_f", 0x18, "strptr"),
            Field("ftx_f", 0x20, "strptr"),
        ),
        kind="class",
        enum="CLASSTYPE",
        file="classes.tsv",
        source="tools/dump_chara_table.py; class_table_relation_matches",
    ),
    TableSpec(
        "npctype",
        0x273A968,
        0x50,
        320,
        (Field("label", 0, "strptr"), Field("mbs", 8, "strptr"), Field("ftx", 0x10, "strptr")),
        kind="npctype",
        enum="NPCTYPE",
        file="charanpc_assets.tsv",
        source="tools/dump_charanpc_table.py; npctype_get_record",
    ),
    TableSpec(
        "charaset",
        0x2741BB8,
        0x48,
        1388,
        (
            Field("label", 0, "strptr"),
            Field("class_id", 8, "u8"),
            Field("gender", 9, "u8"),
            Field("variant", 10, "u8"),
            Field("_pad", 11, "u8"),
            Field("chara_img", 0x30, "strptr"),
        ),
        kind="charaset",
        enum="CHARASET",
        file="charaset.tsv",
        source="docs/EXE.md section 8; viewer/data/charaset_table.tsv; charaset_get_label",
    ),
    TableSpec(
        "bbs",
        0x8827C8,
        0x30,
        185,
        (Field("path", 0, "strptr"), Field("label", 0x18, "strptr")),
        kind="bbs",
        file="bbs.tsv",
        source="tools/dump_bbs_table.py; DrCreateStage",
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
        0x26EA1F0,
        0xB8,
        983,
        (Field("name", 0, "strptr"), Field("category", 8, "i32"))
        + raw_fields(0xC, (0xB8 - 0xC) // 4),
        enum="ITEMID",
        file="items.tsv",
        confidence="confirmed",
        source="item_is_weapon / _UcEnum_Item.inc",
        notes="Row fields after category are not yet named.",
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
            Field("w0c", 0xC, "i32"),
            Field("w10", 0x10, "i32"),
            Field("w14", 0x14, "i32"),
            Field("name", 0x18, "strptr"),
        ),
        enum=None,
        file="story_names.tsv",
        confidence="candidate",
        source="story_name_table_find bound 0x1b4; string column at +0x18",
        notes="Labels run from the prologue through the final chapter.",
    ),
    TableSpec(
        "friendship_talk",
        0x27CBBB0,
        0x28,
        376,
        (
            Field("path", 0, "strptr"),
            Field("w08", 8, "u32"),
            Field("w0c", 0xC, "u32"),
            Field("w10", 0x10, "u32"),
            Field("w14", 0x14, "u32"),
            Field("w18", 0x18, "u32"),
            Field("w1c", 0x1C, "u32"),
            Field("w20", 0x20, "u32"),
            Field("w24", 0x24, "u32"),
        ),
        file="friendship_talk.tsv",
        confidence="candidate",
        source="376 Script/FT_*.asb rows; matches FRIENDSHIP_TALK enum family",
    ),
    TableSpec(
        "npc_appearance",
        0x892068,
        0x30,
        1505,
        (
            Field("label_a", 0, "strptr"),
            Field("ptr_a", 8, "ptr"),
            Field("flags_a", 0x10, "u32"),
            Field("label_b", 0x18, "strptr"),
            Field("ptr_b", 0x20, "ptr"),
            Field("flags_b", 0x28, "u32"),
        ),
        file="npc_appearance.tsv",
        confidence="candidate",
        source="CharaNpc and GUI/IconNpc labels; no accessor recovered yet",
        notes="Rows hold two 0x18 label/pointer/flags entries.",
    ),
    TableSpec(
        "encounter",
        0x27E2FF8,
        0x88,
        2861,
        (Field("name", 0, "strptr"),)
        + raw_fields(8, (0x88 - 8) // 4),
        file="encounter.tsv",
        confidence="candidate",
        source="stage and SAMPLE_E_* / OW_*_EN_* labels",
        notes="Likely enemy or encounter data; row semantics are OPEN.",
    ),
    TableSpec(
        "stage_map",
        0x93D4D0,
        0x30,
        2874,
        (
            Field("name", 0, "strptr"),
            Field("ptr08", 8, "ptr"),
            Field("w10", 0x10, "u32"),
            Field("ptr18", 0x18, "ptr"),
            Field("ptr20", 0x20, "ptr"),
            Field("w28", 0x28, "u32"),
        ),
        file="stage_map.tsv",
        confidence="candidate",
        source="rodata stage labels plus pointers into the encounter region",
    ),
    TableSpec(
        "deliver_list",
        0x90D878,
        0x30,
        344,
        (
            Field("label_a", 0, "strptr"),
            Field("ptr_a", 8, "ptr"),
            Field("flags_a", 0x10, "u32"),
            Field("label_b", 0x18, "strptr"),
            Field("ptr_b", 0x20, "ptr"),
            Field("flags_b", 0x28, "u32"),
        ),
        file="deliver_list.tsv",
        confidence="candidate",
        source="344 GUI/shop asset rows; matches _UcEnum_DeliverList size",
        notes="Rows hold two 0x18 label/pointer/flags entries.",
    ),
    TableSpec(
        "enum_names",
        0x82FA40,
        0x18,
        2449,
        (Field("label", 0, "strptr"), Field("ptr08", 8, "ptr"), Field("w10", 0x10, "u32")),
        file="enum_names.tsv",
        confidence="candidate",
        source="combined enum-name/string table; not a game data table",
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
    "bbs": "bbs_table.tsv",
    "field_cue": "field_cue_table.tsv",
    "item": "item_table.tsv",
    "story_name": "story_name_table.tsv",
    "friendship_talk": "friendship_talk_table.tsv",
    "npc_appearance": "npc_appearance_table.tsv",
    "encounter": "encounter_table.tsv",
    "stage_map": "stage_map_table.tsv",
    "deliver_list": "deliver_list_table.tsv",
    "enum_names": "enum_name_table.tsv",
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
    if field.kind == "u16":
        return image.read_u16(address + field.offset)
    if field.kind == "u32":
        return image.read_u32(address + field.offset)
    if field.kind == "i32":
        return image.read_i32(address + field.offset)
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
    return "# stage\tbase_lv\tcue\tcountry", rows


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
        label = image.read_cstr(image.read_ptr(record + 0x18))
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
        header = "idx\tname\tcue"
        rows = [
            (i, image.read_cstr(image.read_ptr(spec.base + i * spec.stride)), image.read_i32(spec.base + i * spec.stride + 8))
            for i in range(spec.count)
        ]
        return header, rows
    if spec.kind == "facility":
        header = "idx\tlabel\tx\ty"
        rows = []
        for i in range(spec.count):
            record = spec.base + i * spec.stride
            x, y = image.read_f32x2(record + 0x1C)
            rows.append((i, image.read_cstr(image.read_ptr(record + 8)), x, y))
        return header, rows
    if spec.kind == "class":
        header = "idx\tclass_id\tmbs_m\tftx_m\tmbs_f\tftx_f"
        rows = []
        for i in range(spec.count):
            record = spec.base + i * spec.stride
            rows.append(
                (
                    i,
                    image.read_i32(record),
                    image.read_cstr(image.read_ptr(record + 8)),
                    image.read_cstr(image.read_ptr(record + 0x10)),
                    image.read_cstr(image.read_ptr(record + 0x18)),
                    image.read_cstr(image.read_ptr(record + 0x20)),
                )
            )
        return header, rows
    if spec.kind == "npctype":
        header = "idx\tlabel\tmbs\tftx"
        rows = [
            (
                i,
                image.read_cstr(image.read_ptr(spec.base + i * spec.stride)),
                image.read_cstr(image.read_ptr(spec.base + i * spec.stride + 8)),
                image.read_cstr(image.read_ptr(spec.base + i * spec.stride + 0x10)),
            )
            for i in range(spec.count)
        ]
        return header, rows
    if spec.kind == "charaset":
        header = "idx\tlabel\tclass_id\tgender_id\tvariant\tchara_img"
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
                    image.read_cstr(image.read_ptr(record + 0x30)),
                )
            )
        return header, rows
    if spec.kind == "bbs":
        header = "idx\tpath\tlabel"
        rows = [
            (i, image.read_cstr(image.read_ptr(spec.base + i * spec.stride)), image.read_cstr(image.read_ptr(spec.base + i * spec.stride + 0x18)))
            for i in range(spec.count)
        ]
        return header, rows
    if spec.kind == "field_cue":
        header = "idx\tid\tday\tnight\ttime_a\ttime_b"
        rows = []
        for i in range(spec.count):
            record = spec.base + i * spec.stride
            rows.append((i, image.read_i32(record), image.read_u32(record + 4), image.read_u32(record + 8), image.read_u32(record + 0xC), image.read_u32(record + 0x10)))
        return header, rows
    if spec.kind == "drop":
        header = "idx\tstart\tcount"
        rows = [(i, image.read_i32(spec.base + i * spec.stride + 0x10), image.read_i32(spec.base + i * spec.stride + 0x14)) for i in range(spec.count)]
        return header, rows
    if spec.kind == "drop_entry":
        header = "idx\titem_id\tweight\tcount\tadd_index"
        rows = []
        for i in range(spec.count):
            record = spec.base + i * spec.stride
            rows.append((i, image.read_i32(record), image.read_i32(record + 4), image.read_i32(record + 8), image.read_i32(record + 0xC)))
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
            write_tsv(path, header, ([cell(value) for value in row] for row in rows), strip_trailing=True)
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
