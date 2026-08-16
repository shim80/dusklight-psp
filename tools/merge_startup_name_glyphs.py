#!/usr/bin/env python3
"""Merge source Rodan glyphs from DPUI v2 into the file-select DPSU atlas."""
from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path

HEADER = 128
RECORD = 32
ATLAS_W = 512
ATLAS_H = 512
CHARACTERS = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"


def u16(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def p16(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<H", data, offset, value & 0xFFFF)


def p32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value & 0xFFFFFFFF)


def align16(value: int) -> int:
    return (value + 15) & ~15


def package_crc(data: bytes | bytearray) -> int:
    copy = bytearray(data)
    copy[12:16] = b"\0" * 4
    return zlib.crc32(copy) & 0xFFFFFFFF


def unswizzle_16(raw: bytes, width: int, height: int) -> bytearray:
    byte_width = width * 2
    output = bytearray(len(raw))
    for y in range(height):
        for byte_x in range(byte_width):
            block_x = byte_x // 16
            block_y = y // 8
            source = (
                block_y * byte_width * 8
                + block_x * 128
                + (y & 7) * 16
                + (byte_x & 15)
            )
            output[y * byte_width + byte_x] = raw[source]
    return output


def swizzle_16(linear: bytes | bytearray, width: int, height: int) -> bytes:
    byte_width = width * 2
    output = bytearray(len(linear))
    for y in range(height):
        for byte_x in range(byte_width):
            block_x = byte_x // 16
            block_y = y // 8
            destination = (
                block_y * byte_width * 8
                + block_x * 128
                + (y & 7) * 16
                + (byte_x & 15)
            )
            output[destination] = linear[y * byte_width + byte_x]
    return bytes(output)


def pixels(
    linear: bytes | bytearray,
    atlas_width: int,
    x: int,
    y: int,
    width: int,
    height: int,
) -> tuple[int, ...]:
    result: list[int] = []
    for row in range(height):
        offset = ((y + row) * atlas_width + x) * 2
        result.extend(struct.unpack_from("<" + "H" * width, linear, offset))
    return tuple(result)


def maxrects(items: list[dict[str, object]]) -> None:
    free = [(0, 0, ATLAS_W, ATLAS_H)]
    ordered = sorted(
        items,
        key=lambda item: (
            -(int(item["width"]) * int(item["height"])),
            -int(item["height"]),
            -int(item["width"]),
            str(item["key"]),
        ),
    )
    for item in ordered:
        width = int(item["width"])
        height = int(item["height"])
        choices = []
        for index, (x, y, free_width, free_height) in enumerate(free):
            if width <= free_width and height <= free_height:
                choices.append(
                    (
                        min(free_width - width, free_height - height),
                        max(free_width - width, free_height - height),
                        y,
                        x,
                        index,
                    )
                )
        if not choices:
            raise ValueError(f"DPSU atlas overflow at {item['key']}")
        _, _, used_y, used_x, _ = min(choices)
        item["x"] = used_x
        item["y"] = used_y
        next_free = []
        for x, y, free_width, free_height in free:
            if (
                used_x >= x + free_width
                or used_x + width <= x
                or used_y >= y + free_height
                or used_y + height <= y
            ):
                next_free.append((x, y, free_width, free_height))
                continue
            if used_x > x:
                next_free.append((x, y, used_x - x, free_height))
            if used_x + width < x + free_width:
                next_free.append(
                    (
                        used_x + width,
                        y,
                        x + free_width - used_x - width,
                        free_height,
                    )
                )
            if used_y > y:
                next_free.append((x, y, free_width, used_y - y))
            if used_y + height < y + free_height:
                next_free.append(
                    (
                        x,
                        used_y + height,
                        free_width,
                        y + free_height - used_y - height,
                    )
                )
        free = [
            rectangle
            for index, rectangle in enumerate(next_free)
            if not any(
                index != other
                and rectangle[0] >= candidate[0]
                and rectangle[1] >= candidate[1]
                and rectangle[0] + rectangle[2]
                <= candidate[0] + candidate[2]
                and rectangle[1] + rectangle[3]
                <= candidate[1] + candidate[3]
                for other, candidate in enumerate(next_free)
            )
        ]


def validate_header(data: bytes, magic: bytes, version: int) -> None:
    if (
        data[:4] != magic
        or u16(data, 4) != version
        or u16(data, 6) != HEADER
        or u32(data, 8) != len(data)
        or u32(data, 12) != package_crc(data)
        or u32(data, 24) != 2
        or u32(data, 36) != RECORD
    ):
        raise ValueError(f"invalid {magic.decode('ascii')} package")


def merge(file_select_path: Path, hud_path: Path, output_path: Path) -> None:
    file_select = file_select_path.read_bytes()
    hud = hud_path.read_bytes()
    validate_header(file_select, b"DPSU", 1)
    validate_header(hud, b"DPUI", 2)
    if (u32(file_select, 16), u32(file_select, 20)) != (ATLAS_W, ATLAS_H):
        raise ValueError("unexpected DPSU atlas dimensions")

    file_atlas_offset = u32(file_select, 40)
    file_atlas_bytes = u32(file_select, 44)
    file_linear = unswizzle_16(
        file_select[file_atlas_offset : file_atlas_offset + file_atlas_bytes],
        ATLAS_W,
        ATLAS_H,
    )
    hud_width, hud_height = u32(hud, 16), u32(hud, 20)
    hud_atlas_offset, hud_atlas_bytes = u32(hud, 40), u32(hud, 44)
    hud_linear = unswizzle_16(
        hud[hud_atlas_offset : hud_atlas_offset + hud_atlas_bytes],
        hud_width,
        hud_height,
    )

    records: list[bytearray] = []
    items_by_pixels: dict[tuple[int, int, tuple[int, ...]], dict[str, object]] = {}
    record_items: list[dict[str, object]] = []
    table = u32(file_select, 32)
    for index in range(u32(file_select, 28)):
        record = bytearray(file_select[table + index * RECORD : table + (index + 1) * RECORD])
        width, height = u16(record, 16), u16(record, 18)
        content = pixels(
            file_linear,
            ATLAS_W,
            u16(record, 12),
            u16(record, 14),
            width,
            height,
        )
        identity = (width, height, content)
        item = items_by_pixels.get(identity)
        if item is None:
            item = {
                "key": f"file-{u16(record, 0):03d}",
                "width": width,
                "height": height,
                "pixels": content,
            }
            items_by_pixels[identity] = item
        records.append(record)
        record_items.append(item)

    hud_records = {
        u16(hud, u32(hud, 32) + index * RECORD):
        hud[u32(hud, 32) + index * RECORD : u32(hud, 32) + (index + 1) * RECORD]
        for index in range(u32(hud, 28))
    }
    for character in CHARACTERS:
        code = ord(character)
        source = hud_records.get(128 + code)
        if source is None:
            raise ValueError(f"Rodan glyph missing for ASCII {code}")
        width, height = u16(source, 16), u16(source, 18)
        content = pixels(
            hud_linear,
            hud_width,
            u16(source, 12),
            u16(source, 14),
            width,
            height,
        )
        item = {
            "key": f"glyph-{code:02x}",
            "width": width,
            "height": height,
            "pixels": content,
        }
        items_by_pixels[(width, height, content)] = item
        record = bytearray(RECORD)
        p16(record, 0, 256 + code - 32)
        p16(record, 2, 0xFFFF)
        p16(record, 8, width)
        p16(record, 10, height)
        p16(record, 16, width)
        p16(record, 18, height)
        p32(record, 20, 0xFFFFFFFF)
        p32(record, 24, u32(source, 24))
        p16(record, 28, u16(source, 28))
        records.append(record)
        record_items.append(item)

    unique_items = list({id(item): item for item in record_items}.values())
    maxrects(unique_items)
    linear = bytearray(ATLAS_W * ATLAS_H * 2)
    for item in unique_items:
        x, y = int(item["x"]), int(item["y"])
        width, height = int(item["width"]), int(item["height"])
        content = item["pixels"]
        for row in range(height):
            for column in range(width):
                struct.pack_into(
                    "<H",
                    linear,
                    ((y + row) * ATLAS_W + x + column) * 2,
                    content[row * width + column],
                )
    for record, item in zip(records, record_items):
        p16(record, 12, int(item["x"]))
        p16(record, 14, int(item["y"]))

    atlas_offset = align16(HEADER + len(records) * RECORD)
    output = bytearray(atlas_offset + len(linear))
    output[:HEADER] = file_select[:HEADER]
    p32(output, 8, len(output))
    p32(output, 12, 0)
    p32(output, 28, len(records))
    p32(output, 32, HEADER)
    p32(output, 40, atlas_offset)
    p32(output, 44, len(linear))
    p32(output, 48, 5)
    for index, record in enumerate(records):
        offset = HEADER + index * RECORD
        output[offset : offset + RECORD] = record
    output[atlas_offset:] = swizzle_16(linear, ATLAS_W, ATLAS_H)
    p32(output, 12, package_crc(output))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(output)
    print(
        "STARTUP_NAME_GLYPHS_OK "
        f"records={len(records)} source_file_records={u32(file_select, 28)} "
        f"glyphs={len(CHARACTERS)} unique_atlas_items={len(unique_items)} "
        f"atlas={ATLAS_W}x{ATLAS_H} crc=0x{u32(output, 12):08x}"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--file-select", type=Path, required=True)
    parser.add_argument("--hud", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    arguments = parser.parse_args()
    merge(arguments.file_select, arguments.hud, arguments.out)


if __name__ == "__main__":
    main()
