#!/usr/bin/env python3
"""Shared reader for the Unicorn Overlord engine image.

The executable is imported as a loadable AArch64 ELF by `tools/nso2elf.py`.
Its `.data` pointer fields are zero in the file and are only meaningful after
applying `.rela.dyn`; `.rodata` contains some raw absolute pointers of its own.
This module keeps those details in one place for the table dumpers.
"""

from __future__ import annotations

import struct
from collections import Counter
from dataclasses import dataclass
from pathlib import Path

RELA_BASE = 0x82A048
RELA_COUNT = 98211
RELA_ENT = 24
PT_LOAD = 1


@dataclass(frozen=True)
class Segment:
    vaddr: int
    offset: int
    filesz: int
    memsz: int

    @property
    def end(self) -> int:
        return self.vaddr + self.filesz


class EngineImage:
    def __init__(self, path):
        self.path = Path(path)
        self.buf = self.path.read_bytes()
        self.segments = self._read_segments()
        self.relocations = self._read_relocations()

    def _read_segments(self):
        e_phoff, = struct.unpack_from("<Q", self.buf, 0x20)
        e_phentsize, e_phnum = struct.unpack_from("<HH", self.buf, 0x36)
        segments = []
        for i in range(e_phnum):
            off = e_phoff + i * e_phentsize
            p_type, _flags, p_offset, p_vaddr, _paddr, p_filesz, p_memsz, _align = (
                struct.unpack_from("<IIQQQQQQ", self.buf, off)
            )
            if p_type == PT_LOAD:
                segments.append(Segment(p_vaddr, p_offset, p_filesz, p_memsz))
        return segments

    def _read_relocations(self):
        base = self.vaddr_to_offset(RELA_BASE)
        if base is None:
            raise ValueError("no relocation table at the expected address")
        relocations = {}
        for i in range(RELA_COUNT):
            r_offset, _r_info, r_addend = struct.unpack_from(
                "<QQQ", self.buf, base + i * RELA_ENT
            )
            relocations[r_offset] = r_addend
        return relocations

    def vaddr_to_offset(self, vaddr):
        for segment in self.segments:
            if segment.vaddr <= vaddr < segment.end:
                return segment.offset + (vaddr - segment.vaddr)
        return None

    def read(self, vaddr, size):
        offset = self.vaddr_to_offset(vaddr)
        if offset is None or offset + size > len(self.buf):
            raise ValueError(f"address out of range: {vaddr:#x}+{size:#x}")
        return self.buf[offset : offset + size]

    def read_u8(self, vaddr):
        return self.read(vaddr, 1)[0]

    def read_u16(self, vaddr):
        return struct.unpack("<H", self.read(vaddr, 2))[0]

    def read_u32(self, vaddr):
        return struct.unpack("<I", self.read(vaddr, 4))[0]

    def read_i32(self, vaddr):
        return struct.unpack("<i", self.read(vaddr, 4))[0]

    def read_u64(self, vaddr):
        return struct.unpack("<Q", self.read(vaddr, 8))[0]

    def read_f32(self, vaddr):
        return struct.unpack("<f", self.read(vaddr, 4))[0]

    def read_f32x2(self, vaddr):
        return struct.unpack("<ff", self.read(vaddr, 8))

    def read_ptr(self, vaddr):
        if vaddr in self.relocations:
            return self.relocations[vaddr]
        try:
            return self.read_u64(vaddr)
        except ValueError:
            return 0

    def read_cstr(self, vaddr, limit=512):
        if not vaddr:
            return ""
        offset = self.vaddr_to_offset(vaddr)
        if offset is None:
            return ""
        end = self.buf.find(b"\0", offset, min(len(self.buf), offset + limit))
        if end < 0:
            return ""
        try:
            return self.buf[offset:end].decode("utf-8")
        except UnicodeDecodeError:
            return ""

    def string_at(self, vaddr, limit=512):
        text = self.read_cstr(vaddr, limit)
        if not text:
            return None
        if any(ord(ch) < 0x20 and ch not in "\t\n\r" for ch in text):
            return None
        return text

    def text_segment(self):
        for segment in self.segments:
            if segment.vaddr == 0:
                return segment
        return None

    def data_segments(self):
        return [segment for segment in self.segments if segment.vaddr != 0]


def load_enum(path, kind):
    rows = []
    with open(path, encoding="utf-8") as file:
        for line in file:
            columns = line.rstrip("\n").split("\t")
            if len(columns) >= 2 and columns[0] == kind:
                rows.append((columns[1], columns[2] if len(columns) > 2 else ""))
    return rows


def write_tsv(path, header, rows, strip_trailing=False):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as file:
        file.write(header + "\n")
        for row in rows:
            line = "\t".join(str(value) for value in row)
            if strip_trailing:
                line = line.rstrip("\t")
            file.write(line + "\n")


def _is_adrp(insn):
    return (insn & 0x9F000000) == 0x90000000


def _decode_adrp(insn, pc):
    rd = insn & 0x1F
    immhi = (insn >> 5) & 0x7FFFF
    immlo = (insn >> 29) & 0x3
    imm = (immhi << 2) | immlo
    if imm & (1 << 20):
        imm -= 1 << 21
    return rd, (pc & ~0xFFF) + (imm << 12)


def _decode_add_imm(insn):
    if (insn & 0x7F000000) != 0x11000000:
        return None
    if not (insn & 0x80000000):
        return None
    if insn & 0x20000000:
        return None
    shift = (insn >> 22) & 1
    imm12 = (insn >> 10) & 0xFFF
    rn = (insn >> 5) & 0x1F
    rd = insn & 0x1F
    return rd, rn, imm12 << 12 if shift else imm12


def scan_code_refs(image):
    """Return exact ADRP+ADD targets in text, with reference counts."""
    segment = image.text_segment()
    if segment is None:
        return Counter()
    text = image.buf[segment.offset : segment.offset + segment.filesz]
    adrp = {}
    base = {}
    refs = Counter()
    for offset in range(0x30, len(text) - 4, 4):
        pc = segment.vaddr + offset
        insn = struct.unpack_from("<I", text, offset)[0]
        if _is_adrp(insn):
            rd, page = _decode_adrp(insn, pc)
            adrp[rd] = page
            base.pop(rd, None)
            continue
        add = _decode_add_imm(insn)
        if add is not None:
            rd, rn, imm = add
            if rn in adrp:
                value = adrp[rn] + imm
            elif rn in base:
                value = base[rn] + imm
            else:
                base.pop(rd, None)
                continue
            base[rd] = value
            refs[value] += 1
    return refs
