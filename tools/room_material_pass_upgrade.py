#!/usr/bin/env python3
"""Upgrade a DPTX v2/PEV1 package to bounded DPTX v3/MPV1 passes.

The conversion is intentionally conservative. It preserves the source pixel-engine
state and all PEV1 texture identities, appends (rather than relocates) an MPV1
table, and caps each PSP material at two passes.
"""
from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path

HEADER_SIZE = 128
TEXTURE_STRIDE = 48
MATERIAL_STRIDE = 32
PEV_STRIDE = 40
MPV_STRIDE = 48
PASS_STRIDE = 20
MPV_MAGIC = b"MPV1"

FIDELITY_EXACT = 0
FIDELITY_APPROXIMATE = 1
FIDELITY_UNSUPPORTED = 2
REASON_NONE = 0
REASON_TOO_MANY_STAGES = 1
REASON_UNSUPPORTED_TEV = 3
EFFECT_MODULATE = 0
EFFECT_REPLACE = 1
COLOR_RGBA = 1
BLEND_SOURCE = 0
BLEND_ALPHA = 1
BLEND_MULTIPLY = 4
ALPHA_BLEND_CLASS = 2
MULTIPLY_CLASS = 4
GX_BLEND = 1
GX_SRC_ALPHA = 4


def align(value: int, boundary: int = 16) -> int:
    return (value + boundary - 1) & ~(boundary - 1)


def u16(blob: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<H", blob, offset)[0]


def u32(blob: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def crc32_package(blob: bytes | bytearray) -> int:
    copy = bytearray(blob)
    copy[12:16] = b"\0" * 4
    return zlib.crc32(copy) & 0xFFFFFFFF


def validate_v2(blob: bytes) -> tuple[int, int, int, int, int]:
    if len(blob) < HEADER_SIZE or blob[:4] != b"DPTX":
        raise ValueError("expected DPTX package")
    if u16(blob, 4) != 2 or u16(blob, 6) != HEADER_SIZE:
        raise ValueError("expected DPTX v2 with 128-byte header")
    if u32(blob, 8) != len(blob) or u32(blob, 12) != crc32_package(blob):
        raise ValueError("invalid DPTX size/CRC")
    textures, materials = u32(blob, 16), u32(blob, 20)
    texture_table, texture_stride = u32(blob, 24), u32(blob, 28)
    material_table, material_stride = u32(blob, 32), u32(blob, 36)
    pev_table, pev_stride, pev_count = u32(blob, 68), u32(blob, 72), u32(blob, 76)
    if not 1 <= textures <= 96 or not 1 <= materials <= 96:
        raise ValueError("runtime count cap exceeded")
    if texture_stride != TEXTURE_STRIDE or material_stride != MATERIAL_STRIDE:
        raise ValueError("unexpected DPTX table stride")
    if blob[64:68] != b"PEV1" or pev_stride != PEV_STRIDE or pev_count != materials:
        raise ValueError("expected complete PEV1 table")
    if texture_table + textures * TEXTURE_STRIDE > len(blob):
        raise ValueError("texture table out of range")
    if material_table + materials * MATERIAL_STRIDE > len(blob):
        raise ValueError("material table out of range")
    if pev_table + materials * PEV_STRIDE > len(blob):
        raise ValueError("PEV1 table out of range")
    return textures, materials, texture_table, material_table, pev_table


def texture_has_alpha(blob: bytes, texture_table: int, texture_id: int) -> bool:
    return blob[texture_table + texture_id * TEXTURE_STRIDE + 12] != 0


def write_pass(
    output: bytearray,
    offset: int,
    texture_id: int,
    effect: int,
    blend: int,
    depth_write: bool,
) -> None:
    flags = (1 if depth_write else 0) | 2
    struct.pack_into("<HBBBB", output, offset, texture_id, effect, COLOR_RGBA, blend, flags)
    struct.pack_into("<I", output, offset + 8, 0xFFFFFFFF)


def upgrade(blob: bytes) -> bytes:
    textures, materials, texture_table, material_table, pev_table = validate_v2(blob)
    output = bytearray(blob)
    plan_table = align(len(output))
    output.extend(b"\0" * (plan_table - len(output) + materials * MPV_STRIDE))
    output[84:88] = MPV_MAGIC
    struct.pack_into("<IIII", output, 88, plan_table, MPV_STRIDE, materials, 1)
    struct.pack_into("<H", output, 4, 3)

    for material_id in range(materials):
        material = material_table + material_id * MATERIAL_STRIDE
        pev = pev_table + material_id * PEV_STRIDE
        plan = plan_table + material_id * MPV_STRIDE
        primary = u16(blob, material)
        material_class = blob[pev + 2]
        depth_write = blob[pev + 6] != 0
        blend_mode = blob[pev + 13]
        blend_src = blob[pev + 14]
        texture_count = min(blob[pev + 17], 8)
        identities_complete = blob[pev + 18] != 0
        texture_ids = [u16(blob, pev + 20 + index * 2) for index in range(texture_count)]
        texture_ids = [value for value in texture_ids if value != 0xFFFF]
        if not texture_ids:
            texture_ids = [primary]

        missing_tev_alpha = (
            material_class == ALPHA_BLEND_CLASS
            and blend_mode == GX_BLEND
            and blend_src == GX_SRC_ALPHA
            and len(texture_ids) == 1
            and identities_complete
            and not texture_has_alpha(blob, texture_table, texture_ids[0])
        )

        if missing_tev_alpha:
            fidelity, reason = FIDELITY_UNSUPPORTED, REASON_UNSUPPORTED_TEV
            pass_ids = texture_ids[:1]
        elif len(texture_ids) <= 1:
            fidelity, reason = FIDELITY_EXACT, REASON_NONE
            pass_ids = texture_ids[:1]
        elif len(texture_ids) == 2:
            fidelity, reason = FIDELITY_APPROXIMATE, REASON_UNSUPPORTED_TEV
            pass_ids = texture_ids[:2]
        else:
            fidelity, reason = FIDELITY_UNSUPPORTED, REASON_TOO_MANY_STAGES
            pass_ids = texture_ids[:2]

        struct.pack_into("<HBBBB", output, plan, material_id, fidelity, reason, len(pass_ids), 0)
        write_pass(output, plan + 8, pass_ids[0], EFFECT_MODULATE, BLEND_SOURCE, depth_write)
        if len(pass_ids) > 1:
            if material_class == MULTIPLY_CLASS:
                effect, blend = EFFECT_REPLACE, BLEND_MULTIPLY
            elif blend_mode == GX_BLEND and blend_src == GX_SRC_ALPHA:
                # GX TEV can synthesize alpha before an additive PE blend. The
                # PSP has no equivalent combiner here; a second SRC_ALPHA pass
                # keeps the recovered F_SP108 foam translucent instead of
                # saturating it into a white sheet.
                effect, blend = EFFECT_MODULATE, BLEND_ALPHA
            else:
                effect, blend = EFFECT_REPLACE, BLEND_MULTIPLY
            write_pass(output, plan + 8 + PASS_STRIDE, pass_ids[1], effect, blend, False)

    struct.pack_into("<I", output, 8, len(output))
    struct.pack_into("<I", output, 12, crc32_package(output))
    return bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    source = args.source.read_bytes()
    converted = upgrade(source)
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    args.destination.write_bytes(converted)
    print(
        f"DPTX_MPV1_UPGRADE_OK source_bytes={len(source)} output_bytes={len(converted)} "
        f"materials={u32(converted, 20)} plans={u32(converted, 96)} crc={u32(converted, 12):08x}"
    )


if __name__ == "__main__":
    main()
