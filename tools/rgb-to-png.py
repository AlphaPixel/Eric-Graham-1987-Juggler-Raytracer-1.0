#!/usr/bin/env python3
"""Convert raw 24-bit RGB data to a simple PNG for inspection."""

import argparse
import struct
import zlib
from pathlib import Path


def png_chunk(kind, data):
    payload = kind + data
    return (
        struct.pack(">I", len(data))
        + payload
        + struct.pack(">I", zlib.crc32(payload) & 0xFFFFFFFF)
    )


def write_png(path, rgb, width, height, pixels_per_unit_x, pixels_per_unit_y, srgb):
    stride = width * 3
    rows = []
    for y in range(height):
        start = y * stride
        rows.append(b"\x00" + rgb[start : start + stride])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    phys = struct.pack(">IIB", pixels_per_unit_x, pixels_per_unit_y, 1)
    data = zlib.compress(b"".join(rows), 9)

    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", ihdr)
        + png_chunk(b"pHYs", phys)
        + (png_chunk(b"sRGB", b"\x00") if srgb else b"")
        + png_chunk(b"IDAT", data)
        + png_chunk(b"IEND", b"")
    )


def expand_ssg_rgb4(rgb):
    out = bytearray(len(rgb))
    for i, v in enumerate(rgb):
        q = v // 8
        if q > 15:
            q = 15
        out[i] = q * 17
    return bytes(out)


def main():
    parser = argparse.ArgumentParser(
        description="Convert raw 24-bit RGB bytes to PNG."
    )
    parser.add_argument("input", type=Path)
    parser.add_argument("output", nargs="?", type=Path)
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=200)
    parser.add_argument("--pixels-per-unit-x", type=int, default=1000)
    parser.add_argument("--pixels-per-unit-y", type=int, default=1000)
    parser.add_argument(
        "--ssg-rgb4-expand",
        action="store_true",
        help="expand SSG D= bytes through the RGB4 scale used by the HAM path",
    )
    parser.add_argument(
        "--srgb",
        action="store_true",
        help="write an sRGB rendering-intent chunk",
    )
    args = parser.parse_args()

    rgb = args.input.read_bytes()
    expected = args.width * args.height * 3
    if len(rgb) != expected:
        raise SystemExit(
            f"{args.input}: expected {expected} bytes for "
            f"{args.width}x{args.height} RGB, found {len(rgb)}"
        )

    if args.ssg_rgb4_expand:
        rgb = expand_ssg_rgb4(rgb)

    output = args.output or args.input.with_suffix(".png")
    write_png(
        output,
        rgb,
        args.width,
        args.height,
        args.pixels_per_unit_x,
        args.pixels_per_unit_y,
        args.srgb,
    )
    print(f"wrote {output} ({args.width}x{args.height})")


if __name__ == "__main__":
    main()
