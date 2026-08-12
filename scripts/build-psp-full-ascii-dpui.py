#!/usr/bin/env python3
"""Expand a DPUI v2 atlas with printable ASCII from Twilight Princess BFN.

The script keeps the existing PSP HUD/pause sprites verbatim, decodes the
source Rodan BFN I4 glyph sheets, crops glyphs to their non-zero alpha bounds,
and stores source-cell x/y bearings in DPUI record fields +4/+6.  The atlas
stays 512x128 RGBA4444, so PSP EDRAM usage does not increase.
"""
from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path

ASCII_FIRST = 0x20
ASCII_LAST = 0x7E
FONT_ID_BASE = 128
DPUI_HEADER = 128
DPUI_RECORD = 32
ATLAS_W = 512
ATLAS_H = 128


def u16(b: bytes | bytearray, o: int) -> int:
    return struct.unpack_from('<H', b, o)[0]


def u32(b: bytes | bytearray, o: int) -> int:
    return struct.unpack_from('<I', b, o)[0]


def p16(b: bytearray, o: int, v: int) -> None:
    struct.pack_into('<H', b, o, v & 0xFFFF)


def p32(b: bytearray, o: int, v: int) -> None:
    struct.pack_into('<I', b, o, v & 0xFFFFFFFF)


def be16(b: bytes, o: int) -> int:
    return struct.unpack_from('>H', b, o)[0]


def be32(b: bytes, o: int) -> int:
    return struct.unpack_from('>I', b, o)[0]


def align(v: int, a: int = 16) -> int:
    return (v + a - 1) & ~(a - 1)


def fnv1a(text: str) -> int:
    h = 0x811C9DC5
    for c in text.encode('ascii'):
        h ^= c
        h = (h * 0x01000193) & 0xFFFFFFFF
    return h or 1


def package_crc(data: bytes | bytearray) -> int:
    tmp = bytearray(data)
    tmp[12:16] = b'\0' * 4
    return zlib.crc32(tmp) & 0xFFFFFFFF


def psp_unswizzle_16(raw: bytes, width: int, height: int) -> bytearray:
    byte_width = width * 2
    out = bytearray(len(raw))
    for y in range(height):
        for xb in range(byte_width):
            block_x = xb // 16
            block_y = y // 8
            src = block_y * byte_width * 8 + block_x * 128 + (y & 7) * 16 + (xb & 15)
            out[y * byte_width + xb] = raw[src]
    return out


def psp_swizzle_16(linear: bytes | bytearray, width: int, height: int) -> bytes:
    byte_width = width * 2
    out = bytearray(len(linear))
    for y in range(height):
        for xb in range(byte_width):
            block_x = xb // 16
            block_y = y // 8
            dst = block_y * byte_width * 8 + block_x * 128 + (y & 7) * 16 + (xb & 15)
            out[dst] = linear[y * byte_width + xb]
    return bytes(out)


