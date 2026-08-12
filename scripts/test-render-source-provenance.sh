#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

ROOM="$(assert_project_path "build/assets/dusklight-psp/data/stages/F_SP108/R01/room.dprm")"
ROOM_TEXTURES="$(assert_project_path "build/assets/dusklight-psp/data/stages/F_SP108/R01/room.dptx")"
LINK="$(assert_project_path "build/assets/dusklight-psp/data/common/link.dpsk")"

python3 - "$ROOM" "$ROOM_TEXTURES" "$LINK" <<'PY'
import pathlib
import struct
import sys


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def records_dprm(path):
    data = path.read_bytes()
    if data[:4] != b"DPRM" or u32(data, 16) < 3:
        raise SystemExit(f"invalid DPRM: {path}")
    count = u32(data, 32)
    table = u32(data, 72)
    section = table + 2 * 32
    if u32(data, section) != 3 or u32(data, section + 16) != 48:
        raise SystemExit(f"invalid DPRM submesh section: {path}")
    offset = u32(data, section + 4)
    return [(data[offset + i * 48 + 12],) +
            struct.unpack_from("<HH", data, offset + i * 48 + 14) +
            (data[offset + i * 48 + 18], data[offset + i * 48 + 19])
            for i in range(count)]


def material_buckets_dptx(path):
    data = path.read_bytes()
    if data[:4] != b"DPTX":
        raise SystemExit(f"invalid DPTX: {path}")
    count = u32(data, 20)
    offset = u32(data, 32)
    stride = u32(data, 36)
    if stride < 3:
        raise SystemExit(f"invalid DPTX material table: {path}")
    return [data[offset + index * stride + 2] for index in range(count)]


def pairs_dpsk(path):
    data = path.read_bytes()
    if data[:4] != b"DPSK":
        raise SystemExit(f"invalid DPSK: {path}")
    count = u32(data, 28)
    offset = u32(data, 84)
    stride = u32(data, 88)
    if stride < 24:
        raise SystemExit(f"DPSK submesh records lack provenance: {path}")
    return [struct.unpack_from("<HH", data, offset + i * stride + 20)
            for i in range(count)]


def validate(label, pairs, expected):
    if len(pairs) != expected:
        raise SystemExit(f"{label}: expected {expected} records, got {len(pairs)}")
    if len(set(pairs)) != expected:
        raise SystemExit(f"{label}: source shape/material identities collide")
    materials = {material for shape, material in pairs}
    shapes = {shape for shape, material in pairs}
    if len(materials) < 2 or len(shapes) < 2:
        raise SystemExit(f"{label}: source provenance was flattened")
    print(f"{label}_SOURCE_PROVENANCE_OK records={expected} "
          f"shapes={len(shapes)} materials={len(materials)}")


room_records = records_dprm(pathlib.Path(sys.argv[1]))
room_pairs = [(shape, material)
              for bucket, shape, material, depth, depth_func in room_records]
validate("F_SP108_ROOM", room_pairs, 22)
material_buckets = material_buckets_dptx(pathlib.Path(sys.argv[2]))
depth_states = set()
for bucket, shape, material, depth, depth_func in room_records:
    if material >= len(material_buckets) or bucket != material_buckets[material]:
        raise SystemExit(
            f"F_SP108_ROOM: source material bucket mismatch "
            f"shape={shape} material={material} DPRM={bucket}")
    if depth & ~3 or depth_func > 7:
        raise SystemExit(f"F_SP108_ROOM: invalid depth record shape={shape}")
    depth_states.add((depth, depth_func))
if len(depth_states) < 2:
    raise SystemExit("F_SP108_ROOM: source depth state was flattened")
print("F_SP108_ROOM_SOURCE_BUCKETS_OK records=22")
print(f"F_SP108_ROOM_SOURCE_DEPTH_OK states={len(depth_states)}")
validate("LINK", pairs_dpsk(pathlib.Path(sys.argv[3])), 27)
PY

printf '%s\n' "RENDER_SOURCE_PROVENANCE_SUITE_OK"
