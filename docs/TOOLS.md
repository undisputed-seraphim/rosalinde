# Tools — Inspection, Tracing, and Capture

Permanent debug fixtures for correlating script time, asset state, and rendered output.

## `rosalinde` — MBS metadata

`tools/extractor.cpp` already handles CPK listing/extraction, UTF column dumps and raw
MBS CSV dumps. `--mbs-info` adds interpreted track/run metadata as JSON:

```bash
./build/rosalinde --cpk ~/Unicorn.CPK --mbs-info GUI/Logo.mbs
./build/rosalinde --cpk ~/Unicorn.CPK --mbs-info GUI/Logo.mbs --mbs-track 9
./build/rosalinde --cpk ~/Unicorn.CPK --mbs-info GUI/Logo.mbs --mbs-track 9 --mbs-run 21
```

The output includes:

- track index/name, bounds, `sa_set_id`, `sa_set_no`, `sa_set_main`, disabled state
- per-run `s8` range/count, start offset, tick sum, intro/loop point, single-pass flag,
  track id and computed duration
- with `--mbs-run`, the full `s8` frame list (frame count, flags, loop target)

Raw section dumps remain available with `--mbs-dump`.

## `asb_trace` — offline script event stream

`tools/asb/trace.cpp` runs a module without opening a window and emits structured
NDJSON events:

```bash
./build/tools/asb/asb_trace \
  --cpk ~/Unicorn.CPK \
  --asb Script/Dr_OW_0_Opening.asb \
  --frames 2600 \
  --categories stage,camera,sprite \
  --out /tmp/opening-events.ndjson
```

Omit `--out` to write to stdout. `--categories all` is the default. VM events include
the native call name; semantic events include arguments and resolved values (for
example camera position/duration/mode, stage type, sprite create/animation). The
offline tool has no renderer, so `asset` and `draw` events are viewer-only.

## `viewer` — live trace and multi-frame capture

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
- `--trace` writes one JSON object per line. Each line has `frame`, `kind` and `event`.
  Categories are `vm`, `stage`, `camera`, `asset`, `sprite` and `draw`.
- `camera` events include both the script request (`pos`, `duration`, `delay`) and
  the per-frame evaluated camera state, including tween progress and ease ramps.
- `asset` events include cache hits, load mode, path and elapsed milliseconds.
- `sprite` events cover create/chara/animation/rebuild/track changes.
- `draw` events contain the final per-frame object order, instance count and bounds.

## Suggested workflow

1. Use `asb_trace` to identify the exact script frame and native arguments.
2. Use `rosalinde --mbs-info` to inspect the referenced track/runs.
3. Use `viewer --capture-frames` to produce comparison frames.
4. Use `viewer --trace` with only the categories needed when a visual result is
   unexpected.