class BfnFont:
    def __init__(self, path: Path):
        self.data = path.read_bytes()
        if self.data[:8] != b'FONTbfn1':
            raise ValueError('not a FONTbfn1 file')
        count = be32(self.data, 0x0C)
        blocks: dict[str, tuple[int, int]] = {}
        cursor = 0x20
        for _ in range(count):
            tag = self.data[cursor:cursor + 4].decode('ascii')
            size = be32(self.data, cursor + 4)
            blocks[tag] = (cursor, size)
            cursor += size
        if not {'GLY1', 'MAP1', 'WID1'} <= blocks.keys():
            raise ValueError('BFN missing GLY1/MAP1/WID1')
        go, gs = blocks['GLY1']
        self.glyph_first = be16(self.data, go + 8)
        self.glyph_last = be16(self.data, go + 10)
        self.cell_w = be16(self.data, go + 12)
        self.cell_h = be16(self.data, go + 14)
        self.sheet_bytes = be32(self.data, go + 16)
        self.format = be16(self.data, go + 20)
        self.rows = be16(self.data, go + 22)
        self.cols = be16(self.data, go + 24)
        self.sheet_w = be16(self.data, go + 26)
        self.sheet_h = be16(self.data, go + 28)
        self.glyph_payload = go + 32
        if (self.cell_w, self.cell_h, self.sheet_bytes, self.format,
                self.rows, self.cols, self.sheet_w, self.sheet_h) != (24, 24, 8192, 0, 5, 5, 128, 128):
            raise ValueError('unexpected Rodan GLY1 layout')
        wo, _ = blocks['WID1']
        self.width_first = be16(self.data, wo + 8)
        self.width_last = be16(self.data, wo + 10)
        self.width_data = wo + 12

    def glyph_index(self, code: int) -> int:
        # MAP1 is linear 0x20..0xFF for this source font.
        if not (ASCII_FIRST <= code <= ASCII_LAST):
            raise ValueError('code outside printable ASCII')
        return code - ASCII_FIRST

    def advance(self, code: int) -> int:
        idx = self.glyph_index(code)
        if not (self.width_first <= idx <= self.width_last):
            raise ValueError('WID1 index outside range')
        # WID1 stores [left/kerning byte, advance byte]. This was verified
        # against all 10 glyph advances already present in the original DPUI.
        return self.data[self.width_data + idx * 2 + 1]

    def intensity(self, glyph: int, x: int, y: int) -> int:
        sheet = glyph // (self.rows * self.cols)
        cell = glyph % (self.rows * self.cols)
        source_x = (cell % self.cols) * self.cell_w + x
        source_y = (cell // self.cols) * self.cell_h + y
        tile = (source_y // 8) * (self.sheet_w // 8) + (source_x // 8)
        within = (source_y & 7) * 8 + (source_x & 7)
        byte = self.data[
            self.glyph_payload + sheet * self.sheet_bytes + tile * 32 + within // 2]
        return (byte >> 4) & 0xF if (within & 1) == 0 else byte & 0xF

    def cropped(self, code: int) -> tuple[int, int, int, int, list[int]]:
        glyph = self.glyph_index(code)
        values = [[self.intensity(glyph, x, y) for x in range(24)] for y in range(24)]
        points = [(x, y) for y in range(24) for x in range(24) if values[y][x] != 0]
        if not points:
            # Space: keep one transparent texel so every DPUI record has a
            # non-zero source size, while advance comes from WID1.
            return 0, 0, 1, 1, [0]
        x0 = min(x for x, _ in points)
        x1 = max(x for x, _ in points)
        y0 = min(y for _, y in points)
        y1 = max(y for _, y in points)
        width = x1 - x0 + 1
        height = y1 - y0 + 1
        pixels = []
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                a = values[y][x]
                # PSP ABGR4444: alpha in high nibble, white RGB.
                pixels.append((a << 12) | 0x0FFF)
        return x0, y0, width, height, pixels


def maxrects(items: list[dict], width: int, height: int) -> None:
    """Deterministic best-short-side MaxRects packer.

    Each item contains pack_w/pack_h (which may include a gutter). The actual
    sampled image remains w/h at the placed x/y origin.
    """
    free = [(0, 0, width, height)]
    ordered = sorted(items, key=lambda it: (-(it['pack_w'] * it['pack_h']), -it['pack_h'], -it['pack_w'], it['key']))
    for item in ordered:
        best = None
        rw, rh = item['pack_w'], item['pack_h']
        for index, (fx, fy, fw, fh) in enumerate(free):
            if rw <= fw and rh <= fh:
                score = (min(fw - rw, fh - rh), max(fw - rw, fh - rh), fy, fx, index)
                if best is None or score < best[0]:
                    best = (score, fx, fy)
        if best is None:
            raise ValueError(f"atlas packing failed at {item['key']}")
        _, ux, uy = best
        item['x'], item['y'] = ux, uy
        new_free = []
        for fx, fy, fw, fh in free:
            if ux >= fx + fw or ux + rw <= fx or uy >= fy + fh or uy + rh <= fy:
                new_free.append((fx, fy, fw, fh))
                continue
            if ux > fx:
                new_free.append((fx, fy, ux - fx, fh))
            if ux + rw < fx + fw:
                new_free.append((ux + rw, fy, fx + fw - (ux + rw), fh))
            if uy > fy:
                new_free.append((fx, fy, fw, uy - fy))
            if uy + rh < fy + fh:
                new_free.append((fx, uy + rh, fw, fy + fh - (uy + rh)))
        pruned = []
        for i, a in enumerate(new_free):
            ax, ay, aw, ah = a
            if aw <= 0 or ah <= 0:
                continue
            contained = False
            for j, b in enumerate(new_free):
                if i == j:
                    continue
                bx, by, bw, bh = b
                if ax >= bx and ay >= by and ax + aw <= bx + bw and ay + ah <= by + bh:
                    contained = True
                    break
            if not contained:
                pruned.append(a)
        free = pruned


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument('--base-dpui', type=Path, required=True)
    ap.add_argument('--bfn', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()

    base = args.base_dpui.read_bytes()
    if base[:4] != b'DPUI' or u16(base, 4) != 2:
        raise ValueError('base must be DPUI v2')
    if (u32(base, 16), u32(base, 20), u32(base, 24)) != (ATLAS_W, ATLAS_H, 2):
        raise ValueError('unexpected base atlas')
    count = u32(base, 28)
    table = u32(base, 32)
    stride = u32(base, 36)
    atlas_offset = u32(base, 40)
    atlas_bytes = u32(base, 44)
    if stride != DPUI_RECORD or atlas_bytes != ATLAS_W * ATLAS_H * 2:
        raise ValueError('unexpected base DPUI layout')
    old_linear = psp_unswizzle_16(
        base[atlas_offset:atlas_offset + atlas_bytes], ATLAS_W, ATLAS_H)

    # Existing source UI sprites are IDs < 128. Existing partial font records
    # are discarded and regenerated from the source BFN.
    sprite_records: list[bytearray] = []
    existing_font_hashes: dict[int, int] = {}
    for i in range(count):
        q = table + i * stride
        record = bytearray(base[q:q + stride])
        rid = u16(record, 0)
        if rid < FONT_ID_BASE:
            sprite_records.append(record)
        else:
            existing_font_hashes[rid] = u32(record, 24)
    if len(sprite_records) != 20:
        raise ValueError(f'expected 20 source sprite records, got {len(sprite_records)}')

    font = BfnFont(args.bfn)
    items: list[dict] = []
    for index, record in enumerate(sprite_records):
        w, h = u16(record, 16), u16(record, 18)
        x, y = u16(record, 12), u16(record, 14)
        pixels = []
        for row in range(h):
            start = ((y + row) * ATLAS_W + x) * 2
            pixels.extend(struct.unpack_from('<' + 'H' * w, old_linear, start))
        items.append({
            'key': f'sprite-{u16(record, 0):03d}', 'kind': 'sprite',
            'record': record, 'w': w, 'h': h, 'pack_w': w + 1,
            'pack_h': h + 1, 'pixels': pixels,
        })

    for code in range(ASCII_FIRST, ASCII_LAST + 1):
        bearing_x, bearing_y, w, h, pixels = font.cropped(code)
        rid = FONT_ID_BASE + code
        record = bytearray(DPUI_RECORD)
        p16(record, 0, rid)
        p16(record, 2, 8)
        p16(record, 4, bearing_x)
        p16(record, 6, bearing_y)
        p16(record, 8, 24)
        p16(record, 10, 24)
        p16(record, 16, w)
        p16(record, 18, h)
        p32(record, 20, 0xFFFFFFFF)
        p32(record, 24, existing_font_hashes.get(rid, fnv1a(f'rodan-bfn-ascii-{code:02x}')))
        p16(record, 28, font.advance(code))
        items.append({
            'key': f'glyph-{code:02x}', 'kind': 'glyph', 'record': record,
            'w': w, 'h': h, 'pack_w': w, 'pack_h': h, 'pixels': pixels,
        })

    maxrects(items, ATLAS_W, ATLAS_H)
    linear = bytearray(ATLAS_W * ATLAS_H * 2)
    for item in items:
        x, y, w, h = item['x'], item['y'], item['w'], item['h']
        p16(item['record'], 12, x)
        p16(item['record'], 14, y)
        for row in range(h):
            for col in range(w):
                pixel = item['pixels'][row * w + col]
                struct.pack_into('<H', linear, ((y + row) * ATLAS_W + x + col) * 2, pixel)

    # Stable order: original 20 sprite records in original order, then ASCII.
    by_id = {u16(it['record'], 0): it['record'] for it in items}
    records = [by_id[u16(r, 0)] for r in sprite_records]
    records += [by_id[FONT_ID_BASE + code] for code in range(ASCII_FIRST, ASCII_LAST + 1)]
    if len(records) != 115:
        raise AssertionError(len(records))

    new_table = DPUI_HEADER
    new_atlas = align(new_table + len(records) * DPUI_RECORD, 16)
    out = bytearray(new_atlas + ATLAS_W * ATLAS_H * 2)
    out[:DPUI_HEADER] = base[:DPUI_HEADER]
    p32(out, 8, len(out))
    p32(out, 12, 0)
    p32(out, 16, ATLAS_W)
    p32(out, 20, ATLAS_H)
    p32(out, 24, 2)
    p32(out, 28, len(records))
    p32(out, 32, new_table)
    p32(out, 36, DPUI_RECORD)
    p32(out, 40, new_atlas)
    p32(out, 44, ATLAS_W * ATLAS_H * 2)
    p32(out, 48, len(records))
    # +52/+56 are source layout identity dimensions, +60 source sprite count,
    # +64 format-generation marker. Preserve them unchanged.
    for i, record in enumerate(records):
        q = new_table + i * DPUI_RECORD
        out[q:q + DPUI_RECORD] = record
    swizzled = psp_swizzle_16(linear, ATLAS_W, ATLAS_H)
    out[new_atlas:new_atlas + len(swizzled)] = swizzled
    p32(out, 12, package_crc(out))

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(out)
    max_y = max(it['y'] + it['h'] for it in items)
    print(
        f'FULL_ASCII_DPUI_OK records={len(records)} sprites={len(sprite_records)} '
        f'glyphs={ASCII_LAST-ASCII_FIRST+1} atlas={ATLAS_W}x{ATLAS_H} '
        f'max_used_y={max_y} bytes={len(out)} crc=0x{u32(out,12):08x}')
    for ch in (' ', '!', 'A', 'E', 'R', 'e', 'i', 'm', 'o', 's', 't', 'u', 'x', 'Y'):
        rec = by_id[FONT_ID_BASE + ord(ch)]
        print(
            f'glyph={ch!r} uv={u16(rec,12)},{u16(rec,14)} '
            f'size={u16(rec,16)}x{u16(rec,18)} bearing={u16(rec,4)},{u16(rec,6)} '
            f'advance={u16(rec,28)}')


if __name__ == '__main__':
    main()
