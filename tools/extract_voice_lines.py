#!/usr/bin/env python3
"""Extract the message text -> voice cue table from the game executable.

The message window stores a sorted array of 8-byte little-endian pairs
``(message text index, voice ACB cue id)``.  The table has no header or
symbol, so locate it as the longest run of non-decreasing pairs whose first
field is a plausible text index and whose second field is a voice cue id.

The output is kept 1:1 with the binary table, including duplicate text IDs.
Runtime code uses a lower-bound lookup, so the first entry for a duplicated
text ID wins, matching the game's binary search.

Example:

    python3 tools/extract_voice_lines.py \
        --elf ~/zwei/unicorn_overlord/main2.elf \
        --out viewer/data/voice_line_table.tsv
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

MIN_RUN = 1000
MAX_TEXT_INDEX = 2_000_000
MIN_VOICE_CUE = 1_000_000
MAX_VOICE_CUE = 3_000_000


def candidate_pairs(data: bytes):
    for offset in range(0, len(data) - 8, 4):
        key, cue = struct.unpack_from("<II", data, offset)
        if 0 < key < MAX_TEXT_INDEX and MIN_VOICE_CUE <= cue < MAX_VOICE_CUE:
            yield offset, key, cue


def find_tables(data: bytes):
    tables = []
    for offset, key, cue in candidate_pairs(data):
        end = offset
        last_key = -1
        count = 0
        while end + 8 <= len(data):
            k, c = struct.unpack_from("<II", data, end)
            if not (0 < k < MAX_TEXT_INDEX and MIN_VOICE_CUE <= c < MAX_VOICE_CUE):
                break
            if k < last_key:
                break
            last_key = k
            end += 8
            count += 1
        if count >= MIN_RUN:
            tables.append((count, offset, end))
    return tables


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", required=True, help="game executable (main2.elf)")
    parser.add_argument("--out", required=True, help="output TSV")
    args = parser.parse_args()

    data = Path(args.elf).read_bytes()
    tables = find_tables(data)
    if not tables:
        raise SystemExit("no voice-line table found")
    count, start, end = max(tables)
    if len(tables) > 1:
        print(f"found {len(tables)} candidate tables; using the largest ({count} pairs)")

    rows = []
    for offset in range(start, end, 8):
        key, cue = struct.unpack_from("<II", data, offset)
        rows.append((key, cue))

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", encoding="utf-8", newline="\n") as file:
        file.write("text_id\tcue_id\n")
        for key, cue in rows:
            file.write(f"{key}\t{cue}\n")
    print(f"wrote {len(rows)} rows to {out} (table at 0x{start:x})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
