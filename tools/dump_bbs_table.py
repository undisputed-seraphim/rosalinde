#!/usr/bin/env python3
"""Dump the engine BBS catalog indexed by DrCreateStage stage type.

The catalog is an array of 185 rows at 0x8837c8 in the decompressed ELF
(stage type -> path, label). This is the selector used by DrCreateStage.

    python3 tools/dump_bbs_table.py ~/zwei/unicorn_overlord/main2.elf \
        --out viewer/data/bbs.tsv
"""

import argparse
import struct
import sys

from dump_stage_tables import load_segments, read_cstr, vaddr_to_off

TABLE_BASE = 0x8827C8
TABLE_STRIDE = 0x30
TABLE_COUNT = 185


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("elf", help="loadable ELF (from nso2elf.py)")
    ap.add_argument("--out", default="viewer/data/bbs.tsv")
    args = ap.parse_args()

    buf = open(args.elf, "rb").read()
    segs = load_segments(buf)

    rows = []
    for index in range(TABLE_COUNT):
        row = TABLE_BASE + index * TABLE_STRIDE
        path_addr = struct.unpack_from("<Q", buf, vaddr_to_off(segs, row))[0]
        label_addr = struct.unpack_from("<Q", buf, vaddr_to_off(segs, row + 0x18))[0]
        rows.append((index, read_cstr(buf, segs, path_addr), read_cstr(buf, segs, label_addr)))

    with open(args.out, "w", encoding="utf-8") as f:
        f.write("# stage_type\tpath\tlabel (generated from the engine BBS catalog by tools/dump_bbs_table.py)\n")
        for index, path, label in rows:
            if path:
                f.write(f"{index}\t{path}\t{label}\n")
    print(f"wrote {args.out} ({len(rows)} rows)", file=sys.stderr)


if __name__ == "__main__":
    main()
