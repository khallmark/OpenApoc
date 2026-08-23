#!/usr/bin/env python3
"""Build OpenApoc.icns / AppIcon PNG from a PPM via sips + iconutil."""
import shutil
import subprocess
import sys
from pathlib import Path


def write_ppm(path: Path, size: int) -> None:
    header = f"P6\n{size} {size}\n255\n".encode()
    row = bytes([0, 48, 96]) * size
    path.write_bytes(header + row * size)


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: generate_icon.py <output-dir>", file=sys.stderr)
        return 2
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    iconset = out / "OpenApoc.iconset"
    if iconset.exists():
        shutil.rmtree(iconset)
    iconset.mkdir()
    master = out / "icon-1024.ppm"
    write_ppm(master, 1024)
    png1024 = out / "AppIcon-1024.png"
    subprocess.check_call(["sips", "-s", "format", "png", str(master), "--out", str(png1024)])
    sizes = {
        "icon_16x16.png": 16,
        "icon_16x16@2x.png": 32,
        "icon_32x32.png": 32,
        "icon_32x32@2x.png": 64,
        "icon_128x128.png": 128,
        "icon_128x128@2x.png": 256,
        "icon_256x256.png": 256,
        "icon_256x256@2x.png": 512,
        "icon_512x512.png": 512,
        "icon_512x512@2x.png": 1024,
    }
    for name, px in sizes.items():
        dest = iconset / name
        subprocess.check_call(
            ["sips", "-z", str(px), str(px), str(png1024), "--out", str(dest)],
            stdout=subprocess.DEVNULL,
        )
    icns = out / "OpenApoc.icns"
    subprocess.check_call(["iconutil", "-c", "icns", str(iconset), "-o", str(icns)])
    print(icns)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
