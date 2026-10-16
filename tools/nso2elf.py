#!/usr/bin/env python3
"""Convert a Nintendo Switch NSO0 executable into a loadable AArch64 ELF.

Each NSO segment (text/rodata/data) is one raw LZ4 block; rebuild an ELF with
three PT_LOADs so Ghidra/radare2 can disassemble the image at module base 0.

    python3 tools/nso2elf.py <main.nso> -o <main.elf>
"""

import argparse
import struct
import sys

import lz4.block

# NSO0 header field offsets (all little-endian u32).
NSO_MAGIC = 0x00
FLAGS = 0x0C
TEXT = (0x10, 0x14, 0x18)  # file offset, memory offset, decompressed size
MOD0 = 0x1C
RODATA = (0x20, 0x24, 0x28)
DATA = (0x30, 0x34, 0x38)
BSS_SIZE = 0x3C
COMPRESSED = (0x60, 0x64, 0x68)  # text, rodata, data

PT_LOAD = 1
PF_X, PF_W, PF_R = 1, 2, 4


def u32(buf, off):
    return struct.unpack_from("<I", buf, off)[0]


def read_segment(buf, fields, comp_off):
    file_off, mem_off, decomp_size = (u32(buf, f) for f in fields)
    comp_size = u32(buf, comp_off)
    block = buf[file_off : file_off + comp_size]
    out = lz4.block.decompress(block, uncompressed_size=decomp_size)
    if len(out) != decomp_size:
        raise SystemExit(
            f"segment at {file_off:#x}: decompressed {len(out)} != {decomp_size}"
        )
    return mem_off, out


def build_elf(segments, entry=0):
    """segments: list of (vaddr, bytes, flags)."""
    ehsize, phentsize = 64, 56
    phoff = ehsize
    header_size = ehsize + phentsize * len(segments)
    file_off = 0x1000  # keep segment file offsets page aligned and clear of headers

    phdrs = b""
    body = b""
    for vaddr, data, flags in segments:
        file_off = (file_off + 0xFFF) & ~0xFFF
        body += b"\x00" * (file_off - (header_size + len(body)))
        phdrs += struct.pack(
            "<IIQQQQQQ",
            PT_LOAD,
            flags,
            file_off,
            vaddr,
            vaddr,
            len(data),
            len(data),
            0x1000,
        )
        body += data
        file_off += len(data)

    ident = b"\x7fELF" + bytes([2, 1, 1, 0]) + b"\x00" * 8  # 64-bit LE, current ver
    ehdr = ident + struct.pack(
        "<HHIQQQIHHHHHH",
        2,  # ET_EXEC
        183,  # EM_AARCH64
        1,
        entry,
        phoff,
        0,
        0,
        ehsize,
        phentsize,
        len(segments),
        0,
        0,
        0,
    )
    return ehdr + phdrs + body


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("nso", help="input NSO0 file")
    ap.add_argument("-o", "--output", required=True, help="output ELF path")
    args = ap.parse_args()

    buf = open(args.nso, "rb").read()
    if buf[:4] != b"NSO0":
        raise SystemExit(f"{args.nso}: not an NSO0 file")
    flags = u32(buf, FLAGS)
    if flags & 0x7 != 0x7:
        raise SystemExit(f"expected all segments compressed, flags={flags:#x}")

    text = read_segment(buf, TEXT, COMPRESSED[0])
    rodata = read_segment(buf, RODATA, COMPRESSED[1])
    data = read_segment(buf, DATA, COMPRESSED[2])
    bss = u32(buf, BSS_SIZE)

    print(f"text   vaddr={text[0]:#x} size={len(text[1]):#x}", file=sys.stderr)
    print(f"rodata vaddr={rodata[0]:#x} size={len(rodata[1]):#x}", file=sys.stderr)
    print(f"data   vaddr={data[0]:#x} size={len(data[1]):#x} bss={bss:#x}", file=sys.stderr)

    elf = build_elf(
        [
            (text[0], text[1], PF_R | PF_X),
            (rodata[0], rodata[1], PF_R),
            (data[0], data[1], PF_R | PF_W),
        ]
    )
    with open(args.output, "wb") as f:
        f.write(elf)
    print(f"wrote {args.output} ({len(elf)} bytes)", file=sys.stderr)


if __name__ == "__main__":
    main()
