# visiontest — approximate opening-cinematic regression

This harness compares deterministic viewer captures with the reference video.
It does **not** require pixel identity. The reference is compressed video and
the viewer has its own camera/asset approximations, so the checks combine:

- structural similarity after downscale + blur;
- gradient correlation in the title region;
- saturation/brightness for white-vs-colored title phases;
- colour-histogram intersection for backgrounds;
- temporal alignment within a small frame window.

## Run

```bash
python3 visiontest/opening_diff.py \
  --reference opening_cinematic.mp4 \
  --out visiontest/out
```

Defaults:

```text
viewer      build/viewer/viewer
cpk         ~/Unicorn.CPK
script      Script/Dr_OW_0_Opening.asb
manifest    visiontest/manifest.json
out         visiontest/out
```

The viewer is launched with `SDL_VIDEODRIVER=offscreen` and
`MESA_LOADER_DRIVER_OVERRIDE=radeonsi` unless those variables are already set.

`visiontest/out/report.json` contains every metric and check; `index.html`
shows side-by-side and amplified difference images.

## Manifest

`manifest.json` defines:

- working resolution, blur sigma, and temporal search window;
- normalized regions (`full`, `title`, `background`);
- per-frame checks such as:

```json
{ "metric": "pair.title.gradient_correlation", "op": ">=", "value": 0.45 }
```

Metric names are `pair.<region>.<metric>`, `viewer.<region>.<metric>`, or
`reference.<region>.<metric>`.

`known_open: true` marks a frame whose current gap is understood and tracked
but does not fail the run. Remove it only after the frame improves and the
threshold has been reviewed.

## Notes

- Use the reference video directly; do not commit it.
- The out directory is generated and ignored.
- Temporal offsets are reported per frame. An offset beyond the configured
  search window is a timing regression, not a still-image mismatch.
- Add watermark/letterbox rectangles to `ignore_regions` as normalized
  `[x0, y0, x1, y1]` boxes before tightening thresholds.
