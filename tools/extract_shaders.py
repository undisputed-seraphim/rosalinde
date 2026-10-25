#!/usr/bin/env python3
"""Extract NVN shader programs and reflection names from the engine image.

A program starts at ``!!NVvp5.0`` / ``!!NVfp5.0`` and ends at the first
``END\\n``. The reflection block is the NUL-separated identifier run in the gap
between that ``END`` and the next program marker.

Outputs ``<vaddr>.asm``, ``<vaddr>.symbols`` and ``index.txt``.
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

from engine_data import EngineImage

PROGRAM = re.compile(rb"!!NV(?:vp|fp)5\.0")
TOKEN = re.compile(rb"[A-Za-z_][A-Za-z0-9_\[\]]*")


def reflection_runs(region: bytes) -> list[list[str]]:
    runs: list[list[str]] = []
    current: list[str] = []
    pos = 0
    while pos < len(region):
        match = TOKEN.match(region, pos)
        if not match:
            pos += 1
            current = []
            continue
        end = match.end()
        if end < len(region) and region[end] != 0:
            pos = end
            current = []
            continue
        current.append(match.group().decode())
        nxt = end + 1
        following = TOKEN.match(region, nxt)
        if following and following.end() < len(region) and region[following.end()] == 0:
            pos = nxt
        else:
            runs.append(current)
            current = []
            pos = nxt
    if current:
        runs.append(current)
    runs = [run for run in runs if len(run) >= 3]
    runs.sort(key=len, reverse=True)
    return runs


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--elf",
        type=Path,
        default=Path.home() / "zwei/unicorn_overlord/main2.elf",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("mbs_dump/shaders"),
    )
    args = parser.parse_args()

    image = EngineImage(args.elf)
    segment = image.segments[1]
    blob = image.read(segment.vaddr, segment.filesz)
    args.out.mkdir(parents=True, exist_ok=True)
    for old in args.out.iterdir():
        if old.is_file() and old.suffix in {".asm", ".symbols", ".txt"}:
            old.unlink()

    starts = [match.start() for match in PROGRAM.finditer(blob)]
    index: list[tuple[str, str, int, str]] = []
    for i, start in enumerate(starts):
        end = blob.find(b"END\n", start)
        if end < 0:
            continue
        end += 4
        text = blob[start:end]
        try:
            text.decode("utf-8")
        except UnicodeDecodeError:
            continue
        following = starts[i + 1] if i + 1 < len(starts) else len(blob)
        runs = reflection_runs(blob[end:following])
        names = runs[0] if runs else []
        vaddr = segment.vaddr + start
        stem = f"{vaddr:07x}"
        (args.out / f"{stem}.asm").write_bytes(text)
        if names:
            (args.out / f"{stem}.symbols").write_text("\n".join(names) + "\n")
        stage = "vp" if text.startswith(b"!!NVvp") else "fp"
        index.append((stem, stage, len(text), " ".join(names)))

    (args.out / "index.txt").write_text(
        "".join(f"{stem}\t{stage}\t{size}\t{symbols}\n" for stem, stage, size, symbols in index)
    )
    print(f"wrote {len(index)} programs to {args.out}")


if __name__ == "__main__":
    main()
