#!/usr/bin/env python3
import argparse
import binascii
import struct
import sys
from pathlib import Path

TARGET_MATERIALS = {7, 8, 9, 10, 11}
EXPECTED_MATERIAL_COUNT = 22
EXPECTED_TEXTURE_COUNT = 18
DPRM_HEADER_SIZE = 256
SECTION_STRIDE = 32
SUBMESH_STRIDE = 48
BUCKET_ALPHA_TEST = 1


def u16(data: bytearray, offset: int) -> int:
    return struct.unpack_from('<H', data, offset)[0]


def u32(data: bytearray, offset: int) -> int:
    return struct.unpack_from('<I', data, offset)[0]


def validate_range(data: bytearray, offset: int, count: int, stride: int) -> None:
    if offset < 0 or count < 0 or stride <= 0 or offset + count * stride > len(data):
        raise ValueError('DPRM section range is invalid')


def package_crc32(data: bytearray) -> int:
    scratch = bytearray(data)
    scratch[12:16] = b'\x00\x00\x00\x00'
    return binascii.crc32(scratch) & 0xFFFFFFFF


def patch(path: Path) -> tuple[int, int]:
    data = bytearray(path.read_bytes())
    if len(data) < DPRM_HEADER_SIZE:
        raise ValueError('DPRM is truncated')
    if data[:4] != b'DPRM':
        raise ValueError('not a DPRM package')
    if u16(data, 4) != 1 or u16(data, 6) != DPRM_HEADER_SIZE:
        raise ValueError('unsupported DPRM version/header')
    if u32(data, 8) != len(data):
        raise ValueError('DPRM size field does not match file size')
    if u32(data, 16) != 4:
        raise ValueError('unexpected DPRM section count')
    if u32(data, 36) != EXPECTED_MATERIAL_COUNT or u32(data, 40) != EXPECTED_TEXTURE_COUNT:
        raise ValueError('package is not the audited F_SP108/R01 model')

    section_table = u32(data, 72)
    validate_range(data, section_table, 4, SECTION_STRIDE)
    submesh_section = section_table + 2 * SECTION_STRIDE
    if u32(data, submesh_section) != 3 or u32(data, submesh_section + 16) != SUBMESH_STRIDE:
        raise ValueError('unexpected DPRM submesh section')
    submesh_offset = u32(data, submesh_section + 4)
    submesh_count = u32(data, submesh_section + 12)
    validate_range(data, submesh_offset, submesh_count, SUBMESH_STRIDE)

    found = set()
    changed = 0
    for index in range(submesh_count):
        item = submesh_offset + index * SUBMESH_STRIDE
        material_id = u16(data, item + 16)
        if material_id not in TARGET_MATERIALS:
            continue
        found.add(material_id)
        bucket = data[item + 12]
        if bucket not in (0, BUCKET_ALPHA_TEST):
            raise ValueError(
                f'material {material_id} has unexpected bucket {bucket}')
        if bucket != BUCKET_ALPHA_TEST:
            data[item + 12] = BUCKET_ALPHA_TEST
            changed += 1

    if found != TARGET_MATERIALS:
        missing = ','.join(str(value) for value in sorted(TARGET_MATERIALS - found))
        raise ValueError(f'audited F_SP108 alpha-test materials missing: {missing}')

    struct.pack_into('<I', data, 12, 0)
    struct.pack_into('<I', data, 12, package_crc32(data))
    path.write_bytes(data)
    return changed, submesh_count


def main() -> int:
    parser = argparse.ArgumentParser(
        description='Restore audited F_SP108 foliage materials to DPRM alpha-test bucket.')
    parser.add_argument('dprm', type=Path)
    args = parser.parse_args()
    try:
        changed, total = patch(args.dprm)
    except (OSError, ValueError, struct.error) as exc:
        print(f'FSP108_ALPHA_TEST_PATCH_ERROR: {exc}', file=sys.stderr)
        return 1
    print(
        'FSP108_ALPHA_TEST_PATCH_OK '
        f'changed_submeshes={changed} total_submeshes={total} materials=7,8,9,10,11')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
