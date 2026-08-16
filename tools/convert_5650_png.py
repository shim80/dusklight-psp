#!/usr/bin/env python3
"""Convert PSP little-endian RGB565 framebuffers to cropped PNG files."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


def convert(source: Path, output: Path, width: int, height: int, stride: int) -> None:
    raw = source.read_bytes()
    expected = stride * height * 2
    if len(raw) != expected:
        raise ValueError(f"{source}: expected {expected} bytes, got {len(raw)}")
    rgb = bytearray(width * height * 3)
    cursor = 0
    for y in range(height):
        row = y * stride * 2
        for x in range(width):
            value = raw[row + x * 2] | (raw[row + x * 2 + 1] << 8)
            rgb[cursor] = (value & 0x1F) * 255 // 31
            rgb[cursor + 1] = ((value >> 5) & 0x3F) * 255 // 63
            rgb[cursor + 2] = ((value >> 11) & 0x1F) * 255 // 31
            cursor += 3
    output.parent.mkdir(parents=True, exist_ok=True)
    Image.frombytes("RGB", (width, height), bytes(rgb)).save(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("inputs", nargs="+", type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--width", type=int, default=480)
    parser.add_argument("--height", type=int, default=272)
    parser.add_argument("--stride", type=int, default=512)
    args = parser.parse_args()
    for source in args.inputs:
        output = args.output_dir / f"{source.stem}.png"
        convert(source, output, args.width, args.height, args.stride)
        print(f"RGB565_PNG_OK input={source} output={output}")


if __name__ == "__main__":
    main()
