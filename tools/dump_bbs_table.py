#!/usr/bin/env python3
"""Dump the engine BBS catalog indexed by DrCreateStage stage type.

The catalog is an array of 185 rows at 0x8827c8 in the decompressed ELF
(stage type -> path, label). This is the selector used by DrCreateStage.

    python3 tools/dump_bbs_table.py ~/zwei/unicorn_overlord/main2.elf \
        --out viewer/data/generated/bbs.tsv
"""

import argparse
import sys

from engine_data import EngineImage

TABLE_BASE = 0x8827C8
TABLE_STRIDE = 0x30
TABLE_COUNT = 185


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("elf", help="loadable ELF (from nso2elf.py)")
    ap.add_argument("--out", default="viewer/data/generated/bbs.tsv")
    args = ap.parse_args()

    image = EngineImage(args.elf)

    rows = []
    for index in range(TABLE_COUNT):
        row = TABLE_BASE + index * TABLE_STRIDE
        path = image.read_cstr(image.read_ptr(row))
        label = image.read_cstr(image.read_ptr(row + 0x18))
        rows.append((index, path, label))

    with open(args.out, "w", encoding="utf-8") as file:
        file.write(
            "# stage_type\tpath\tlabel "
            "(generated from the engine BBS catalog by tools/dump_bbs_table.py)\n"
        )
        for index, path, label in rows:
            if path:
                file.write(f"{index}\t{path}\t{label}\n")
    print(f"wrote {args.out} ({len(rows)} rows)", file=sys.stderr)


if __name__ == "__main__":
    main()
