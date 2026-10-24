#!/usr/bin/env python3
"""Build the committed runtime tables from raw engine records and the CPK.

Raw executable tables live in `viewer/data`. This script is run manually with
explicit `--cpk`; the generated tables are committed under
`viewer/data/generated`. No CPK path is assumed by the build or install.
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

DEFAULT_ASBRUN = Path("build/tools/asb/asbrun")
DEFAULT_ROSALINDE = Path("build/rosalinde")
DEFAULT_OUT = Path("viewer/data/generated")
CPK_CENSUS = Path(__file__).with_name("cpk_census.py")

EXPECTED = {
    "stages.tsv": 103,
    "facility_pos.tsv": 485,
    "drop_tables.tsv": 846,
    "classes.tsv": 83,
    "charanpc_assets.tsv": 319,
    "charaset.tsv": 1387,
    "chara_colors.tsv": 123,
    "bbs.tsv": 185,
    "uc_enums.tsv": 4099,
    "chara_table.tsv": 47,
    "battlebgs.tsv": 166,
    "field_assets.tsv": 39,
    "field_names.tsv": 520,
    "field_objects.tsv": 192,
    "field_entries.tsv": 1479,
    "field_ucb.tsv": 1,
    "otm_files.tsv": 1500,
    "otm_enums.tsv": 137,
}


class BuildError(RuntimeError):
    pass


def read_tsv(path):
    rows = []
    header = None
    with open(path, encoding="utf-8") as file:
        for line_number, line in enumerate(file, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            columns = line.split("\t")
            if header is None:
                header = columns
                continue
            if len(columns) != len(header):
                raise BuildError(
                    f"{path}:{line_number}: {len(columns)} columns, expected {len(header)}"
                )
            rows.append(dict(zip(header, columns)))
    if header is None:
        raise BuildError(f"missing header: {path}")
    return rows


def read_rows(path):
    rows = []
    with open(path, encoding="utf-8") as file:
        for line in file:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            rows.append(line.split("\t"))
    return rows


def write_tsv(path, header, rows):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as file:
        file.write(header + "\n")
        for row in rows:
            file.write("\t".join(str(value) for value in row) + "\n")


def run_export(asbrun, cpk, out_dir):
    commands = "\n".join(
        [
            f"export enums --out {out_dir / 'uc_enums.tsv'}",
            f"export chara --out {out_dir / 'chara_table.tsv'}",
            f"export bgs --out {out_dir / 'battlebgs.tsv'}",
            "q",
        ]
    ) + "\n"
    result = subprocess.run(
        [str(asbrun), "--cpk", str(cpk)],
        input=commands,
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        raise BuildError(f"asbrun export failed ({result.returncode}):\n{result.stderr}")
    for name in ("uc_enums.tsv", "chara_table.tsv", "battlebgs.tsv"):
        if not (out_dir / name).exists():
            raise BuildError(f"CPK export did not produce {name}")


def run_census(rosalinde, cpk, out_dir):
    result = subprocess.run(
        [
            sys.executable,
            str(CPK_CENSUS),
            "--cpk",
            str(cpk),
            "--rosalinde",
            str(rosalinde),
            "--out",
            str(out_dir),
        ],
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        raise BuildError(f"CPK census failed ({result.returncode}):\n{result.stderr}")
    print(result.stdout, end="", flush=True)


def enum_map(path):
    groups = {}
    for row in read_rows(path):
        if len(row) < 2:
            continue
        kind, name = row[0], row[1]
        comment = row[2] if len(row) > 2 else ""
        groups.setdefault(kind, []).append((name, comment))
    return groups


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpk", required=True, help="path to Unicorn.CPK (explicit runtime input)")
    parser.add_argument("--raw-dir", default="viewer/data")
    parser.add_argument("--out", default=str(DEFAULT_OUT))
    parser.add_argument("--asbrun", default=str(DEFAULT_ASBRUN))
    parser.add_argument("--rosalinde", default=str(DEFAULT_ROSALINDE))
    args = parser.parse_args()

    raw_dir = Path(args.raw_dir)
    out_dir = Path(args.out)
    cpk = Path(args.cpk)
    if not cpk.exists():
        raise BuildError(f"CPK not found: {cpk}")
    if not Path(args.asbrun).exists():
        raise BuildError(f"asbrun not found: {args.asbrun}")
    if not Path(args.rosalinde).exists():
        raise BuildError(f"rosalinde not found: {args.rosalinde}")
    out_dir.mkdir(parents=True, exist_ok=True)

    run_export(Path(args.asbrun), cpk, out_dir)
    run_census(Path(args.rosalinde), cpk, out_dir)
    enums = enum_map(out_dir / "uc_enums.tsv")
    class_names = [name for name, _ in enums.get("CLASSTYPE", [])]
    chara_names = [name for name, _ in enums.get("CHARASET", [])]

    stage = read_tsv(raw_dir / "stage_table.tsv")
    cue = read_tsv(raw_dir / "stage_cue_table.tsv")
    facility = read_tsv(raw_dir / "facility_table.tsv")
    drop = read_tsv(raw_dir / "drop_table.tsv")
    drop_entries = read_tsv(raw_dir / "drop_entry_table.tsv")
    classes = read_tsv(raw_dir / "class_table.tsv")
    npctype = read_tsv(raw_dir / "npctype_table.tsv")
    charaset = read_tsv(raw_dir / "charaset_table.tsv")
    bbs = read_tsv(raw_dir / "bbs_table.tsv")
    chara_color = read_tsv(raw_dir / "chara_color_table.tsv")
    chara_model = read_tsv(raw_dir / "chara_model_table.tsv")
    chara_model_mask = [int(row["mask"], 0) for row in chara_model]
    class_model = {}
    for gender, name in (("M", "m"), ("F", "f")):
        rows = read_tsv(raw_dir / f"class_model_{name}_table.tsv")
        class_model[gender] = {int(row["class_id"]): row for row in rows if row["class_id"]}

    def appearance_mask(class_id, gender, variant):
        record = class_model.get(gender, {}).get(class_id)
        model = 0
        if record:
            model = int(record.get(f"model{min(max(variant, 0), 9)}") or 0)
        return (chara_model_mask[model] if model < len(chara_model_mask) else 0) | 0xFE000000

    def color_preset(row):
        return int(row.get("w10") or 0, 0) & 0xFFFFFFFF

    write_tsv(
        out_dir / "chara_colors.tsv",
        "# idx\tcolor0\tcolor1\tcolor2\tcolor3 (generated from raw chara_color table)",
        [
            (
                int(row["idx"]),
                row["color0"],
                row["color1"],
                row["color2"],
                row["color3"],
            )
            for row in chara_color
        ],
    )

    cue_by_name = {row["name"]: int(row["bgm_cue_id"]) for row in cue}
    write_tsv(
        out_dir / "stages.tsv",
        "# stage\tbase_lv\tbgm_cue_id\tcountry (generated from raw stage tables)",
        [
            (row["name"], int(row["base_lv"]), cue_by_name.get(row["name"], -1), int(row["country"]))
            for row in stage
        ],
    )

    facility_names = enums.get("ST_FACILITYID", [])
    write_tsv(
        out_dir / "facility_pos.tsv",
        "# key\tlabel\tx\ty (generated from raw facility table)",
        [
            (
                facility_names[i][0] if i < len(facility_names) else f"FAC_{i}",
                row["label"],
                row["x"],
                row["y"],
            )
            for i, row in enumerate(facility)
        ],
    )

    item_names = enums.get("ITEMID", [])
    drop_names = enums.get("DROPTABLE", [])
    drop_rows = []
    for i, row in enumerate(drop):
        start = int(row["start"])
        count = int(row["count"])
        drop_name = drop_names[i][0] if i < len(drop_names) else f"DROP_{i}"
        for j in range(max(0, count)):
            entry = drop_entries[start + j]
            item_id = int(entry["item_id"])
            item = item_names[item_id][0] if 0 <= item_id < len(item_names) else f"ITEM_{item_id}"
            drop_rows.append((drop_name, item, int(entry["weight"]), int(entry["count"]), int(entry["add_index"])))
    write_tsv(
        out_dir / "drop_tables.tsv",
        "# drop_table\titem\tweight\tcount\tadd_index (generated from raw drop tables)",
        drop_rows,
    )

    class_rows = []
    for i, row in enumerate(classes):
        name = class_names[i] if i < len(class_names) else f"CLASS_{i}"
        label = enums.get("CLASSTYPE", [])[i][1] if i < len(enums.get("CLASSTYPE", [])) else ""
        for suffix, mbs_column, ftx_column in (("M", "mbs_m", "ftx_m"), ("F", "mbs_f", "ftx_f")):
            mbs = row[mbs_column]
            if not mbs:
                continue
            if mbs.startswith("Chara/"):
                mbs = mbs[6:]
            ftx = row[ftx_column]
            if ftx.startswith("Chara/"):
                ftx = ftx[6:]
            class_rows.append((f"{name}_{suffix}", label, mbs, ftx))
    write_tsv(out_dir / "classes.tsv", "# key\tjp_name\tmbs\tftx (generated from raw class table)", class_rows)

    npctype_names = enums.get("NPCTYPE", [])
    npctype_rows = []
    for i, row in enumerate(npctype):
        if not row["mbs"]:
            continue
        npctype_rows.append(
            (
                i,
                npctype_names[i][0] if i < len(npctype_names) else f"NPCTYPE_{i}",
                npctype_names[i][1] if i < len(npctype_names) else row["label"],
                row["mbs"],
                row["ftx"],
            )
        )
    write_tsv(out_dir / "charanpc_assets.tsv", "# idx\tname\tjp\tmbs\tftx (generated from raw NPCTYPE table)", npctype_rows)

    charaset_rows = []
    for i, row in enumerate(charaset):
        if i == 0:
            continue
        class_id = int(row["class_id"])
        if class_id >= len(class_names):
            continue
        gender = "M" if int(row["gender_id"]) == 1 else "F" if int(row["gender_id"]) == 2 else "?"
        charaset_rows.append(
            (
                i,
                chara_names[i] if i < len(chara_names) else f"CHARASET_{i}",
                class_names[class_id],
                gender,
                int(row["variant"]),
                row["chara_img"],
                row["label"],
                f"0x{appearance_mask(class_id, gender, int(row['variant'])):08x}",
                color_preset(row),
            )
        )
    write_tsv(
        out_dir / "charaset.tsv",
        "# idx\tname\tclass\tgender\tvariant\tcharaImg\tlabel\tappearance_mask\tcolor_preset (generated from raw CharaSet table)",
        charaset_rows,
    )

    write_tsv(
        out_dir / "bbs.tsv",
        "# stage_type\tpath\tlabel (generated from raw BBS table)",
        [(int(row["idx"]), row["path"], row["label"]) for row in bbs if row["path"]],
    )

    country_link = out_dir / "country_regions.tsv"
    if country_link.exists() or country_link.is_symlink():
        country_link.unlink()
    country_link.symlink_to(os.path.relpath(raw_dir / "country_regions.tsv", out_dir))

    counts = {}
    for name in EXPECTED:
        counts[name] = sum(1 for _ in open(out_dir / name, encoding="utf-8")) - 1
    expected = dict(EXPECTED)
    for name, wanted in expected.items():
        actual = counts[name]
        if actual != wanted:
            raise BuildError(f"{name}: expected {wanted} rows, got {actual}")
    for name, count in sorted(counts.items()):
        print(f"wrote {out_dir / name} ({count} rows)", flush=True)


if __name__ == "__main__":
    try:
        main()
    except BuildError as error:
        raise SystemExit(f"build_viewer_data: {error}")
