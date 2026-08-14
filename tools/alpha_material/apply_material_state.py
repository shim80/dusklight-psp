#!/usr/bin/env python3
"""Attach source-proven J3D PE material state to room DPTX and buckets to DPRM.

The DPTX v2 extension is append-only: existing texture/material/pixel offsets stay
unchanged. Header bytes 64..83 describe a PEV1 table of 40-byte material-state
records. DPRM remains v1 and only its semantic bucket byte is updated.
"""
from __future__ import annotations

import argparse
import binascii
import json
import struct
from pathlib import Path

PE_MAGIC = b"PEV1"
PE_STRIDE = 40
CLASS_IDS = {
    "OPAQUE": 0,
    "ALPHA_TEST": 1,
    "ALPHA_BLEND": 2,
    "ADDITIVE": 3,
    "MULTIPLY": 4,
    "UNSUPPORTED_COMPLEX": 5,
}
DRAW_BUFFER_IDS = {"OPAQUE": 0, "XLU": 1}


def u16(data: bytes | bytearray, off: int) -> int:
    return struct.unpack_from("<H", data, off)[0]


def u32(data: bytes | bytearray, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]


def p16(data: bytearray, off: int, value: int) -> None:
    struct.pack_into("<H", data, off, value)


def p32(data: bytearray, off: int, value: int) -> None:
    struct.pack_into("<I", data, off, value)


def crc32_package(data: bytes | bytearray) -> int:
    tmp = bytearray(data)
    tmp[12:16] = b"\0\0\0\0"
    return binascii.crc32(tmp) & 0xFFFFFFFF


def validate_base(data: bytes, magic: bytes) -> None:
    if len(data) < 16 or data[:4] != magic:
        raise ValueError(f"invalid {magic.decode()} magic")
    if u32(data, 8) != len(data):
        raise ValueError(f"invalid {magic.decode()} size")
    if u32(data, 12) != crc32_package(data):
        raise ValueError(f"invalid {magic.decode()} CRC")


def semantic_bucket(class_name: str) -> int:
    if class_name == "OPAQUE":
        return 0
    if class_name == "ALPHA_TEST":
        return 1
    return 2


def load_manifest(path: Path) -> dict:
    doc = json.loads(path.read_text())
    if doc.get("schema") != "dusklight.psp.alpha-material-state.v1":
        raise ValueError("unsupported material state manifest schema")
    materials = doc.get("materials")
    if not isinstance(materials, list) or len(materials) != doc.get("material_count"):
        raise ValueError("manifest material count mismatch")
    if [m.get("id") for m in materials] != list(range(len(materials))):
        raise ValueError("manifest material IDs must be dense and ordered")
    for m in materials:
        if m.get("class") not in CLASS_IDS:
            raise ValueError(f"invalid class for material {m['id']}")
        if m.get("draw_buffer") not in DRAW_BUFFER_IDS:
            raise ValueError(f"invalid draw buffer for material {m['id']}")
        z, alpha, blend = m.get("z"), m.get("alpha"), m.get("blend")
        if not (isinstance(z, list) and len(z) == 3 and z[0] in (0, 1) and 0 <= z[1] <= 7 and z[2] in (0, 1)):
            raise ValueError(f"invalid Z state for material {m['id']}")
        if not (isinstance(alpha, list) and len(alpha) == 5 and 0 <= alpha[0] <= 7 and 0 <= alpha[1] <= 255 and 0 <= alpha[2] <= 3 and 0 <= alpha[3] <= 7 and 0 <= alpha[4] <= 255):
            raise ValueError(f"invalid alpha state for material {m['id']}")
        if not (isinstance(blend, list) and len(blend) == 4 and all(isinstance(v, int) and 0 <= v <= 255 for v in blend)):
            raise ValueError(f"invalid blend state for material {m['id']}")
        if not isinstance(m.get("cull"), int) or not 0 <= m["cull"] <= 3:
            raise ValueError(f"invalid cull state for material {m['id']}")
        textures = m.get("textures", [])
        if not isinstance(textures, list) or len(textures) > 8 or any(not isinstance(v, int) or v < 0 or v > 0xFFFE for v in textures):
            raise ValueError(f"invalid texture identities for material {m['id']}")
        if not isinstance(m.get("texture_count"), int) or not 0 <= m["texture_count"] <= 8:
            raise ValueError(f"invalid source texture count for material {m['id']}")
        if m.get("texture_identities_complete") and len(textures) != m["texture_count"]:
            raise ValueError(f"complete texture identities mismatch for material {m['id']}")
        if m["class"] != "OPAQUE" and not m.get("texture_identities_complete"):
            raise ValueError(f"non-opaque material {m['id']} must have complete texture identities")
    return doc


def dprm_submesh_layout(data: bytes) -> tuple[int, int]:
    validate_base(data, b"DPRM")
    if u16(data, 4) != 1 or u16(data, 6) != 256:
        raise ValueError("unsupported DPRM layout")
    count = u32(data, 32)
    table = u32(data, 72)
    section = table + 2 * 32
    if section + 32 > len(data) or u32(data, section) != 3 or u32(data, section + 16) != 48:
        raise ValueError("invalid DPRM submesh section")
    off = u32(data, section + 4)
    if off + count * 48 > len(data):
        raise ValueError("DPRM submesh table out of range")
    return off, count


