# Tools — Inspection, Tracing, and Capture

Permanent debug fixtures for correlating script time, asset state, and rendered output.

## `rosalinde` — interactive CPK shell

`rosalinde` opens the CPK once and accepts line commands on stdin; nothing is written
to disk unless you ask with `e`. See [`tools/README.md`](../tools/README.md) for the
full command reference and the line editor.

```bash
printf 'ls GUI\nd GUI/Logo.mbs --track 9 --seq 21 --limit 8\nq\n' \
  | ./build/rosalinde --cpk ~/Unicorn.CPK
```

`d` inspects an entry in memory (`acb`, `acf`, `nnd`, `ftx`, `mbs`, `otm`, `fms`).
For MBS the view is track → sequence → frame:

- `d X.mbs` — compact track list (sequence count, index range, `main`, `disabled`)
- `d X.mbs --track N` — sequences, with `total_frames`, `once`, `commit`, `track`, `loop`
- `d X.mbs --track N --seq M` — frames, with `duration`, flags, `s6`/`s7`,
  `s8_loop_target` and the interpolation tiers (`s5s3`, `rate`, `s7i`, `s6i`, `s8_render_value`)
- `--json` — machine-readable, filtered by `--track`/`--seq`
- `--section sN` — one raw section (`s0`…`s9`, `sa`, `sb`); `--full` for all

In the raw `s0` table the `r g b a` columns are the bytes of the first corner colour
printed high→low. `uint32_t` colours are RGBA in memory (red in the least-significant
byte), so the printed `a` column is red and `r` is alpha; `center`/`topleft` already
carry red in the low byte.

## `asbrun` — static ASB inspection

`asbrun` is the CPK-backed inspector for compiled `.asb` modules; see
[`tools/ASBRUN_README.md`](../tools/ASBRUN_README.md). It decodes bytecode,
lists functions and static call events, disassembles, scans the archive, and
generates runtime tables. It does not execute scenes.

```bash
printf 'open Dr_OW_0_Opening.asb\nsummary\nscene --index\nq\n' \
  | ./build/tools/asb/asbrun --cpk ~/Unicorn.CPK
```

Use `scene`, `timeline`, `scan`, and `disasm` for static inventory. The old
`play`/`trace`/`record`/`calls`/`state` commands are gone; they print a viewer
migration hint. `exec` and `eval` remain VM-only experiments without scene
modeling or MBS providers.

## `viewer` — live trace and multi-frame capture

The viewer is the canonical playback-dissection path: it owns live runtime state,
renderer/assets, and frame-accurate capture. Use `--headless` for execution
without a window or GL context.

```bash
./build/viewer/viewer \
  --cpk ~/Unicorn.CPK \
  --script Script/Dr_OW_0_Opening.asb \
  --capture-frames 0,60,420,480 \
  --capture-dir /tmp/opening-ours \
  --capture-prefix opening \
  --trace /tmp/opening-live.ndjson \
  --trace-categories vm,stage,camera,asset,sprite,draw
```

Behavior:

- `--capture-frames` steps to each requested scene frame and captures it in a single
  process, reusing the asset cache. Output names are `<prefix>-<frame:06>.png`.
- `--scene-frame` is still used for a single deterministic screenshot via `--screenshot`.
- `--headless` runs the same `PlaybackRuntime` without creating a window or GL
  context. `--run-to N` stops and exits at a scene frame; `--max-frames N` caps
  the run.
- `--state FILE|-` and `--state-json FILE|-` write a scene snapshot at the stop
  frame, including selected/resolved MBS tracks, run counts, drawability, and
  missing-asset reasons.
- `--trace` writes one JSON object per line. Each line has `frame`, `kind` and `event`.
  Categories are `vm`, `stage`, `camera`, `asset`, `sprite`, `draw` and `timeline`.
- `vm/call` events are complete native calls: call text, caller, handled status and
  return value. This is the canonical native-call trace; `asbrun` no longer runs
  scenes.
- `camera` events include both the script request (`pos`, `duration`, `delay`) and
  the per-frame evaluated camera state, including tween progress and ease ramps.
- `asset` events include cache hits, load mode, path and elapsed milliseconds.
- `sprite` events cover create/chara/animation/rebuild/track changes.
- `draw` events contain the final per-frame object order, sort keys, instance count and bounds;
  `draw/runs` carries each run's `s8`/`s6`/`s7`, offset, flags, and done/hidden state.
- `timeline/step` is a compact per-frame step record with entity count, stage and camera.
- `--trace-frames 820-880`, `--trace-object TEXT` (repeatable), and `--trace-limit N`
  narrow traces before writing.
- `--calls FILE` and `--calls-json FILE` write the native-call census: calls, stubs,
  first/last frame, and imported-but-uncalled natives.
- `--record FILE --record-every N` writes a runtime timeline. The normal viewer
  records renderer objects; `--headless` records the full scene state. A `.gz`
  suffix compresses the NDJSON, and `--record -` writes to stdout.
- `--probe DIR` writes `state-<frame>.json`, `draw-<frame>.ndjson` and
  `summary-<frame>.txt` at `--probe-frames` (default: capture frames or `--scene-frame`).
  In `--headless` mode it writes state/summary only. The draw dump is layer-level:
  MBS run/`s8`/`s6`/`s7`, UVs, effective colors, blend/fog, and per-layer skip reasons.
- `--only NAME`, `--hide TEXT` and `--keep-stage` isolate objects for texture/order
  experiments. `--clear-color #RRGGBB[AA]` sets a neutral background; `--data-dir`
  selects the runtime tables.
- `FTX_GPU_DUMP_DIR=DIR FTX_GPU_DUMP_MATCH=Opening010` dumps the uploaded texture
  array to a PNG for atlas/UV auditing. `VIEWER_FLIP_V` and `VIEWER_RUN_ORDER`
  (`reverse`/`track`) are experimental orientation/order probes, not rendering fixes.

## Suggested workflow

1. Use `viewer --headless --trace` (or `asbrun scene`/`timeline` for a static
   inventory) to identify the script frame and native arguments.
2. Use `rosalinde`'s `d ... --track/--seq` to inspect the referenced track and frames
   in memory (no extraction).
3. Use `viewer --headless --state` or `viewer --probe` at that frame to inspect the
   live entities, resolved MBS runs, draw order, and per-layer UV/color/skip decisions.
4. Use `viewer --capture-frames` to produce comparison frames.
5. Use `viewer --trace` with only the categories needed when a visual result is
   unexpected.
