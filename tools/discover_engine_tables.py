#!/usr/bin/env python3
"""Discover static table candidates in the engine image.

This is intentionally a discovery tool, not a dumper. It finds runs of
pointer-to-string columns and annotates them with code references and known
catalog matches. A run is not asserted to be a table until its accessor is
recovered and its row layout is documented.
"""

from __future__ import annotations

import argparse
import json
from bisect import bisect_left
from pathlib import Path

from engine_data import EngineImage, scan_code_refs, write_tsv
from engine_tables import DEFAULT_ELF, TABLES


def string_pointer_positions(image):
    positions = {}
    for segment in image.data_segments():
        for address in range(segment.vaddr, segment.end - 8, 4):
            pointer = image.read_ptr(address)
            text = image.string_at(pointer)
            if text:
                positions[address] = (pointer, text)
    return positions


def find_runs(positions, min_count=8):
    runs = []
    for stride in range(8, 0x101, 4):
        for start in positions:
            if start - stride in positions:
                continue
            count = 0
            address = start
            while address in positions:
                count += 1
                address += stride
            if count >= min_count:
                runs.append(
                    {
                        "column": start,
                        "stride": stride,
                        "count": count,
                        "end": start + (count - 1) * stride,
                    }
                )
    runs.sort(key=lambda run: (-run["count"], run["stride"], run["column"]))
    filtered = []
    for run in runs:
        contained = False
        for other in filtered:
            if other["stride"] <= run["stride"] and run["stride"] % other["stride"] == 0:
                if (run["column"] - other["column"]) % other["stride"] == 0 and other["end"] >= run["end"]:
                    contained = True
                    break
        if not contained:
            filtered.append(run)
    return filtered


def known_match(column, stride):
    for table in TABLES:
        start = table.base
        end = table.base + table.stride * table.count
        if start <= column < end and table.stride == stride:
            return table
    return None


def reference_count(refs, start, end):
    addresses = refs
    if not addresses:
        return 0
    keys = sorted(addresses)
    lo = bisect_left(keys, start)
    hi = bisect_left(keys, end)
    return sum(addresses[key] for key in keys[lo:hi])


def discover(image, min_count=8):
    positions = string_pointer_positions(image)
    runs = find_runs(positions, min_count)
    refs = scan_code_refs(image)
    records = []
    seen_known = set()
    for run in runs:
        table = known_match(run["column"], run["stride"])
        if table is not None:
            if table.name in seen_known:
                continue
            seen_known.add(table.name)
            name = table.name
            base = table.base
            stride = table.stride
            count = table.count
            end = table.base + (table.count - 1) * table.stride
            confidence = "known"
        else:
            name = f"candidate_{run['column']:x}"
            base = run["column"]
            stride = run["stride"]
            count = run["count"]
            end = run["end"]
            confidence = "candidate"
        code_refs = reference_count(refs, base, end + 1)
        if table is None and code_refs:
            confidence = "strong"
        samples = []
        address = run["column"]
        for _ in range(min(3, count)):
            samples.append(positions[address][1][:80])
            address += run["stride"]
        records.append(
            {
                "name": name,
                "base": f"0x{base:x}",
                "column": f"0x{run['column']:x}",
                "stride": f"0x{stride:x}",
                "count": count,
                "end": f"0x{end:x}",
                "code_refs": code_refs,
                "confidence": confidence,
                "samples": samples,
                "kind": "string_column",
            }
        )
    known = [
        {
            "name": table.name,
            "base": f"0x{table.base:x}",
            "stride": f"0x{table.stride:x}",
            "count": table.count,
            "kind": table.kind,
            "confidence": table.confidence,
            "source": table.source,
            "notes": table.notes,
        }
        for table in TABLES
    ]
    return {
        "image": str(image.path),
        "segments": [
            {"vaddr": f"0x{segment.vaddr:x}", "filesz": segment.filesz, "memsz": segment.memsz}
            for segment in image.segments
        ],
        "known": known,
        "columns": records,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", nargs="?", default=str(DEFAULT_ELF))
    parser.add_argument("--out", default="build/engine_tables/discovery.json")
    parser.add_argument("--tsv", default="build/engine_tables/discovery.tsv")
    parser.add_argument("--min-count", type=int, default=8)
    args = parser.parse_args()

    image = EngineImage(args.elf)
    report = discover(image, args.min_count)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    rows = [
        (
            record["name"],
            record["base"],
            record["column"],
            record["stride"],
            record["count"],
            record["code_refs"],
            record["confidence"],
            record["samples"][0] if record["samples"] else "",
        )
        for record in report["columns"]
    ]
    write_tsv(
        args.tsv,
        "# name\tbase\tcolumn\tstride\tcount\tcode_refs\tconfidence\tsample",
        rows,
    )
    print(f"wrote {out} ({len(report['columns'])} columns)", flush=True)
    print(f"wrote {args.tsv}", flush=True)


if __name__ == "__main__":
    main()
