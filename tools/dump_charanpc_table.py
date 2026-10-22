#!/usr/bin/env python3
"""Dump the game's CharaNpc (NPCTYPE) asset table from a loadable ELF.

`DrCreateNpc(objectName, defName, a, b)` resolves `defName` through
`npctype_find_by_label`, a linear search of the per-NPCTYPE record array
comparing the Japanese display name (record word 0). Each record names the
model / icon assets under `CharaNpc/` and `GUI/IconNpc_*`. Emits
`viewer/data/generated/charanpc_assets.tsv`.

    python3 tools/dump_charanpc_table.py main2.elf -o viewer/data/generated/charanpc_assets.tsv

Base/relocation addresses are specific to the current main2.elf (located via
`npctype_get_record` / `npctype_find_by_label`).
"""

import argparse
import sys

from engine_data import EngineImage, load_enum

NPCTYPE_BASE = 0x273A968
NPCTYPE_STRIDE = 0x50
NPCTYPE_COUNT = 320

MBS, FTX = 8, 16


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("elf")
    ap.add_argument("--enums", default="viewer/data/generated/uc_enums.tsv")
    ap.add_argument("-o", "--output", default="-")
    args = ap.parse_args()

    image = EngineImage(args.elf)
    types = load_enum(args.enums, "NPCTYPE")

    out = [
        "# idx\tname\tjp\tmbs\tftx (generated from the engine NPCTYPE table by "
        "tools/dump_charanpc_table.py)\n"
    ]
    rows = 0
    for i in range(NPCTYPE_COUNT):
        if i >= len(types):
            break
        rec = NPCTYPE_BASE + i * NPCTYPE_STRIDE
        mbs = image.read_cstr(image.read_ptr(rec + MBS))
        if not mbs:
            continue
        ftx = image.read_cstr(image.read_ptr(rec + FTX))
        name, jp = types[i]
        out.append(f"{i}\t{name}\t{jp}\t{mbs}\t{ftx}\n")
        rows += 1
    text = "".join(out)
    if args.output == "-":
        sys.stdout.write(text)
    else:
        open(args.output, "w", encoding="utf-8").write(text)
        print(f"wrote {args.output} ({rows} rows)", file=sys.stderr)


if __name__ == "__main__":
    main()
