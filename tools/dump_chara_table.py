#!/usr/bin/env python3
"""Dump the game's class -> character asset table from a loadable ELF.

The engine keeps a per-CLASSTYPE record array (stride 0x58) whose male slot
(+0x08 mbs, +0x10 ftx) and female slot (+0x18 mbs, +0x20 ftx) name the model
under `Chara/`. `DrCreateChara`/`DrCreateNpc` resolve a def name to a CharaSet
whose class + gender pick the slot, so this table is the exact class -> mbs/ftx
mapping (no name heuristics). Regenerates `viewer/data/classes.tsv`.

    python3 tools/dump_chara_table.py main2.elf -o viewer/data/classes.tsv

The table base/relocation addresses are specific to the current main2.elf; they
were located by reading `FUN_001194e0` (the accessor) and `.rela.dyn`.
"""

import argparse
import struct
import sys

CLASS_TABLE_BASE = 0x2738FF8
CLASS_TABLE_STRIDE = 0x58
CLASS_COUNT = 74  # CLASSTYPE entries

M_MBS, M_FTX, F_MBS, F_FTX = 8, 16, 24, 32

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


def read_cstr(buf, segs, v):
    o = vaddr_to_off(segs, v) if v else None
    if o is None:
        return ""
    end = buf.find(b"\x00", o)
    if end < 0:
        return ""
    return buf[o:end].decode("utf-8", "replace")


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


def load_classes(path):
    """CLASSTYPE entries (name, jp) in enum order from uc_enums.tsv."""
    out = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            c = line.rstrip("\n").split("\t")
            if len(c) >= 2 and c[0] == "CLASSTYPE":
                out.append((c[1], c[2] if len(c) > 2 else ""))
    return out


def asset(reloc, segs, buf, v):
    s = read_cstr(buf, segs, read_ptr(buf, segs, reloc, v))
    return s[6:] if s.startswith("Chara/") else s


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("elf", help="loadable ELF (from nso2elf.py)")
    ap.add_argument("--enums", default="viewer/data/uc_enums.tsv")
    ap.add_argument("-o", "--output", default="-")
    args = ap.parse_args()

    buf = open(args.elf, "rb").read()
    segs = load_segments(buf)
    reloc = load_relocs(buf, segs)
    classes = load_classes(args.enums)
    if len(classes) < CLASS_COUNT:
        print(f"warning: only {len(classes)} CLASSTYPE entries in {args.enums}", file=sys.stderr)

    os_ = []
    os_.append(
        "# key\tjp_name\tmbs\tftx (generated from the engine CLASSTYPE table by "
        "tools/dump_chara_table.py)\n"
    )
    rows = 0
    for i in range(CLASS_COUNT):
        if i >= len(classes):
            break
        name, jp = classes[i]
        rec = CLASS_TABLE_BASE + i * CLASS_TABLE_STRIDE
        for suffix, om, of in (("M", M_MBS, M_FTX), ("F", F_MBS, F_FTX)):
            mbs = asset(reloc, segs, buf, rec + om)
            ftx = asset(reloc, segs, buf, rec + of)
            if not mbs:
                continue
            os_.append(f"{name}_{suffix}\t{jp}\t{mbs}\t{ftx}\n")
            rows += 1
    out = "".join(os_)
    if args.output == "-":
        sys.stdout.write(out)
    else:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(out)
        print(f"wrote {args.output} ({rows} rows)", file=sys.stderr)


if __name__ == "__main__":
    main()
