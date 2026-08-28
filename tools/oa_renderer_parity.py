#!/usr/bin/env python3
"""Compare two renderer backends pixel-for-pixel, and profile them fairly.

The renderers are meant to be interchangeable, so the strongest check available is that they
produce identical frames from identical state. This drives the game through the command socket
(see oa_harness.py), captures the same scene under two backends, and diffs the results.

  # do Metal and GL_2_0 draw the same city?
  tools/oa_renderer_parity.py compare --a Metal --b GL_2_0 --load /path/to/save

  # frame profile for one backend
  tools/oa_renderer_parity.py bench --renderer GLES_3_0 --extra --Framework.GLProfile=core

  # just diff two PNGs you already have
  tools/oa_renderer_parity.py diff before.png after.png

Two traps this tool exists to stop you falling into:

* Several screens are not deterministic between launches, so a raw A/B diff can report a
  difference that is really animation. 'compare' therefore captures a *same-renderer control*
  first and refuses to interpret the A/B number unless the control is clean. Pass --no-control
  only when you already know the scene is static.

* On macOS a *windowed* Metal layer is paced by the window server no matter what the swap
  interval says, so a windowed benchmark silently pins Metal to the panel refresh rate while
  the GL backends run uncapped. 'bench' defaults to borderless and warns if you override it.
"""

from __future__ import annotations

import argparse
import collections
import os
import struct
import subprocess
import sys
import time
import zlib

DEFAULT_APP = "build/bin/OpenApoc.app/Contents/MacOS/OpenApoc"
HARNESS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "oa_harness.py")


# --- PNG decoding ------------------------------------------------------------------------
# Deliberately dependency-free: this has to run on a bare checkout with no pip install.


