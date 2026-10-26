#!/usr/bin/env python3
"""Resize PNG to WxH and write raw RGB565 LE .bin (LVGL / SD images/)."""
import argparse
import struct
from pathlib import Path

from PIL import Image


def png_to_rgb565_bin(png_path: Path, out_path: Path, width: int, height: int) -> None:
    im = Image.open(png_path).convert("RGBA")
    im = im.resize((width, height), Image.Resampling.LANCZOS)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("wb") as f:
        for y in range(height):
            for x in range(width):
                r, g, b, a = im.getpixel((x, y))
                if a < 32:
                    v = 0
                else:
                    v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                f.write(struct.pack("<H", v))
    im.save(png_path)


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("png", type=Path)
    p.add_argument("-o", "--out", type=Path, help="output .bin (default: same name .bin)")
    p.add_argument("-s", "--size", type=int, default=20, help="square size in pixels")
    args = p.parse_args()
    out = args.out or args.png.with_suffix(".bin")
    png_to_rgb565_bin(args.png, out, args.size, args.size)
    print(out, args.size * args.size * 2, "bytes")


if __name__ == "__main__":
    main()
