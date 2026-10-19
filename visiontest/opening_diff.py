#!/usr/bin/env python3
"""Perceptual opening-cinematic regression against the reference video.

The reference is compressed video, not a render target.  This harness therefore
compares blurred, downscaled frames with structural, colour, and presence
metrics instead of requiring pixel identity.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass
class Image:
    width: int
    height: int
    rgb: bytes
    gray: list[int]


def read_ppm(path: Path) -> Image:
    data = path.read_bytes()
    if not data.startswith(b"P6"):
        raise ValueError(f"{path}: expected binary PPM (P6)")
    pos = 2
    tokens: list[bytes] = []
    while len(tokens) < 3:
        while pos < len(data) and data[pos] in b" \t\r\n":
            pos += 1
        if pos < len(data) and data[pos] == ord("#"):
            while pos < len(data) and data[pos] not in b"\r\n":
                pos += 1
            continue
        start = pos
        while pos < len(data) and data[pos] not in b" \t\r\n":
            pos += 1
        tokens.append(data[start:pos])
    width, height, maxval = (int(token) for token in tokens)
    if maxval != 255:
        raise ValueError(f"{path}: only 8-bit PPM is supported")
    pos += 1
    rgb = data[pos : pos + width * height * 3]
    if len(rgb) != width * height * 3:
        raise ValueError(f"{path}: truncated PPM payload")
    gray = [0] * (width * height)
    for i in range(width * height):
        r = rgb[i * 3 + 0]
        g = rgb[i * 3 + 1]
        b = rgb[i * 3 + 2]
        gray[i] = (r * 299 + g * 587 + b * 114) // 1000
    return Image(width, height, rgb, gray)


def rect_for(region: list[float], width: int, height: int) -> tuple[int, int, int, int]:
    x0 = max(0, min(width, int(round(region[0] * width))))
    y0 = max(0, min(height, int(round(region[1] * height))))
    x1 = max(x0, min(width, int(round(region[2] * width))))
    y1 = max(y0, min(height, int(round(region[3] * height))))
    return x0, y0, x1, y1


def percentile_from_hist(hist: list[int], fraction: float) -> float:
    total = sum(hist)
    if total == 0:
        return 0.0
    target = total * fraction
    running = 0
    for value, count in enumerate(hist):
        running += count
        if running >= target:
            return float(value)
    return float(len(hist) - 1)


def ssim_from_moments(
    n: int, sum_a: float, sum_b: float, sum_aa: float, sum_bb: float, sum_ab: float
) -> float:
    if n == 0:
        return 1.0
    mean_a = sum_a / n
    mean_b = sum_b / n
    var_a = max(0.0, sum_aa / n - mean_a * mean_a)
    var_b = max(0.0, sum_bb / n - mean_b * mean_b)
    cov = sum_ab / n - mean_a * mean_b
    c1 = 0.01 * 0.01
    c2 = 0.03 * 0.03
    return float(((2.0 * mean_a * mean_b + c1) * (2.0 * cov + c2)) / ((mean_a * mean_a + mean_b * mean_b + c1) * (var_a + var_b + c2)))


def rgb_bin(r: int, g: int, b: int) -> int:
    return ((r >> 5) << 6) | ((g >> 5) << 3) | (b >> 5)


def region_pair_metrics(
    a: Image, b: Image, rect: tuple[int, int, int, int], ignored: list[tuple[int, int, int, int]] | None = None
) -> dict[str, float]:
    x0, y0, x1, y1 = rect
    ignored = ignored or []
    w = x1 - x0
    h = y1 - y0
    if w <= 2 or h <= 2:
        return {
            "ssim": 1.0,
            "ssim_blocks": 1.0,
            "mae": 0.0,
            "rmse": 0.0,
            "p95_abs": 0.0,
            "gradient_correlation": 1.0,
            "hist_intersection": 1.0,
            "viewer_saturation_mean": 0.0,
            "reference_saturation_mean": 0.0,
            "viewer_luma_mean": 0.0,
            "reference_luma_mean": 0.0,
            "viewer_edge_density": 0.0,
            "reference_edge_density": 0.0,
        }

    diff_hist = [0] * 256
    hist_a = [0] * 512
    hist_b = [0] * 512
    sum_d = 0
    sum_d2 = 0
    sum_a = 0.0
    sum_b = 0.0
    sum_aa = 0.0
    sum_bb = 0.0
    sum_ab = 0.0
    sat_a = 0.0
    sat_b = 0.0
    edge_a = 0
    edge_b = 0
    grad_n = 0
    gx_a_sum = gy_a_sum = gx_b_sum = gy_b_sum = 0.0
    gx_a_sq = gy_a_sq = gx_b_sq = gy_b_sq = 0.0
    gx_ab = gy_ab = 0.0
    block = 32
    block_ssim_sum = 0.0
    block_count = 0
    block_stats: dict[tuple[int, int], list[float]] = {}

    n = 0
    for y in range(y0, y1):
        row = y * a.width
        for x in range(x0, x1):
            if ignored and any(ix0 <= x < ix1 and iy0 <= y < iy1 for ix0, iy0, ix1, iy1 in ignored):
                continue
            n += 1
            ia = row + x
            ga = a.gray[ia]
            gb = b.gray[ia]
            d = abs(ga - gb)
            diff_hist[d] += 1
            sum_d += d
            sum_d2 += d * d
            sum_a += ga
            sum_b += gb
            sum_aa += ga * ga
            sum_bb += gb * gb
            sum_ab += ga * gb

            ar = a.rgb[ia * 3 + 0]
            ag = a.rgb[ia * 3 + 1]
            ab = a.rgb[ia * 3 + 2]
            br = b.rgb[ia * 3 + 0]
            bg = b.rgb[ia * 3 + 1]
            bb = b.rgb[ia * 3 + 2]
            hist_a[rgb_bin(ar, ag, ab)] += 1
            hist_b[rgb_bin(br, bg, bb)] += 1
            sat_a += (max(ar, ag, ab) - min(ar, ag, ab)) / 255.0
            sat_b += (max(br, bg, bb) - min(br, bg, bb)) / 255.0

            bx = (x - x0) // block
            by = (y - y0) // block
            key = (bx, by)
            stats = block_stats.get(key)
            if stats is None:
                stats = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
                block_stats[key] = stats
            stats[0] += 1.0
            stats[1] += ga
            stats[2] += gb
            stats[3] += ga * ga
            stats[4] += gb * gb
            stats[5] += ga * gb

            if x > x0 and x + 1 < x1 and y > y0 and y + 1 < y1:
                iax = ia + 1
                iay = ia + a.width
                iaxm = ia - 1
                iaym = ia - a.width
                ibx = ia + 1
                iby = ia + a.width
                ibxm = ia - 1
                ibym = ia - a.width
                gxa = (a.gray[iax] - a.gray[iaxm]) * 0.5
                gya = (a.gray[iay] - a.gray[iaym]) * 0.5
                gxb = (b.gray[ibx] - b.gray[ibxm]) * 0.5
                gyb = (b.gray[iby] - b.gray[ibym]) * 0.5
                ma = abs(gxa) + abs(gya)
                mb = abs(gxb) + abs(gyb)
                if ma > 16.0:
                    edge_a += 1
                if mb > 16.0:
                    edge_b += 1
                gx_a_sum += gxa
                gy_a_sum += gya
                gx_b_sum += gxb
                gy_b_sum += gyb
                gx_a_sq += gxa * gxa
                gy_a_sq += gya * gya
                gx_b_sq += gxb * gxb
                gy_b_sq += gyb * gyb
                gx_ab += gxa * gxb
                gy_ab += gya * gyb
                grad_n += 1

    n = max(1, n)
    mean_a = sum_a / n
    mean_b = sum_b / n
    var_a = max(0.0, sum_aa / n - mean_a * mean_a)
    var_b = max(0.0, sum_bb / n - mean_b * mean_b)
    cov = sum_ab / n - mean_a * mean_b
    c1 = 0.01 * 0.01
    c2 = 0.03 * 0.03
    ssim = ((2.0 * mean_a * mean_b + c1) * (2.0 * cov + c2)) / (
        (mean_a * mean_a + mean_b * mean_b + c1) * (var_a + var_b + c2)
    )

    for stats in block_stats.values():
        bn, sa, sb, saa, sbb, sab = stats
        if bn == 0:
            continue
        ma = sa / bn
        mb = sb / bn
        va = max(0.0, saa / bn - ma * ma)
        vb = max(0.0, sbb / bn - mb * mb)
        cv = sab / bn - ma * mb
        block_ssim_sum += ((2.0 * ma * mb + c1) * (2.0 * cv + c2)) / (
            (ma * ma + mb * mb + c1) * (va + vb + c2)
        )
        block_count += 1

    grad_corr = 0.0
    if grad_n > 0:
        gxa_mean = gx_a_sum / grad_n
        gya_mean = gy_a_sum / grad_n
        gxb_mean = gx_b_sum / grad_n
        gyb_mean = gy_b_sum / grad_n
        cov_g = (gx_ab / grad_n - gxa_mean * gxb_mean) + (gy_ab / grad_n - gya_mean * gyb_mean)
        va_g = (gx_a_sq / grad_n - gxa_mean * gxa_mean) + (gy_a_sq / grad_n - gya_mean * gya_mean)
        vb_g = (gx_b_sq / grad_n - gxb_mean * gxb_mean) + (gy_b_sq / grad_n - gyb_mean * gyb_mean)
        denom = math.sqrt(max(1e-12, va_g * vb_g))
        grad_corr = cov_g / denom

    hist_total_a = sum(hist_a)
    hist_total_b = sum(hist_b)
    hist_intersection = 0.0
    if hist_total_a and hist_total_b:
        hist_intersection = sum(min(hist_a[i], hist_b[i]) for i in range(512)) / max(hist_total_a, hist_total_b)

    return {
        "ssim": float(ssim),
        "ssim_blocks": float(block_ssim_sum / block_count) if block_count else 1.0,
        "mae": sum_d / n,
        "rmse": math.sqrt(sum_d2 / n),
        "p95_abs": percentile_from_hist(diff_hist, 0.95),
        "gradient_correlation": float(grad_corr),
        "hist_intersection": float(hist_intersection),
        "viewer_saturation_mean": sat_b / n,
        "reference_saturation_mean": sat_a / n,
        "viewer_luma_mean": mean_b,
        "reference_luma_mean": mean_a,
        "viewer_edge_density": edge_b / max(1, grad_n),
        "reference_edge_density": edge_a / max(1, grad_n),
    }


def alignment_score(a: Image, b: Image) -> float:
    """Cheap full-frame correlation for temporal candidate selection."""
    step = 4
    n = 0
    sa = sb = saa = sbb = sab = 0.0
    for y in range(0, a.height, step):
        row = y * a.width
        for x in range(0, a.width, step):
            ia = row + x
            ga = a.gray[ia]
            gb = b.gray[ia]
            n += 1
            sa += ga
            sb += gb
            saa += ga * ga
            sbb += gb * gb
            sab += ga * gb
    if n == 0:
        return 0.0
    ma = sa / n
    mb = sb / n
    va = saa / n - ma * ma
    vb = sbb / n - mb * mb
    cov = sab / n - ma * mb
    return cov / math.sqrt(max(1e-12, va * vb))


def metric_value(metrics: dict[str, dict[str, float]], key: str) -> float | None:
    parts = key.split(".")
    if len(parts) != 3:
        return None
    group, region, metric = parts
    if group == "pair":
        return metrics.get(region, {}).get(metric)
    if group == "viewer":
        value = metrics.get(region, {}).get("viewer_" + metric)
        return value
    if group == "reference":
        value = metrics.get(region, {}).get("reference_" + metric)
        return value
    return None


def evaluate_check(metrics: dict[str, dict[str, float]], check: dict[str, Any]) -> dict[str, Any]:
    metric = str(check["metric"])
    value = metric_value(metrics, metric)
    op = str(check.get("op", ">="))
    target = float(check["value"])
    if value is None:
        return {"metric": metric, "op": op, "value": target, "actual": None, "passed": False}
    if op == ">=":
        passed = value >= target
    elif op == ">":
        passed = value > target
    elif op == "<=":
        passed = value <= target
    elif op == "<":
        passed = value < target
    elif op == "abs<=":
        passed = abs(value) <= target
    else:
        raise ValueError(f"unknown check op: {op}")
    return {"metric": metric, "op": op, "value": target, "actual": value, "passed": passed}


def run_ffmpeg(args: list[str]) -> None:
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"ffmpeg failed: {' '.join(args)}\n{result.stderr}")


def extract_reference(
    reference: Path, frames: list[int], outdir: Path, width: int, height: int, blur: float
) -> dict[int, Path]:
    selected = sorted(set(frames))
    expr = "+".join(f"eq(n\\,{frame})" for frame in selected)
    vf = f"select='{expr}',scale={width}:{height}:flags=lanczos"
    if blur > 0:
        vf += f",gblur=sigma={blur:g}"
    outdir.mkdir(parents=True, exist_ok=True)
    template = outdir / "ref-%06d.ppm"
    run_ffmpeg(
        [
            "ffmpeg",
            "-v",
            "error",
            "-y",
            "-i",
            str(reference),
            "-vf",
            vf,
            "-fps_mode",
            "passthrough",
            "-start_number",
            "0",
            "-f",
            "image2",
            str(template),
        ]
    )
    paths = sorted(outdir.glob("ref-*.ppm"))
    if len(paths) < len(selected):
        raise RuntimeError(f"ffmpeg produced {len(paths)} reference frames, expected {len(selected)}")
    return {frame: paths[i] for i, frame in enumerate(selected)}


def capture_viewer(
    viewer: Path, cpk: Path, script: str, frames: list[int], outdir: Path, width: int, height: int, blur: float
) -> dict[int, Path]:
    outdir.mkdir(parents=True, exist_ok=True)
    rawdir = outdir / "raw"
    rawdir.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.setdefault("SDL_VIDEODRIVER", "offscreen")
    env.setdefault("MESA_LOADER_DRIVER_OVERRIDE", "radeonsi")
    result = subprocess.run(
        [
            str(viewer),
            "--cpk",
            str(cpk),
            "--script",
            script,
            "--capture-frames",
            ",".join(str(frame) for frame in frames),
            "--capture-dir",
            str(rawdir),
        ],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"viewer failed:\n{result.stdout}\n{result.stderr}")

    paths: dict[int, Path] = {}
    for frame in frames:
        src = rawdir / f"frame-{frame:06d}.png"
        if not src.exists():
            raise RuntimeError(f"viewer did not write {src}")
        dst = outdir / f"view-{frame:06d}.ppm"
        vf = f"scale={width}:{height}:flags=lanczos"
        if blur > 0:
            vf += f",gblur=sigma={blur:g}"
        run_ffmpeg(["ffmpeg", "-v", "error", "-y", "-i", str(src), "-vf", vf, "-frames:v", "1", str(dst)])
        paths[frame] = dst
    return paths


def write_visuals(reference: Path, viewer: Path, outdir: Path, frame: int) -> dict[str, str]:
    outdir.mkdir(parents=True, exist_ok=True)
    side = outdir / f"side-{frame:06d}.png"
    diff = outdir / f"diff-{frame:06d}.png"
    run_ffmpeg(
        [
            "ffmpeg",
            "-v",
            "error",
            "-y",
            "-i",
            str(reference),
            "-i",
            str(viewer),
            "-filter_complex",
            "hstack=inputs=2",
            str(side),
        ]
    )
    run_ffmpeg(
        [
            "ffmpeg",
            "-v",
            "error",
            "-y",
            "-i",
            str(reference),
            "-i",
            str(viewer),
            "-filter_complex",
            "[0:v][1:v]blend=all_mode=difference,eq=contrast=4",
            str(diff),
        ]
    )
    return {"side": side.name, "diff": diff.name}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--viewer", default="build/viewer/viewer")
    parser.add_argument("--cpk", default=os.path.expanduser("~/Unicorn.CPK"))
    parser.add_argument("--script", default="Script/Dr_OW_0_Opening.asb")
    parser.add_argument("--reference", default="opening_cinematic.mp4")
    parser.add_argument("--manifest", default="visiontest/manifest.json")
    parser.add_argument("--out", default="visiontest/out")
    parser.add_argument("--strict", action="store_true", help="Fail known-open frames too")
    args = parser.parse_args()

    manifest_path = Path(args.manifest)
    manifest = json.loads(manifest_path.read_text())
    reference = Path(args.reference)
    if not reference.exists():
        raise SystemExit(f"reference video not found: {reference}")

    config = manifest.get("config", {})
    width = int(config.get("width", 960))
    height = int(config.get("height", 540))
    blur = float(config.get("blur_sigma", 1.0))
    align_window = int(config.get("align_window", 2))
    regions = {name: [float(v) for v in box] for name, box in manifest["regions"].items()}
    ignored = [rect_for([float(v) for v in box], width, height) for box in manifest.get("ignore_regions", [])]
    frames = [int(item["frame"]) for item in manifest["frames"]]

    outdir = Path(args.out)
    if outdir.exists():
        shutil.rmtree(outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    all_reference_frames = sorted({frame + offset for frame in frames for offset in range(-align_window, align_window + 1) if frame + offset >= 0})
    ref_paths = extract_reference(reference, all_reference_frames, outdir / "reference", width, height, blur)
    view_paths = capture_viewer(
        Path(args.viewer), Path(args.cpk), args.script, frames, outdir / "viewer", width, height, blur
    )

    refs = {frame: read_ppm(ref_paths[frame]) for frame in all_reference_frames}
    views = {frame: read_ppm(view_paths[frame]) for frame in frames}

    reports: list[dict[str, Any]] = []
    hard_failures = 0
    for item in manifest["frames"]:
        frame = int(item["frame"])
        candidates = [frame + offset for offset in range(-align_window, align_window + 1) if frame + offset in refs]
        view = views[frame]
        best_frame = min(candidates, key=lambda candidate: (-alignment_score(refs[candidate], view), abs(candidate - frame)))
        best_ref = refs[best_frame]
        metrics: dict[str, dict[str, float]] = {}
        for name, box in regions.items():
            rect = rect_for(box, width, height)
            metrics[name] = region_pair_metrics(best_ref, view, rect, ignored)

        checks = [evaluate_check(metrics, check) for check in item.get("checks", [])]
        failed = [check for check in checks if not check["passed"]]
        known_open = bool(item.get("known_open", False))
        if known_open:
            status = "xfail" if failed else "xpass"
            if failed and args.strict:
                hard_failures += 1
        else:
            status = "fail" if failed else "pass"
            if failed:
                hard_failures += 1

        visuals = write_visuals(ref_paths[best_frame], view_paths[frame], outdir / "visuals", frame)
        reports.append(
            {
                "frame": frame,
                "name": item.get("name", ""),
                "known_open": known_open,
                "status": status,
                "reference_frame": best_frame,
                "offset": best_frame - frame,
                "metrics": metrics,
                "checks": checks,
                "failures": failed,
                "visuals": visuals,
            }
        )

    report = {
        "manifest": str(manifest_path),
        "reference": str(reference),
        "frames": reports,
        "hard_failures": hard_failures,
    }
    (outdir / "report.json").write_text(json.dumps(report, indent=2))
    write_html(outdir, reports)

    for item in reports:
        failures = ", ".join(check["metric"] for check in item["failures"])
        print(f"{item['status']:>5} {item['frame']:4d} {item['name']:<20} offset={item['offset']:+d} {failures}")
    print(f"report: {outdir / 'report.json'}")
    print(f"html:   {outdir / 'index.html'}")
    if hard_failures:
        print(f"FAILED: {hard_failures} frame(s)")
        return 1
    return 0


def write_html(outdir: Path, reports: list[dict[str, Any]]) -> None:
    rows: list[str] = []
    for item in reports:
        color = {"pass": "#1b5e20", "fail": "#b71c1c", "xfail": "#8d6e63", "xpass": "#1565c0"}.get(item["status"], "#333")
        checks = "<br>".join(
            f"{check['metric']} {check['op']} {check['value']:.4g} = "
            f"{'n/a' if check['actual'] is None else format(check['actual'], '.4g')} "
            f"{'✓' if check['passed'] else '✗'}"
            for check in item["checks"]
        )
        visuals = item["visuals"]
        rows.append(
            f"<tr><td>{item['frame']}</td><td>{item['name']}</td>"
            f"<td style='color:{color};font-weight:bold'>{item['status']}</td>"
            f"<td>{item['offset']:+d}</td><td>{checks}</td>"
            f"<td><a href='visuals/{visuals['side']}'><img src='visuals/{visuals['side']}' width='480'></a>"
            f"<br><a href='visuals/{visuals['diff']}'>diff</a></td></tr>"
        )
    html = f"""<!doctype html>
<html><head><meta charset="utf-8"><title>Opening regression</title>
<style>body{{font-family:system-ui,sans-serif;margin:2rem}}table{{border-collapse:collapse}}
td,th{{border:1px solid #ccc;padding:.4rem;vertical-align:top}}th{{background:#eee;text-align:left}}
</style></head><body>
<h1>Opening cinematic regression</h1>
<p>Reference frame is the best temporal match; side-by-side is reference | viewer.</p>
<table><tr><th>frame</th><th>name</th><th>status</th><th>offset</th><th>checks</th><th>visual</th></tr>
{''.join(rows)}
</table></body></html>"""
    (outdir / "index.html").write_text(html)


if __name__ == "__main__":
    sys.exit(main())