def dptx_layout(data: bytes) -> tuple[int, int, int, int, int]:
    validate_base(data, b"DPTX")
    if u16(data, 6) != 128:
        raise ValueError("unsupported DPTX header size")
    version = u16(data, 4)
    if version not in (1, 2):
        raise ValueError(f"unsupported DPTX version {version}")
    textures = u32(data, 16)
    materials = u32(data, 20)
    material_off = u32(data, 32)
    material_stride = u32(data, 36)
    if material_stride != 32 or material_off + materials * material_stride > len(data):
        raise ValueError("invalid DPTX material table")
    return version, textures, materials, material_off, material_stride


def build_record(m: dict, texture_limit: int) -> bytes:
    rec = bytearray(PE_STRIDE)
    p16(rec, 0, m["id"])
    rec[2] = CLASS_IDS[m["class"]]
    rec[3] = DRAW_BUFFER_IDS[m["draw_buffer"]]
    rec[4:7] = bytes(m["z"])
    rec[7] = m["cull"]
    rec[8:13] = bytes(m["alpha"])
    rec[13:17] = bytes(m["blend"])
    rec[17] = m["texture_count"]
    rec[18] = 1 if m.get("texture_identities_complete") else 0
    rec[19] = 1
    ids = list(m.get("textures", []))
    if any(v >= texture_limit for v in ids):
        raise ValueError(f"material {m['id']} texture identity out of DPTX range")
    for index in range(8):
        p16(rec, 20 + index * 2, ids[index] if index < len(ids) else 0xFFFF)
    return bytes(rec)


def patch(dprm_path: Path, dptx_path: Path, manifest_path: Path) -> dict:
    doc = load_manifest(manifest_path)
    dprm = bytearray(dprm_path.read_bytes())
    dptx = bytearray(dptx_path.read_bytes())
    submesh_off, submesh_count = dprm_submesh_layout(dprm)
    version, texture_count, material_count, material_off, material_stride = dptx_layout(dptx)
    if material_count != doc["material_count"]:
        raise ValueError(f"DPTX material count {material_count} != manifest {doc['material_count']}")
    if submesh_count != material_count:
        raise ValueError("V4C F_SP108 contract requires one source material per submesh")

    records = b"".join(build_record(m, texture_count) for m in doc["materials"])
    semantic = [semantic_bucket(m["class"]) for m in doc["materials"]]

    for m in doc["materials"]:
        item = material_off + m["id"] * material_stride
        primary = u16(dptx, item)
        if m["textures"] and primary != m["textures"][0]:
            raise ValueError(f"material {m['id']} primary texture mismatch DPTX={primary} source={m['textures'][0]}")
        source_count = dptx[item + 16]
        if source_count != m["texture_count"]:
            raise ValueError(f"material {m['id']} source texture count mismatch DPTX={source_count} source={m['texture_count']}")

    seen = set()
    for index in range(submesh_count):
        item = submesh_off + index * 48
        material_id = u16(dprm, item + 16)
        if material_id >= material_count or material_id in seen:
            raise ValueError("DPRM source material identity is missing or duplicated")
        seen.add(material_id)
        state = doc["materials"][material_id]
        expected_depth = (1 if state["z"][0] else 0) | (2 if state["z"][2] else 0)
        if dprm[item + 18] != expected_depth or dprm[item + 19] != state["z"][1]:
            raise ValueError(f"DPRM depth state mismatch for source material {material_id}")

    if version == 2:
        if bytes(dptx[64:68]) != PE_MAGIC or u32(dptx, 72) != PE_STRIDE or u32(dptx, 76) != material_count:
            raise ValueError("unexpected existing DPTX v2 PE extension")
        table = u32(dptx, 68)
        if table + len(records) > len(dptx) or bytes(dptx[table:table + len(records)]) != records:
            raise ValueError("existing DPTX v2 PE state differs from requested source state")
    else:
        pad = (-len(dptx)) & 15
        dptx.extend(b"\0" * pad)
        table = len(dptx)
        dptx.extend(records)
        p16(dptx, 4, 2)
        dptx[64:68] = PE_MAGIC
        p32(dptx, 68, table)
        p32(dptx, 72, PE_STRIDE)
        p32(dptx, 76, material_count)
        p32(dptx, 80, 1)

    for material_id, bucket in enumerate(semantic):
        dptx[material_off + material_id * material_stride + 2] = bucket
    for index in range(submesh_count):
        item = submesh_off + index * 48
        material_id = u16(dprm, item + 16)
        dprm[item + 12] = semantic[material_id]

    p32(dptx, 8, len(dptx))
    p32(dptx, 12, 0)
    p32(dptx, 12, crc32_package(dptx))
    p32(dprm, 12, 0)
    p32(dprm, 12, crc32_package(dprm))
    dptx_path.write_bytes(dptx)
    dprm_path.write_bytes(dprm)

    counts = {name: 0 for name in CLASS_IDS}
    for m in doc["materials"]:
        counts[m["class"]] += 1
    return {
        "materials": material_count,
        "opaque": counts["OPAQUE"],
        "alpha_test": counts["ALPHA_TEST"],
        "alpha_blend": counts["ALPHA_BLEND"],
        "multiply": counts["MULTIPLY"],
        "complex": counts["UNSUPPORTED_COMPLEX"],
        "dptx_version": u16(dptx, 4),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--dprm", required=True, type=Path)
    parser.add_argument("--dptx", required=True, type=Path)
    args = parser.parse_args()
    result = patch(args.dprm, args.dptx, args.manifest)
    print("ALPHA_MATERIAL_STATE_PATCH_OK " + " ".join(f"{k}={v}" for k, v in result.items()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
