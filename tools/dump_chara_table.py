#!/usr/bin/env python3
"""Dump the game's class -> character asset table from a loadable ELF.

The engine keeps a per-CLASSTYPE record array (stride 0x58) whose male slot
(+0x08 mbs, +0x10 ftx) and female slot (+0x18 mbs, +0x20 ftx) name the model
under `Chara/`. `DrCreateChara`/`DrCreateNpc` resolve a def name to a CharaSet
whose class + gender pick the slot, so this table is the exact class -> mbs/ftx
mapping (no name heuristics). Regenerates `viewer/data/generated/classes.tsv`.

    python3 tools/dump_chara_table.py main2.elf -o viewer/data/generated/classes.tsv

The table base/relocation addresses are specific to the current main2.elf; they
were located by reading `class_table_relation_matches` and `.rela.dyn`.
"""

import argparse
import sys

from engine_data import EngineImage, load_enum

CLASS_TABLE_BASE = 0x2738FF8
CLASS_TABLE_STRIDE = 0x58
CLASS_COUNT = 74

M_MBS, M_FTX, F_MBS, F_FTX = 8, 16, 24, 32


def asset(image, address):
    value = image.read_cstr(image.read_ptr(address))
    return value[6:] if value.startswith("Chara/") else value


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("elf", help="loadable ELF (from nso2elf.py)")
    ap.add_argument("--enums", default="viewer/data/generated/uc_enums.tsv")
    ap.add_argument("-o", "--output", default="-")
    args = ap.parse_args()

    image = EngineImage(args.elf)
    classes = load_enum(args.enums, "CLASSTYPE")
    if len(classes) < CLASS_COUNT:
        print(f"warning: only {len(classes)} CLASSTYPE entries in {args.enums}", file=sys.stderr)

    rows = []
    rows.append(
        "# key\tjp_name\tmbs\tftx (generated from the engine CLASSTYPE table by "
        "tools/dump_chara_table.py)\n"
    )
    count = 0
    for i in range(CLASS_COUNT):
        if i >= len(classes):
            break
        name, jp = classes[i]
        rec = CLASS_TABLE_BASE + i * CLASS_TABLE_STRIDE
        for suffix, om, of in (("M", M_MBS, M_FTX), ("F", F_MBS, F_FTX)):
            mbs = asset(image, rec + om)
            ftx = asset(image, rec + of)
            if not mbs:
                continue
            rows.append(f"{name}_{suffix}\t{jp}\t{mbs}\t{ftx}\n")
            count += 1
    text = "".join(rows)
    if args.output == "-":
        sys.stdout.write(text)
    else:
        open(args.output, "w", encoding="utf-8").write(text)
        print(f"wrote {args.output} ({count} rows)", file=sys.stderr)


if __name__ == "__main__":
    main()
