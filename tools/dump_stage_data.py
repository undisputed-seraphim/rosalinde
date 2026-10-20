#!/usr/bin/env python3
"""Dump the UcStStage stage-data record used by DrCreateStage.

The stage-data database is the buffer returned by stage_get_data().  The
per-stage record passed to UcStStage_ctor starts at 0x2f0b70 in that buffer.
This tool is intentionally a raw inspection tool: it does not depend on the
CPK and it does not try to reconstruct the viewer's StageComposition.

    python3 tools/dump_stage_data.py UCGLOBALDATA.DAT
    python3 tools/dump_stage_data.py UCGLOBALDATA.DAT --base 0x2f0b70

The record layout was recovered from UcStStage_copyStageData,
stage_create_from_data, stage_spawn_scene_objects,
stage_validate_object_state, stage_copy_layer_table and stage_bind_layer_slot.
"""

import argparse
import struct
import sys

DEFAULT_BASE = 0x2F0B70
SCENE = 0x1330
SCENE_STATE_SIZE = 0x80000
SCENE_STATE_HASH = 0x80138
SCENE_SPAWNS = 0x8013C
SCENE_SPAWN_COUNT = 0x8413C
SCENE_LAYERS = 0x84000
LAYER_TABLE_SIZE = 0x138
LAYER_HEADER_SIZE = 0x48
LAYER_SLOT_SIZE = 0xC
LAYER_SLOTS = 20

ARRAYS = (
    (0x8627C, 0x10C, 0, "objects"),
    (0x883FC, 4, 1, "object_ids"),
    (0x8841C, 0x40, 2, "transform_2"),
    (0x88C1C, 0x40, 3, "transform_3"),
    (0x8941C, 0x40, 5, "transform_5"),
    (0x8A41C, 0x40, 7, "transform_7"),
    (0x8AC28, 0x58, 8, "params_8"),
    (0x8B720, 0x80, 9, "params_9"),
)
COUNTS = 0x8C550

SPAWN_TYPES = {
    0: "object_table",
    1: "fixed_1",
    2: "placed_2",
    3: "placed_3",
    4: "placed_4",
    5: "fixed_5",
    6: "fixed_6",
    7: "fixed_7",
    8: "placed_8",
    9: "placed_9",
    10: "cloud",
    11: "kind_id",
    12: "fixed_12",
    13: "fixed_13",
}


def read_cstr(buf, off, limit=None):
    if off < 0 or off >= len(buf):
        return ""
    end = buf.find(b"\0", off, len(buf) if limit is None else min(len(buf), off + limit))
    if end < 0:
        end = len(buf)
    return buf[off:end].decode("utf-8", "replace")


def u32(buf, off):
    if off < 0 or off + 4 > len(buf):
        raise ValueError(f"u32 out of bounds: {off:#x}")
    return struct.unpack_from("<I", buf, off)[0]


def u64(buf, off):
    if off < 0 or off + 8 > len(buf):
        raise ValueError(f"u64 out of bounds: {off:#x}")
    return struct.unpack_from("<Q", buf, off)[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("path", help="stage-data database (UCGLOBALDATA.DAT or a raw dump)")
    ap.add_argument("--base", type=lambda s: int(s, 0), default=DEFAULT_BASE,
                    help="offset of the per-stage record (default: 0x2f0b70)")
    ap.add_argument("--no-spawns", action="store_true", help="omit the 512 spawn slots")
    args = ap.parse_args()

    buf = open(args.path, "rb").read()
    base = args.base
    scene = base + SCENE
    if scene + SCENE_SPAWN_COUNT + 4 > len(buf):
        raise SystemExit(f"{args.path}: stage-data record at {base:#x} is out of bounds")

    print(f"file={args.path} size=0x{len(buf):x}")
    print(f"stage_record=0x{base:x} scene=0x{scene:x}")

    spawn_count = u32(buf, scene + SCENE_SPAWN_COUNT)
    print(f"spawn_count={spawn_count}")

    if not args.no_spawns:
        for index in range(min(spawn_count, 512)):
            rec = scene + SCENE_SPAWNS + index * 0x20
            values = struct.unpack_from("<8I", buf, rec)
            typ, table, order, object_id, arg0, arg1, arg2, arg3 = values
            print(
                f"spawn[{index}] type={typ}({SPAWN_TYPES.get(typ, '?')}) "
                f"table={table} order={order} id={object_id} "
                f"args=({arg0},{arg1},{arg2},{arg3})"
            )

    print("layer_table:")
    for index in range(LAYER_SLOTS):
        slot = scene + SCENE_LAYERS + LAYER_HEADER_SIZE + index * LAYER_SLOT_SIZE
        key = u64(buf, slot)
        flags = buf[slot + 8]
        if key != 0 or flags:
            print(f"  [{index:02d}] key=0x{key:016x} flags=0x{flags:02x}")

    counts = [buf[base + COUNTS + i] if base + COUNTS + i < len(buf) else 0 for i in range(10)]
    print("counts=" + " ".join(f"{i}:{counts[i]}" for i in range(10)))

    for relative, stride, count_index, name in ARRAYS:
        offset = base + relative
        count = counts[count_index]
        print(f"{name}: offset=0x{offset:x} stride=0x{stride:x} count={count}")
        end = offset + count * stride
        if end > len(buf):
            print(f"  truncated: end=0x{end:x}")
            count = max(0, (len(buf) - offset) // stride)
        for index in range(count):
            rec = offset + index * stride
            if stride == 0x10C:
                object_id = u32(buf, rec)
                object_name = read_cstr(buf, rec + 4, 0x40)
                model = read_cstr(buf, rec + 0x44, 0x40)
                state = u32(buf, rec + 0x104)
                print(
                    f"  [{index:03d}] id={object_id} state=0x{state:08x} "
                    f"name={object_name!r} model={model!r}"
                )
            else:
                print(f"  [{index:03d}] " + buf[rec:rec + min(stride, 0x20)].hex())


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
