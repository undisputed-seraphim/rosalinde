#!/usr/bin/env python3
"""Dump the engine's country-region quads by emulating its constructor.

`GetCountryByPos` (0x303ba4) asks the field manager (0x205870) for the polygon
containing a point. The manager's constructor (0x202d00) builds a vector of
0x64-byte records at `this+0xc0`:

    +0x00  country id (0..4)
    +0x44  point 0 (x, y)
    +0x4c  point 1
    +0x54  point 2
    +0x5c  point 3

The records are hardcoded as immediate constants, so this tool maps the ELF,
stubs malloc/free/operator new, runs the constructor, and walks the resulting
vector. Writes `country_regions.tsv` (`id x0 y0 x1 y1 x2 y2 x3 y3`). The C++
loader applies the engine's 5-entry country map (3,4,1,0,0; outside -> 2).

Requires `unicorn` (`pip install unicorn`). The addresses are for the current
`main2.elf`, like the other exe-table dumpers.

    python3 tools/dump_country_regions.py ~/zwei/unicorn_overlord/main2.elf
"""

import argparse
import struct
import sys

try:
    from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
    from unicorn.arm64_const import (
        UC_ARM64_REG_PC,
        UC_ARM64_REG_LR,
        UC_ARM64_REG_SP,
        UC_ARM64_REG_X0,
    )
except ImportError:
    raise SystemExit("missing dependency: pip install unicorn")

CTOR = 0x202D00
SINGLETON = 0x28D5310
REC_STRIDE = 0x64
PT_OFF = 0x44
LR_SENTINEL = 0xDEADBEE0
HEAP = 0x40000000
STACK = 0x30000000
THIS = 0x20000000


def load_segments(buf):
    e_phoff, = struct.unpack_from("<Q", buf, 0x20)
    e_phentsize, e_phnum = struct.unpack_from("<HH", buf, 0x36)
    out = []
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, _fl, p_off, p_va, _pa, p_fsz, p_msz, _al = struct.unpack_from(
            "<IIQQQQQQ", buf, off
        )
        if p_type == 1:
            out.append((p_va, p_off, p_fsz, p_msz))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("elf", help="loadable ELF (from nso2elf.py)")
    ap.add_argument("-o", "--output", default="viewer/data/country_regions.tsv")
    args = ap.parse_args()

    buf = open(args.elf, "rb").read()
    uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)

    for va, off, fsz, msz in load_segments(buf):
        start = va & ~0xFFF
        total = ((va + msz + 0xFFF) & ~0xFFF) - start
        uc.mem_map(start, total, 7)
        uc.mem_write(va, buf[off:off + fsz])

    # .bss (the singleton), heap, stack and the `this` object.
    uc.mem_map(0x28CF000, 0x2000000, 7)
    uc.mem_map(HEAP, 0x4000000, 7)
    uc.mem_map(STACK, 0x100000, 7)
    uc.mem_map(THIS, 0x10000, 7)

    heap_ptr = [HEAP + 0x1000]

    def malloc_hook(uc, address, size, _user):
        n = uc.reg_read(UC_ARM64_REG_X0)
        p = heap_ptr[0]
        heap_ptr[0] = (heap_ptr[0] + n + 15) & ~15
        uc.reg_write(UC_ARM64_REG_X0, p)
        uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))

    def free_hook(uc, address, size, _user):
        uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))

    def ret0_hook(uc, address, size, _user):
        uc.reg_write(UC_ARM64_REG_X0, 0)
        uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))

    for a in (0x400,):
        uc.hook_add(UC_HOOK_CODE, malloc_hook, begin=a, end=a)
    for a in (0x410,):
        uc.hook_add(UC_HOOK_CODE, free_hook, begin=a, end=a)
    for a in (0x5A8828, 0x5A8908):
        uc.hook_add(UC_HOOK_CODE, ret0_hook, begin=a, end=a)

    uc.reg_write(UC_ARM64_REG_SP, STACK + 0x80000)
    uc.reg_write(UC_ARM64_REG_LR, LR_SENTINEL)
    uc.reg_write(UC_ARM64_REG_X0, THIS)
    try:
        uc.emu_start(CTOR, LR_SENTINEL, count=20000000)
    except Exception as e:
        raise SystemExit(f"emulation failed at {uc.reg_read(UC_ARM64_REG_PC):#x}: {e}")

    def rd64(a):
        return struct.unpack("<Q", uc.mem_read(a, 8))[0]

    obj = rd64(SINGLETON)
    if not obj:
        raise SystemExit("constructor did not set the singleton")
    begin = rd64(obj + 0xC0)
    end = rd64(obj + 0xC8)
    count = (end - begin) // REC_STRIDE
    if count <= 0:
        raise SystemExit("empty country vector")

    rows = []
    for i in range(count):
        rec = begin + i * REC_STRIDE
        cid = struct.unpack("<i", uc.mem_read(rec, 4))[0]
        pts = struct.unpack("<8f", uc.mem_read(rec + PT_OFF, 32))
        rows.append((cid, *pts))

    with open(args.output, "w", encoding="utf-8") as f:
        f.write(
            "# id\tx0\ty0\tx1\ty1\tx2\ty2\tx3\ty3 "
            "(emulated engine country constructor " + hex(CTOR) + ")\n"
        )
        for r in rows:
            f.write("\t".join(str(x) for x in r) + "\n")
    print(f"wrote {args.output} ({len(rows)} regions)", file=sys.stderr)


if __name__ == "__main__":
    main()
