#!/usr/bin/env python3
"""Dump the game's CharaNpc (NPCTYPE) asset table from a loadable ELF.

`DrCreateNpc(objectName, defName, a, b)` resolves `defName` through
`FUN_0011aac0`, a linear search of the per-NPCTYPE record array comparing the
Japanese display name (record word 0). Each record names the model / icon assets
under `CharaNpc/` and `GUI/IconNpc_*`. Emits `viewer/data/charanpc_assets.tsv`.

    python3 tools/dump_charanpc_table.py main2.elf -o viewer/data/charanpc_assets.tsv

Base/relocation addresses are specific to the current main2.elf (located via
`FUN_0011a890`/`FUN_0011aac0`, the NPCTYPE accessors).
"""

import argparse
import struct
import sys

NPCTYPE_BASE = 0x273A968
NPCTYPE_STRIDE = 0x50
NPCTYPE_COUNT = 320  # _UcEnum_CharaNpc.inc entries (index 0 = UNKNOWN)

KEY, MBS, FTX = 0, 8, 16

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
        p_type, _f, p_offset, p_vaddr, _pa, p_filesz, _ms, _al = struct.unpack_from("<IIQQQQQQ", buf, off)
        if p_type == PT_LOAD:
            segs.append((p_vaddr, p_offset, p_filesz))
    return segs


def vaddr_to_off(segs, v):
    for base, off, sz in segs:
        if base <= v < base + sz:
            return off + (v - base)
    return None


def read_cstr(buf, segs, v):
    o = vaddr_to_off(segs, v) if v else None
    if o is None:
        return ""
    end = buf.find(b"\x00", o)
    return buf[o:end].decode("utf-8", "replace") if end >= 0 else ""


def load_relocs(buf, segs):
    reloc = {}
    b = vaddr_to_off(segs, RELA_BASE)
    if b is None:
        raise SystemExit("no relocation table at the expected address")
    for i in range(RELA_COUNT):
        r_offset, _ri, r_addend = struct.unpack_from("<QQQ", buf, b + i * RELA_ENT)
        reloc[r_offset] = r_addend
    return reloc


def read_ptr(buf, segs, reloc, v):
    if v in reloc:
        return reloc[v]
    o = vaddr_to_off(segs, v)
    return struct.unpack_from("<Q", buf, o)[0] if o is not None else 0


def read_str(buf, segs, reloc, v):
    return read_cstr(buf, segs, read_ptr(buf, segs, reloc, v))


def load_npctypes(path):
    out = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            c = line.rstrip("\n").split("\t")
            if len(c) >= 2 and c[0] == "NPCTYPE":
                out.append((c[1], c[2] if len(c) > 2 else ""))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("elf")
    ap.add_argument("--enums", default="viewer/data/uc_enums.tsv")
    ap.add_argument("-o", "--output", default="-")
    args = ap.parse_args()

    buf = open(args.elf, "rb").read()
    segs = load_segments(buf)
    reloc = load_relocs(buf, segs)
    types = load_npctypes(args.enums)

    out = ["# idx\tname\tjp\tmbs\tftx (generated from the engine NPCTYPE table by "
           "tools/dump_charanpc_table.py)\n"]
    rows = 0
    for i in range(NPCTYPE_COUNT):
        if i >= len(types):
            break
        rec = NPCTYPE_BASE + i * NPCTYPE_STRIDE
        mbs = read_str(buf, segs, reloc, rec + MBS)
        if not mbs:
            continue
        ftx = read_str(buf, segs, reloc, rec + FTX)
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