def load_png(path):
    """Return (width, height, rgb_bytes) for 8-bit PNGs of colour type 0/2/3/6."""
    data = open(path, "rb").read()
    pos = 8
    idat = b""
    width = height = colour_type = None
    palette = b""
    while pos < len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        chunk = data[pos + 4 : pos + 8]
        body = data[pos + 8 : pos + 8 + length]
        if chunk == b"IHDR":
            width, height, depth, colour_type, _, _, interlace = struct.unpack(
                ">IIBBBBB", body[:13]
            )
            if depth != 8 or interlace != 0:
                raise ValueError(f"{path}: unsupported bit depth/interlace {depth}/{interlace}")
        elif chunk == b"PLTE":
            palette = body
        elif chunk == b"IDAT":
            idat += body
        pos += 12 + length
    raw = zlib.decompress(idat)
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[colour_type]
    stride = width * channels
    out = bytearray()
    prev = bytearray(stride)
    i = 0
    for _ in range(height):
        filter_type = raw[i]
        i += 1
        line = bytearray(raw[i : i + stride])
        i += stride
        if filter_type:
            for x in range(stride):
                a = line[x - channels] if x >= channels else 0
                b = prev[x]
                c = prev[x - channels] if x >= channels else 0
                if filter_type == 1:
                    line[x] = (line[x] + a) & 255
                elif filter_type == 2:
                    line[x] = (line[x] + b) & 255
                elif filter_type == 3:
                    line[x] = (line[x] + (a + b) // 2) & 255
                elif filter_type == 4:
                    p = a + b - c
                    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                    pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                    line[x] = (line[x] + pred) & 255
        out += line
        prev = line

    rgb = bytearray(width * height * 3)
    for idx in range(width * height):
        o = idx * channels
        if colour_type == 3:
            p = out[o] * 3
            rgb[idx * 3 : idx * 3 + 3] = palette[p : p + 3]
        elif colour_type in (0, 4):
            rgb[idx * 3] = rgb[idx * 3 + 1] = rgb[idx * 3 + 2] = out[o]
        else:
            rgb[idx * 3 : idx * 3 + 3] = out[o : o + 3]
    return width, height, bytes(rgb)


def diff_images(path_a, path_b):
    """Return a dict describing how two PNGs differ."""
    wa, ha, a = load_png(path_a)
    wb, hb, b = load_png(path_b)
    if (wa, ha) != (wb, hb):
        return {"error": f"size mismatch {wa}x{ha} vs {wb}x{hb}"}
    total = wa * ha
    differing = 0
    histogram = collections.Counter()
    min_x, min_y, max_x, max_y = wa, ha, -1, -1
    for y in range(ha):
        row = y * wa * 3
        for x in range(wa):
            o = row + x * 3
            if a[o : o + 3] != b[o : o + 3]:
                differing += 1
                delta = max(abs(a[o + i] - b[o + i]) for i in range(3))
                histogram[min(delta, 64)] += 1
                min_x, max_x = min(min_x, x), max(max_x, x)
                min_y, max_y = min(min_y, y), max(max_y, y)
    return {
        "width": wa,
        "height": ha,
        "total": total,
        "differing": differing,
        "percent": 100.0 * differing / total if total else 0.0,
        "histogram": dict(sorted(histogram.items())),
        "bbox": None if max_x < 0 else (min_x, min_y, max_x, max_y),
    }


# --- driving the game --------------------------------------------------------------------


def harness(port, *words, timeout=10):
    cmd = [sys.executable, HARNESS, "--port", str(port), *[str(w) for w in words]]
    try:
        done = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return done.stdout.strip()
    except subprocess.TimeoutExpired:
        return ""


def capture(app, renderer, out_png, port, args):
    """Launch one backend, wait for a stage, screenshot it, and shut down.

    Returns (screenshot_path, [frame profile lines]). The caller owns interpreting them.
    """
    log_path = f"{out_png}.log"
    launch = [
        app,
        f"--Framework.Renderers={renderer}",
        "--Framework.Harness.Enable=1",
        f"--Framework.Harness.Port={port}",
        "--Game.SkipIntro=1",
        "--Config.Save=0",
        f"--Framework.SwapInterval={args.swap_interval}",
        f"--Framework.ProfileFrames={args.profile_frames}",
        f"--Framework.Screen.Mode={args.mode}",
        f"--Framework.Screen.Width={args.width}",
        f"--Framework.Screen.Height={args.height}",
        "--Framework.TargetFPS=1000",
    ]
    if args.load:
        launch.append(f"--Game.Load={args.load}")
    launch += args.extra

    with open(log_path, "w") as log:
        proc = subprocess.Popen(launch, stdout=log, stderr=subprocess.STDOUT)

    stage = ""
    deadline = time.time() + args.startup_timeout
    while time.time() < deadline:
        stage = harness(port, "status")
        if "stage=" in stage:
            break
        time.sleep(2)
    if "stage=" not in stage:
        proc.kill()
        raise SystemExit(f"{renderer}: game never reached a stage (see {log_path})")

    # Let the scene settle and the profiler accumulate a full window before sampling.
    time.sleep(args.settle)
    harness(port, "screenshot", out_png)
    time.sleep(args.settle)
    profiles = [l.strip() for l in open(log_path) if "Frame profile" in l]
    harness(port, "quit")
    time.sleep(2)
    if proc.poll() is None:
        proc.kill()
    proc.wait()
    return out_png, profiles


def report_diff(label, result):
    if "error" in result:
        print(f"{label}: {result['error']}")
        return False
    print(
        f"{label}: {result['differing']} / {result['total']} pixels differ "
        f"({result['percent']:.3f}%)"
    )
    if result["differing"]:
        print(f"  bounding box: {result['bbox']}")
        print(f"  max-channel-delta histogram (64 = 64 or more): {result['histogram']}")
    return result["differing"] == 0


# --- subcommands -------------------------------------------------------------------------


def cmd_diff(args):
    clean = report_diff(f"{args.a} vs {args.b}", diff_images(args.a, args.b))
    return 0 if clean else 1


def cmd_bench(args):
    if args.mode != "borderless":
        print(
            f"warning: --mode {args.mode} -- on macOS a windowed Metal layer is paced by the "
            "window server whatever the swap interval says, so this measurement is not "
            "comparable against a GL backend.",
            file=sys.stderr,
        )
    out = args.out or f"bench-{args.renderer}.png"
    _, profiles = capture(args.app, args.renderer, out, args.port, args)
    if not profiles:
        raise SystemExit(f"{args.renderer}: no frame profile -- was --Framework.ProfileFrames set?")
    print(f"--- {args.renderer} ---")
    for line in profiles[-3:]:
        print(f"  {line.split('Frame profile')[-1].strip()}")
    return 0


def cmd_compare(args):
    prefix = args.out_prefix
    if args.control:
        # A same-renderer control first: without it a nondeterministic scene reads as a
        # renderer defect, which is a mistake that is very easy to make and very slow to undo.
        a1, _ = capture(args.app, args.a, f"{prefix}-{args.a}-1.png", args.port, args)
        a2, _ = capture(args.app, args.a, f"{prefix}-{args.a}-2.png", args.port + 1, args)
        control = diff_images(a1, a2)
        if not report_diff(f"control ({args.a} vs itself)", control):
            print(
                "\nThe control is not clean, so this scene is not deterministic between "
                "launches and any A/B number below is meaningless. Pick a static scene.",
                file=sys.stderr,
            )
            return 2
    else:
        a1, _ = capture(args.app, args.a, f"{prefix}-{args.a}-1.png", args.port, args)

    b1, _ = capture(args.app, args.b, f"{prefix}-{args.b}-1.png", args.port + 2, args)
    identical = report_diff(f"{args.a} vs {args.b}", diff_images(a1, b1))
    return 0 if identical else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    def add_run_options(p):
        p.add_argument("--app", default=DEFAULT_APP)
        p.add_argument("--port", type=int, default=17321)
        p.add_argument("--load", default="", help="save game to load at startup")
        p.add_argument("--mode", default="borderless", choices=["borderless", "windowed", "fullscreen"])
        p.add_argument("--width", type=int, default=1280)
        p.add_argument("--height", type=int, default=720)
        p.add_argument("--swap-interval", type=int, default=0)
        p.add_argument("--profile-frames", type=int, default=240)
        p.add_argument("--settle", type=float, default=10.0, help="seconds to let the scene settle")
        p.add_argument("--startup-timeout", type=float, default=120.0)
        p.add_argument("--extra", nargs=argparse.REMAINDER, default=[],
                       help="remaining args are passed to the game verbatim")

    p_diff = sub.add_parser("diff", help="diff two PNGs that already exist")
    p_diff.add_argument("a")
    p_diff.add_argument("b")
    p_diff.set_defaults(func=cmd_diff)

    p_bench = sub.add_parser("bench", help="frame profile for one backend")
    p_bench.add_argument("--renderer", required=True)
    p_bench.add_argument("--out", default="")
    add_run_options(p_bench)
    p_bench.set_defaults(func=cmd_bench)

    p_cmp = sub.add_parser("compare", help="capture the same scene under two backends and diff")
    p_cmp.add_argument("--a", required=True, help="reference renderer, e.g. Metal")
    p_cmp.add_argument("--b", required=True, help="renderer under test, e.g. GL_2_0")
    p_cmp.add_argument("--out-prefix", default="parity")
    p_cmp.add_argument("--no-control", dest="control", action="store_false",
                       help="skip the same-renderer control (only for scenes known to be static)")
    p_cmp.set_defaults(func=cmd_compare, control=True)
    add_run_options(p_cmp)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
