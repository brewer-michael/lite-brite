#!/usr/bin/env python3
"""Render lite-brite messages to an animated GIF, using the firmware's own code.

Builds the desktop simulator (`pio run -e sim` in firmware/) the first time,
runs it with your payloads, and draws each frame as glowing LED dots.

    python3 tools/preview.py --out keys.gif \
        '{"text": "Welcome home! :key: Keys away first :smile:", "color": "orange", "effect": "flash"}'

Several payloads play back to back, like several waiting messages would.
Needs Pillow (pip install pillow) and PlatformIO (or pass --sim PATH).
"""

from __future__ import annotations

import argparse
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIRMWARE = ROOT / "firmware"
DEFAULT_SIM = FIRMWARE / ".pio" / "build" / "sim" / "program"

BACKGROUND = (14, 14, 17)
DARK_LED = (34, 34, 40)


def build_sim() -> Path:
    pio = shutil.which("pio") or shutil.which("platformio")
    if pio is None:
        sys.exit("PlatformIO not found; install it or pass --sim path/to/program")
    subprocess.run([pio, "run", "-e", "sim"], cwd=FIRMWARE, check=True)
    return DEFAULT_SIM


def read_frames(path: Path):
    data = path.read_bytes()
    header_end = data.index(b"\n")
    magic, width, height = data[:header_end].decode().split()
    if magic != "LBSIM1":
        sys.exit(f"{path}: not a simulator frame file")
    width, height = int(width), int(height)
    size = width * height * 3
    frames, pos = [], header_end + 1
    while pos + 4 + size <= len(data):
        (t_us,) = struct.unpack_from("<I", data, pos)
        frames.append((t_us, data[pos + 4 : pos + 4 + size]))
        pos += 4 + size
    return width, height, frames


def merge_repeats(frames, end_us):
    """Collapse runs of identical frames into (duration_us, pixels)."""
    merged = []
    for i, (t_us, pixels) in enumerate(frames):
        nxt = frames[i + 1][0] if i + 1 < len(frames) else end_us
        if merged and merged[-1][1] == pixels:
            merged[-1][0] += nxt - t_us
        else:
            merged.append([nxt - t_us, pixels])
    return merged


def gif_durations(durations_us):
    """GIF delays are in 10 ms steps; diffuse the rounding error so long
    animations keep their real speed."""
    out, error = [], 0.0
    for d in durations_us:
        exact = d / 1000.0 + error
        rounded = max(20, int(round(exact / 10.0)) * 10)
        error = exact - rounded
        out.append(rounded)
    return out


def render(width, height, pixels, pitch, pil):
    Image, ImageChops, ImageDraw, ImageFilter = pil
    w, h = width * pitch, height * pitch
    dots = Image.new("RGB", (w, h), BACKGROUND)
    glow = Image.new("RGB", (w, h), (0, 0, 0))
    d_dots, d_glow = ImageDraw.Draw(dots), ImageDraw.Draw(glow)
    r, g = pitch * 0.40, pitch * 0.70
    for y in range(height):
        for x in range(width):
            i = (y * width + x) * 3
            c = (pixels[i], pixels[i + 1], pixels[i + 2])
            cx, cy = x * pitch + pitch / 2, y * pitch + pitch / 2
            lit = c != (0, 0, 0)
            d_dots.ellipse([cx - r, cy - r, cx + r, cy + r], fill=c if lit else DARK_LED)
            if lit:
                d_glow.ellipse([cx - g, cy - g, cx + g, cy + g], fill=c)
    # A soft halo around lit LEDs, added with a "screen" blend like light.
    glow = glow.filter(ImageFilter.GaussianBlur(pitch * 0.45))
    glow = ImageChops.multiply(glow, Image.new("RGB", glow.size, (150, 150, 150)))
    return ImageChops.screen(dots, glow)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("payloads", nargs="+", help="message JSON or plain text")
    ap.add_argument("--out", type=Path, required=True, help="GIF to write")
    ap.add_argument("--width", type=int, default=32)
    ap.add_argument("--height", type=int, default=8)
    ap.add_argument("--speed", type=int, help="default scroll speed (px/s)")
    ap.add_argument("--duration", type=int, help="default seconds per message")
    ap.add_argument("--pitch", type=int, default=10, help="GIF pixels per LED")
    ap.add_argument("--colors", type=int, default=64, help="GIF palette size")
    ap.add_argument("--sim", type=Path, help="path to an already-built simulator")
    args = ap.parse_args()

    try:
        from PIL import Image, ImageChops, ImageDraw, ImageFilter
    except ImportError:
        sys.exit("preview.py needs Pillow: pip install pillow")
    pil = (Image, ImageChops, ImageDraw, ImageFilter)

    sim = args.sim or (DEFAULT_SIM if DEFAULT_SIM.exists() else build_sim())
    with tempfile.TemporaryDirectory() as tmp:
        frames_path = Path(tmp) / "out.frames"
        cmd = [str(sim), "--out", str(frames_path), "--width", str(args.width), "--height", str(args.height)]
        if args.speed:
            cmd += ["--speed", str(args.speed)]
        if args.duration:
            cmd += ["--duration", str(args.duration)]
        subprocess.run(cmd + args.payloads, check=True)
        width, height, frames = read_frames(frames_path)

    if not frames:
        sys.exit("simulator produced no frames")
    end_us = frames[-1][0] + 40000
    merged = merge_repeats(frames, end_us)
    images = [render(width, height, px, args.pitch, pil) for _, px in merged]
    durations = gif_durations([d for d, _ in merged])
    # One shared palette for all frames keeps the GIF small and flicker-free.
    sample = Image.new("RGB", (images[0].width, images[0].height * min(len(images), 16)))
    for k, img in enumerate(images[:: max(1, len(images) // 16)][:16]):
        sample.paste(img, (0, k * img.height))
    palette = sample.quantize(colors=args.colors, method=Image.Quantize.MEDIANCUT)
    images = [img.quantize(palette=palette, dither=Image.Dither.NONE) for img in images]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    images[0].save(
        args.out,
        save_all=True,
        append_images=images[1:],
        duration=durations,
        loop=0,
        optimize=True,
        disposal=1,
    )
    total = sum(durations) / 1000
    print(f"wrote {args.out} ({len(images)} frames, {total:.1f} s, {args.out.stat().st_size // 1024} KiB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
