#!/usr/bin/env bash
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$ROOT"

python3 - <<'PY'
from pathlib import Path
import struct


def png_size(data: bytes):
    if len(data) >= 24 and data[:8] == b'\x89PNG\r\n\x1a\n' and data[12:16] == b'IHDR':
        return struct.unpack('>II', data[16:24])
    return None


def jpeg_size(data: bytes):
    if len(data) < 4 or data[:2] != b'\xff\xd8':
        return None
    i = 2
    sof = {0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7,
           0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF}
    while i + 3 < len(data):
        if data[i] != 0xFF:
            i += 1
            continue
        while i < len(data) and data[i] == 0xFF:
            i += 1
        if i >= len(data):
            break
        marker = data[i]
        i += 1
        if marker in {0xD8, 0xD9} or 0xD0 <= marker <= 0xD7:
            continue
        if i + 1 >= len(data):
            break
        length = int.from_bytes(data[i:i+2], 'big')
        if length < 2 or i + length > len(data):
            break
        if marker in sof and length >= 7:
            height = int.from_bytes(data[i+3:i+5], 'big')
            width = int.from_bytes(data[i+5:i+7], 'big')
            return width, height
        i += length
    return None

root = Path('screenshot')
files = sorted(p for p in root.iterdir() if p.is_file())
if not files:
    raise SystemExit('SCREENSHOT_NATIVE_FAIL: screenshot/ is empty')

for path in files:
    data = path.read_bytes()
    size = png_size(data) if path.suffix.lower() == '.png' else jpeg_size(data) if path.suffix.lower() in {'.jpg', '.jpeg'} else None
    if size is None:
        raise SystemExit(f'SCREENSHOT_NATIVE_FAIL: unsupported/invalid image: {path}')
    if size != (480, 272):
        raise SystemExit(f'SCREENSHOT_NATIVE_FAIL: {path} is {size[0]}x{size[1]}, expected 480x272')
    print(f'SCREENSHOT_NATIVE_OK path={path} size={size[0]}x{size[1]}')

print(f'SCREENSHOT_NATIVE_SUITE_OK files={len(files)} size=480x272')
PY
