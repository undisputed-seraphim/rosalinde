#!/usr/bin/env python3
"""Dump the engine's stage/facility/drop tables from a loadable ELF.

The tables live in the engine's `.data` (not the CPK) and are indexed by the
names/ids the scripts pass:

- stage table    0x28920d0, stride 0x50, 103 records: name (+0), base level (+0xc),
  country (+0x10)
- stage cue table 0x285d9f8, stride 0x10, 103 records: name (+0), BGM cue id (+8)
- facility table 0x27b31b0, stride 0x90, 485 records: label (+8), pos (+0x1c/+0x20)
- drop table     0x26e5d30, stride 0x18, 733 records: start (+0x10), count (+0x14);
  entries at 0xaee6d0, stride 0x10: item id (+0), weight (+4), count (+8)

Writes:

    viewer/data/generated/stages.tsv        stage  base_lv  bgm_cue_id  country
    viewer/data/generated/facility_pos.tsv  key    label    x  y
    viewer/data/generated/drop_tables.tsv   drop_table  item  weight  count  add_index

The stage table names are `ST_OW_*` and match the 103 `St_OW_*.asb` scripts;
facility records are index-aligned with `ST_FACILITYID` (485 entries).

    python3 tools/dump_stage_tables.py ~/zwei/unicorn_overlord/main2.elf
"""

import argparse
import os
import sys

from engine_data import EngineImage, load_enum, write_tsv

STAGE_BASE, STAGE_STRIDE, STAGE_COUNT = 0x28920D0, 0x50, 103
CUE_BASE, CUE_STRIDE, CUE_COUNT = 0x285D9F8, 0x10, 103
FAC_BASE, FAC_STRIDE, FAC_COUNT = 0x27B31B0, 0x90, 485
DROP_BASE, DROP_STRIDE, DROP_COUNT = 0x26E5D30, 0x18, 733
DROP_ENTRY_BASE, DROP_ENTRY_STRIDE = 0xAEE6D0, 0x10


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("elf", help="loadable ELF (from nso2elf.py)")
    ap.add_argument("--enums", default="viewer/data/generated/uc_enums.tsv")
    ap.add_argument("--out-dir", default="viewer/data/generated")
    args = ap.parse_args()

    image = EngineImage(args.elf)

    facilities = load_enum(args.enums, "ST_FACILITYID")
    drops = load_enum(args.enums, "DROPTABLE")
    items = load_enum(args.enums, "ITEMID")

    cues = {}
    for i in range(CUE_COUNT):
        rec = CUE_BASE + i * CUE_STRIDE
        name = image.read_cstr(image.read_ptr(rec))
        cues[name] = image.read_i32(rec + 8)

    stage_rows = []
    for i in range(STAGE_COUNT):
        rec = STAGE_BASE + i * STAGE_STRIDE
        name = image.read_cstr(image.read_ptr(rec))
        country = image.read_i32(rec + 0x10)
        stage_rows.append((name, image.read_i32(rec + 0xC), cues.get(name, -1), country))

    fac_rows = []
    for i in range(FAC_COUNT):
        rec = FAC_BASE + i * FAC_STRIDE
        label = image.read_cstr(image.read_ptr(rec + 8))
        key = facilities[i][0] if i < len(facilities) else f"FAC_{i}"
        x, y = image.read_f32x2(rec + 0x1C)
        fac_rows.append((key, label, x, y))

    drop_rows = []
    for i in range(DROP_COUNT):
        rec = DROP_BASE + i * DROP_STRIDE
        start = image.read_i32(rec + 0x10)
        count = image.read_i32(rec + 0x14)
        drop_name = drops[i][0] if i < len(drops) else f"DROP_{i}"
        for j in range(max(0, count)):
            ent = DROP_ENTRY_BASE + (start + j) * DROP_ENTRY_STRIDE
            item_id = image.read_i32(ent)
            weight = image.read_i32(ent + 4)
            n = image.read_i32(ent + 8)
            add_index = image.read_i32(ent + 12)
            item = items[item_id][0] if 0 <= item_id < len(items) else f"ITEM_{item_id}"
            drop_rows.append((drop_name, item, weight, n, add_index))

    os.makedirs(args.out_dir, exist_ok=True)
    write_tsv(
        os.path.join(args.out_dir, "stages.tsv"),
        "# stage\tbase_lv\tbgm_cue_id\tcountry (generated from the engine stage/cue tables by tools/dump_stage_tables.py)",
        stage_rows,
    )
    write_tsv(
        os.path.join(args.out_dir, "facility_pos.tsv"),
        "# key\tlabel\tx\ty (generated from the engine facility table by tools/dump_stage_tables.py)",
        fac_rows,
    )
    write_tsv(
        os.path.join(args.out_dir, "drop_tables.tsv"),
        "# drop_table\titem\tweight\tcount\tadd_index (generated from the engine drop tables by tools/dump_stage_tables.py)",
        drop_rows,
    )
    print(f"wrote {os.path.join(args.out_dir, 'stages.tsv')} ({len(stage_rows)} rows)", file=sys.stderr)
    print(f"wrote {os.path.join(args.out_dir, 'facility_pos.tsv')} ({len(fac_rows)} rows)", file=sys.stderr)
    print(f"wrote {os.path.join(args.out_dir, 'drop_tables.tsv')} ({len(drop_rows)} rows)", file=sys.stderr)


if __name__ == "__main__":
    main()
