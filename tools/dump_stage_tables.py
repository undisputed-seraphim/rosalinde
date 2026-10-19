#!/usr/bin/env python3
"""Dump the engine's stage/facility/drop tables from a loadable ELF.

The tables live in the engine's `.data` (not the CPK) and are indexed by the
names/ids the scripts pass:

- stage table    0x28920d0, stride 0x50, 103 records: name (+0), base level (+0xc),
  country (+0x10)
- stage cue table 0x285d9f8, stride 0x10, 103 records: name (+0), cue id (+8)
- facility table 0x27b31b0, stride 0x90, 485 records: label (+8), pos (+0x1c/+0x20)
- drop table     0x26e5d30, stride 0x18, 733 records: start (+0x10), count (+0x14);
  entries at 0xaee6d0, stride 0x10: item id (+0), weight (+4), count (+8)

Writes:

    viewer/data/stages.tsv        stage  base_lv  cue  country
    viewer/data/facility_pos.tsv  key    label    x  y
    viewer/data/drop_tables.tsv   drop_table  item  weight  count  add_index

The stage table names are `ST_OW_*` and match the 103 `St_OW_*.asb` scripts;
facility records are index-aligned with `ST_FACILITYID` (485 entries).

    python3 tools/dump_stage_tables.py ~/zwei/unicorn_overlord/main2.elf
"""

import argparse
import os
import struct
import sys

STAGE_BASE, STAGE_STRIDE, STAGE_COUNT = 0x28920D0, 0x50, 103
CUE_BASE, CUE_STRIDE, CUE_COUNT = 0x285D9F8, 0x10, 103
FAC_BASE, FAC_STRIDE, FAC_COUNT = 0x27B31B0, 0x90, 485
DROP_BASE, DROP_STRIDE, DROP_COUNT = 0x26E5D30, 0x18, 733
DROP_ENTRY_BASE, DROP_ENTRY_STRIDE = 0xAEE6D0, 0x10

RELA_BASE = 0x82A048
RELA_COUNT = 98211
RELA_ENT = 24
PT_LOAD = 1


def load_segments(buf):
    e_phoff, = struct.unpack_from("<Q", buf, 0x20)
    e_phentsize, e_phnum = struct.unpack_from("<HH", buf, 0x36)
    segs = []
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, _flags, p_offset, p_vaddr, _pa, p_filesz, _ms, _al = struct.unpack_from(
            "<IIQQQQQQ", buf, off
        )
        if p_type == PT_LOAD:
            segs.append((p_vaddr, p_offset, p_filesz))
    return segs


def vaddr_to_off(segs, v):
    for base, off, sz in segs:
        if base <= v < base + sz:
            return off + (v - base)
    return None


def load_relocs(buf, segs):
    reloc = {}
    base_off = vaddr_to_off(segs, RELA_BASE)
    if base_off is None:
        raise SystemExit("no relocation table at the expected address")
    for i in range(RELA_COUNT):
        r_offset, _r_info, r_addend = struct.unpack_from("<QQQ", buf, base_off + i * RELA_ENT)
        reloc[r_offset] = r_addend
    return reloc


def read_ptr(buf, segs, reloc, v):
    if v in reloc:
        return reloc[v]
    o = vaddr_to_off(segs, v)
    if o is None:
        return 0
    return struct.unpack_from("<Q", buf, o)[0]


def read_cstr(buf, segs, v):
    o = vaddr_to_off(segs, v) if v else None
    if o is None:
        return ""
    end = buf.find(b"\x00", o)
    if end < 0:
        return ""
    return buf[o:end].decode("utf-8", "replace")


def read_i32(buf, segs, v):
    o = vaddr_to_off(segs, v)
    return struct.unpack_from("<i", buf, o)[0] if o is not None else 0


def read_f32x2(buf, segs, v):
    o = vaddr_to_off(segs, v)
    return struct.unpack_from("<ff", buf, o) if o is not None else (0.0, 0.0)


def load_enum(path, kind):
    out = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            c = line.rstrip("\n").split("\t")
            if len(c) >= 2 and c[0] == kind:
                out.append((c[1], c[2] if len(c) > 2 else ""))
    return out


def write_tsv(path, header, rows):
    with open(path, "w", encoding="utf-8") as f:
        f.write(header + "\n")
        for r in rows:
            f.write("\t".join(str(x) for x in r) + "\n")
    print(f"wrote {path} ({len(rows)} rows)", file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("elf", help="loadable ELF (from nso2elf.py)")
    ap.add_argument("--enums", default="viewer/data/uc_enums.tsv")
    ap.add_argument("--out-dir", default="viewer/data")
    args = ap.parse_args()

    buf = open(args.elf, "rb").read()
    segs = load_segments(buf)
    reloc = load_relocs(buf, segs)

    facilities = load_enum(args.enums, "ST_FACILITYID")
    drops = load_enum(args.enums, "DROPTABLE")
    items = load_enum(args.enums, "ITEMID")

    cues = {}
    for i in range(CUE_COUNT):
        rec = CUE_BASE + i * CUE_STRIDE
        name = read_cstr(buf, segs, read_ptr(buf, segs, reloc, rec))
        cues[name] = read_i32(buf, segs, rec + 8)

    stage_rows = []
    for i in range(STAGE_COUNT):
        rec = STAGE_BASE + i * STAGE_STRIDE
        name = read_cstr(buf, segs, read_ptr(buf, segs, reloc, rec))
        country = read_i32(buf, segs, rec + 0x10)
        stage_rows.append((name, read_i32(buf, segs, rec + 0xC), cues.get(name, -1), country))

    fac_rows = []
    for i in range(FAC_COUNT):
        rec = FAC_BASE + i * FAC_STRIDE
        label = read_cstr(buf, segs, read_ptr(buf, segs, reloc, rec + 8))
        key = facilities[i][0] if i < len(facilities) else f"FAC_{i}"
        x, y = read_f32x2(buf, segs, rec + 0x1C)
        fac_rows.append((key, label, x, y))

    drop_rows = []
    for i in range(DROP_COUNT):
        rec = DROP_BASE + i * DROP_STRIDE
        start = read_i32(buf, segs, rec + 0x10)
        count = read_i32(buf, segs, rec + 0x14)
        drop_name = drops[i][0] if i < len(drops) else f"DROP_{i}"
        for j in range(max(0, count)):
            ent = DROP_ENTRY_BASE + (start + j) * DROP_ENTRY_STRIDE
            item_id = read_i32(buf, segs, ent)
            weight = read_i32(buf, segs, ent + 4)
            n = read_i32(buf, segs, ent + 8)
            add_index = read_i32(buf, segs, ent + 12)
            item = items[item_id][0] if 0 <= item_id < len(items) else f"ITEM_{item_id}"
            drop_rows.append((drop_name, item, weight, n, add_index))

    os.makedirs(args.out_dir, exist_ok=True)
    write_tsv(
        os.path.join(args.out_dir, "stages.tsv"),
        "# stage\tbase_lv\tcue\tcountry (generated from the engine stage/cue tables by tools/dump_stage_tables.py)",
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


if __name__ == "__main__":
    main()
