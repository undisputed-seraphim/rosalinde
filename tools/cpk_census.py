#!/usr/bin/env python3
"""Build CPK-derived field/audio census tables with rosalinde dumps.

The source formats are already parsed by rosalinde. This tool only asks the
existing inspectors for their JSON summaries (`d ... --json --stats`) and joins
the result with the UFB string tables. It does not parse binary layouts here.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from collections import defaultdict
from pathlib import Path

DEFAULT_ROSALINDE = Path("build/rosalinde")
DEFAULT_OUT = Path("viewer/data/generated")
OTM_CHUNK = 200

ASSET_RE = re.compile(r"^AC(?P<area>\d+)_(?P<variant>\d+)(?P<layer>[pPfF])(?P<sub>\d*)$")


class CensusError(RuntimeError):
    pass


def write_tsv(path: Path, header: str, rows) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as file:
        file.write(header + "\n")
        for row in rows:
            file.write("\t".join(str(value) for value in row) + "\n")


def json_sequence(text: str):
    decoder = json.JSONDecoder()
    documents = []
    offset = 0
    while True:
        while offset < len(text) and text[offset].isspace():
            offset += 1
        if offset >= len(text):
            return documents
        document, offset = decoder.raw_decode(text, offset)
        documents.append(document)


class Rosalinde:
    def __init__(self, binary: Path, cpk: Path):
        self.binary = binary
        self.cpk = cpk

    def script(self, commands) -> str:
        text = "\n".join([*commands, "q"]) + "\n"
        result = subprocess.run(
            [str(self.binary), "--cpk", str(self.cpk)],
            input=text,
            text=True,
            capture_output=True,
            check=False,
        )
        if result.returncode != 0:
            detail = result.stderr.strip() or result.stdout[-1000:]
            raise CensusError(f"rosalinde failed ({result.returncode}): {detail}")
        return result.stdout

    def json_commands(self, commands):
        return json_sequence(self.script(commands))

    def find(self, directory: str, suffix: str):
        text = self.script([f"find {directory} {suffix}"])
        return [line.strip() for line in text.splitlines() if line.strip().startswith(directory)]


def normalize_path(path: str) -> str:
    return path.replace("\\", "/")


def build_assets(ufb, out_dir: Path) -> int:
    rows = []
    for index, asset in enumerate(ufb.get("assets", [])):
        main = normalize_path(asset["main"])
        texture = normalize_path(asset["texture"])
        stem = Path(main).stem
        match = ASSET_RE.match(stem)
        if match:
            area = match.group("area")
            variant = match.group("variant")
            layer = match.group("layer").lower()
            sub = match.group("sub")
        else:
            area = variant = layer = sub = ""
        rows.append(
            (
                index,
                main,
                texture,
                stem,
                Path(main).suffix.lower().lstrip("."),
                area,
                variant,
                layer,
                sub,
            )
        )
    write_tsv(
        out_dir / "field_assets.tsv",
        "# idx\tmain\ttexture\tstem\ttype\tarea\tvariant\tlayer\tsub",
        rows,
    )
    return len(rows)


def build_names(ufb, out_dir: Path) -> int:
    rows = []
    for index, name in enumerate(ufb.get("object_names", [])):
        rows.append(("object", index, name))
    for index, name in enumerate(ufb.get("tile_names", [])):
        rows.append(("tile", index, name))
    for index, name in enumerate(ufb.get("region_names", [])):
        rows.append(("region", index, name))
    write_tsv(out_dir / "field_names.tsv", "# kind\tindex\tname", rows)
    return len(rows)


def build_ucb(ucb, ufb, out_dir: Path) -> tuple[int, int]:
    object_names = ufb.get("object_names", [])
    object_rows = []
    for entry in ucb.get("objects", []):
        index = int(entry["index"])
        name = object_names[index] if index < len(object_names) else ""
        object_rows.append(
            (
                index,
                name,
                entry["count"],
                entry["min_x"],
                entry["min_y"],
                entry["max_x"],
                entry["max_y"],
            )
        )
    write_tsv(
        out_dir / "field_objects.tsv",
        "# idx\tname\tcells\tmin_x\tmin_y\tmax_x\tmax_y",
        object_rows,
    )

    entry_rows = [
        (
            entry["index"],
            entry["count"],
            entry["min_x"],
            entry["min_y"],
            entry["max_x"],
            entry["max_y"],
        )
        for entry in ucb.get("entries", [])
    ]
    write_tsv(
        out_dir / "field_entries.tsv",
        "# idx\tcells\tmin_x\tmin_y\tmax_x\tmax_y",
        entry_rows,
    )
    write_tsv(
        out_dir / "field_ucb.tsv",
        "# width\theight\tcell_size\tcells\toccupied\tempty\tobjects\tentries\treserved_nonzero",
        [
            (
                ucb["width"],
                ucb["height"],
                ucb["cell_size"],
                ucb["cell_count"],
                ucb["occupied"],
                ucb["empty"],
                ucb["object_count"],
                ucb["entry_count"],
                ucb["reserved_nonzero"],
            )
        ],
    )
    return len(object_rows), len(entry_rows)


def otm_commands(paths):
    for start in range(0, len(paths), OTM_CHUNK):
        chunk = paths[start : start + OTM_CHUNK]
        yield [f"d {path} --json --stats" for path in chunk]


def build_otm(rosalinde: Rosalinde, out_dir: Path) -> int:
    paths = rosalinde.find("OTM", ".otm")
    documents = []
    for commands in otm_commands(paths):
        documents.extend(rosalinde.json_commands(commands))

    file_rows = []
    enum_counts = defaultdict(lambda: defaultdict(lambda: [0, set()]))
    for document in documents:
        path = document.get("path", "")
        file_rows.append(
            (
                path,
                document.get("mbs_name", ""),
                document.get("track_count", 0),
                document.get("event_count", 0),
                document.get("cue_count", 0),
                document.get("tracks_with_events", 0),
                document.get("empty_tracks", 0),
                document.get("cue_id_count", 0),
                document.get("cue_id_min", 0),
                document.get("cue_id_max", 0),
                document.get("frame_max", 0),
                document.get("value_max", 0),
            )
        )
        for field in ("event_kinds", "event_flags", "cue_types", "cue_flags", "cue_param0", "cue_param1"):
            for entry in document.get(field, []):
                value = int(entry["value"])
                count = int(entry["count"])
                slot = enum_counts[field][value]
                slot[0] += count
                slot[1].add(path)

    write_tsv(
        out_dir / "otm_files.tsv",
        "# path\tmbs\ttracks\tevents\tcues\ttracks_with_events\tempty_tracks\t"
        "cue_id_count\tcue_id_min\tcue_id_max\tframe_max\tvalue_max",
        file_rows,
    )

    enum_rows = []
    for field in ("event_kinds", "event_flags", "cue_types", "cue_flags", "cue_param0", "cue_param1"):
        for value in sorted(enum_counts[field]):
            count, paths_for_value = enum_counts[field][value]
            enum_rows.append((field, value, count, len(paths_for_value)))
    write_tsv(out_dir / "otm_enums.tsv", "# field\tvalue\tentries\tfiles", enum_rows)
    return len(file_rows)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpk", required=True, help="path to Unicorn.CPK")
    parser.add_argument("--rosalinde", default=str(DEFAULT_ROSALINDE))
    parser.add_argument("--out", default=str(DEFAULT_OUT))
    args = parser.parse_args()

    cpk = Path(args.cpk)
    binary = Path(args.rosalinde)
    out_dir = Path(args.out)
    if not cpk.exists():
        raise SystemExit(f"cpk_census: CPK not found: {cpk}")
    if not binary.exists():
        raise SystemExit(f"cpk_census: rosalinde not found: {binary}")

    rosalinde = Rosalinde(binary, cpk)
    ufb = rosalinde.json_commands(["d FieldBG/Field.ufb --json --stats"])[0]
    ucb = rosalinde.json_commands(["d FieldBG/Field.ucb --json --stats"])[0]

    assets = build_assets(ufb, out_dir)
    names = build_names(ufb, out_dir)
    objects, entries = build_ucb(ucb, ufb, out_dir)
    otm_files = build_otm(rosalinde, out_dir)

    print(f"wrote {out_dir / 'field_assets.tsv'} ({assets} rows)")
    print(f"wrote {out_dir / 'field_names.tsv'} ({names} rows)")
    print(f"wrote {out_dir / 'field_objects.tsv'} ({objects} rows)")
    print(f"wrote {out_dir / 'field_entries.tsv'} ({entries} rows)")
    print(f"wrote {out_dir / 'field_ucb.tsv'} (1 row)")
    print(f"wrote {out_dir / 'otm_files.tsv'} ({otm_files} rows)")
    print(f"wrote {out_dir / 'otm_enums.tsv'} (see file)")


if __name__ == "__main__":
    try:
        main()
    except CensusError as error:
        raise SystemExit(f"cpk_census: {error}")
