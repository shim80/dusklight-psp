#!/usr/bin/env python3
"""Export Twilight Princess demo38_01 JStudio camera data to Dusklight DPCM v1.

The tool contains no game data. It accepts one of:
  * demo38_01.stb
  * Demo38_01.arc (RARC or Yaz0-wrapped RARC)
  * an uncompressed GameCube disc image containing Demo38_01.arc
"""
from __future__ import annotations

import argparse
import bisect
import hashlib
import math
from pathlib import Path
import struct
import zlib

SOURCE_FPS = 30
EXPECTED_SOURCE_FRAMES = 2400
DPCM_HEADER_BYTES = 64
DPCM_SAMPLE_BYTES = 32


def be_u16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def be_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def align4(value: int) -> int:
    return (value + 3) & ~3


def yaz0_decompress(source: bytes) -> bytes:
    if source[:4] != b"Yaz0" or len(source) < 16:
        raise ValueError("not a Yaz0 stream")
    output_size = be_u32(source, 4)
    output = bytearray()
    position = 16
    code = 0
    remaining_bits = 0
    while len(output) < output_size:
        if remaining_bits == 0:
            if position >= len(source):
                raise ValueError("truncated Yaz0 control stream")
            code = source[position]
            position += 1
            remaining_bits = 8
        if code & 0x80:
            if position >= len(source):
                raise ValueError("truncated Yaz0 literal")
            output.append(source[position])
            position += 1
        else:
            if position + 2 > len(source):
                raise ValueError("truncated Yaz0 copy command")
            first = source[position]
            second = source[position + 1]
            position += 2
            distance = ((first & 0x0F) << 8) | second
            copy_position = len(output) - distance - 1
            if copy_position < 0:
                raise ValueError("invalid Yaz0 copy distance")
            length = first >> 4
            if length == 0:
                if position >= len(source):
                    raise ValueError("truncated Yaz0 long copy")
                length = source[position] + 0x12
                position += 1
            else:
                length += 2
            for _ in range(length):
                output.append(output[copy_position])
                copy_position += 1
                if len(output) == output_size:
                    break
        code = (code << 1) & 0xFF
        remaining_bits -= 1
    return bytes(output)


def extract_rarc_named_file(rarc: bytes, wanted_name: str) -> bytes:
    if rarc[:4] != b"RARC" or len(rarc) < 0x40:
        raise ValueError("not a RARC archive")
    file_size = be_u32(rarc, 4)
    if file_size > len(rarc):
        raise ValueError("truncated RARC archive")
    info = 0x20
    file_count = be_u32(rarc, info + 8)
    entry_offset = 0x20 + be_u32(rarc, info + 12)
    string_size = be_u32(rarc, info + 16)
    string_offset = 0x20 + be_u32(rarc, info + 20)
    data_offset = 0x20 + be_u32(rarc, 0x0C)
    if string_offset + string_size > len(rarc):
        raise ValueError("invalid RARC string table")
    for index in range(file_count):
        entry = entry_offset + index * 20
        if entry + 20 > len(rarc):
            raise ValueError("truncated RARC file table")
        entry_type = be_u16(rarc, entry + 4)
        name_offset = be_u16(rarc, entry + 6)
        start = string_offset + name_offset
        end = rarc.find(b"\0", start, string_offset + string_size)
        if end < 0:
            raise ValueError("unterminated RARC file name")
        name = rarc[start:end].decode("ascii", "replace")
        if entry_type & 0x0200:
            continue
        if name != wanted_name:
            continue
        relative = be_u32(rarc, entry + 8)
        size = be_u32(rarc, entry + 12)
        absolute = data_offset + relative
        if absolute + size > len(rarc):
            raise ValueError("truncated RARC file payload")
        return rarc[absolute:absolute + size]
    raise ValueError(f"RARC does not contain {wanted_name}")


def extract_disc_named_file(image: bytes, wanted_name: str) -> bytes:
    if len(image) < 0x42C:
        raise ValueError("disc image header is truncated")
    fst_offset = be_u32(image, 0x424)
    fst_size = be_u32(image, 0x428)
    if fst_offset + fst_size > len(image):
        raise ValueError("disc FST is outside the image")
    fst = image[fst_offset:fst_offset + fst_size]
    if len(fst) < 12:
        raise ValueError("disc FST is truncated")
    entry_count = be_u32(fst, 8)
    entry_bytes = entry_count * 12
    if entry_bytes > len(fst):
        raise ValueError("disc FST entry table is truncated")
    for index in range(1, entry_count):
        entry = index * 12
        word = be_u32(fst, entry)
        if word >> 24:
            continue
        name_offset = word & 0x00FFFFFF
        name_start = entry_bytes + name_offset
        if name_start >= len(fst):
            raise ValueError("disc FST name offset is invalid")
        name_end = fst.find(b"\0", name_start)
        if name_end < 0:
            raise ValueError("unterminated disc FST name")
        name = fst[name_start:name_end].decode("ascii", "replace")
        if name != wanted_name:
            continue
        offset = be_u32(fst, entry + 4)
        size = be_u32(fst, entry + 8)
        if offset + size > len(image):
            raise ValueError("disc file is outside the image")
        return image[offset:offset + size]
    raise ValueError(f"disc image does not contain {wanted_name}")


def load_demo_stb(source_path: Path) -> bytes:
    source = source_path.read_bytes()
    if source[:4] == b"STB\0":
        return source
    if source[:4] == b"Yaz0":
        source = yaz0_decompress(source)
    if source[:4] == b"RARC":
        return extract_rarc_named_file(source, "demo38_01.stb")
    archive = extract_disc_named_file(source, "Demo38_01.arc")
    if archive[:4] == b"Yaz0":
        archive = yaz0_decompress(archive)
    return extract_rarc_named_file(archive, "demo38_01.stb")


def paragraph(stb: bytes, offset: int) -> tuple[int, int, int, int]:
    word = be_u16(stb, offset)
    if word & 0x8000:
        size = ((word & 0x7FFF) << 16) | be_u16(stb, offset + 2)
        kind = be_u32(stb, offset + 4)
        header = 8
    else:
        size = word
        kind = be_u16(stb, offset + 2)
        header = 4
    content = offset + header
    return size, kind, content, content + align4(size)


class FunctionValueListParameter:
    def __init__(self, pairs: list[tuple[float, float]], interpolation: int):
        if not pairs:
            raise ValueError("empty FVB list-parameter function")
        self.pairs = pairs
        self.interpolation = interpolation
        self.parameters = [pair[0] for pair in pairs]

    @staticmethod
    def bspline_nonuniform(
        parameter: float, control: list[float], knots: list[float]
    ) -> float:
        k0, k1, k2, k3, k4, k5 = knots
        diff0 = parameter - k0
        diff1 = parameter - k1
        diff2 = parameter - k2
        diff3 = k3 - parameter
        diff4 = k4 - parameter
        diff5 = k5 - parameter
        inverse = 1.0 / (k3 - k2)
        blend3 = (diff3 * inverse) / (k3 - k1)
        blend2 = (diff2 * inverse) / (k4 - k2)
        blend1 = (diff3 * blend3) / (k3 - k0)
        blend4 = ((diff1 * blend3) + (diff4 * blend2)) / (k4 - k1)
        blend5 = (diff2 * blend2) / (k5 - k2)
        term1 = diff3 * blend1
        term2 = (diff0 * blend1) + (diff4 * blend4)
        term3 = (diff1 * blend4) + (diff5 * blend5)
        term4 = diff2 * blend5
        return (
            term1 * control[0]
            + term2 * control[1]
            + term3 * control[2]
            + term4 * control[3]
        )

    def value(self, parameter: float) -> float:
        pairs = self.pairs
        if parameter <= pairs[0][0]:
            return pairs[0][1]
        if parameter >= pairs[-1][0]:
            return pairs[-1][1]
        upper = bisect.bisect_right(self.parameters, parameter)
        if self.interpolation == 0:
            return pairs[upper - 1][1]
        if self.interpolation == 1 or (
            self.interpolation == 3 and len(pairs) == 2
        ):
            p0, v0 = pairs[upper - 1]
            p1, v1 = pairs[upper]
            return v0 + (v1 - v0) * (parameter - p0) / (p1 - p0)
        if self.interpolation == 2:
            p0, v0 = pairs[upper - 1]
            p1, v1 = pairs[upper]
            ratio = (parameter - p0) / (p1 - p0)
            blend = (3.0 - 2.0 * ratio) * ratio * ratio
            return (1.0 - blend) * v0 + blend * v1
        if self.interpolation != 3:
            raise ValueError(f"unsupported FVB interpolation {self.interpolation}")

        index = upper
        flat_index = index * 2
        remaining = (len(pairs) - index) * 2
        control = [0.0, pairs[index - 1][1], pairs[index][1], 0.0]
        knots = [0.0, 0.0, pairs[index - 1][0], pairs[index][0], 0.0, 0.0]
        if flat_index == 2:
            control[0] = 2.0 * control[1] - control[2]
            control[3] = pairs[index + 1][1]
            knots[4] = pairs[index + 1][0]
            knots[1] = 2.0 * knots[2] - knots[3]
            knots[0] = 2.0 * knots[2] - knots[4]
            if remaining == 2:
                raise ValueError("invalid first FVB B-spline interval")
            if remaining == 4:
                knots[5] = 2.0 * knots[4] - knots[3]
            else:
                knots[5] = pairs[index + 2][0]
        elif flat_index == 4:
            control[0] = pairs[index - 2][1]
            knots[1] = pairs[index - 2][0]
            knots[0] = 2.0 * knots[1] - knots[2]
            if remaining == 2:
                control[3] = 2.0 * control[2] - control[1]
                knots[4] = 2.0 * knots[3] - knots[2]
                knots[5] = 2.0 * knots[3] - knots[1]
            elif remaining == 4:
                control[3] = pairs[index + 1][1]
                knots[4] = pairs[index + 1][0]
                knots[5] = 2.0 * knots[4] - knots[3]
            else:
                control[3] = pairs[index + 1][1]
                knots[4] = pairs[index + 1][0]
                knots[5] = pairs[index + 2][0]
        else:
            control[0] = pairs[index - 2][1]
            knots[1] = pairs[index - 2][0]
            knots[0] = pairs[index - 3][0]
            if remaining == 2:
                control[3] = 2.0 * control[2] - control[1]
                knots[4] = 2.0 * knots[3] - knots[2]
                knots[5] = 2.0 * knots[3] - knots[1]
            elif remaining == 4:
                control[3] = pairs[index + 1][1]
                knots[4] = pairs[index + 1][0]
                knots[5] = 2.0 * knots[4] - knots[3]
            else:
                control[3] = pairs[index + 1][1]
                knots[4] = pairs[index + 1][0]
                knots[5] = pairs[index + 2][0]
        return self.bspline_nonuniform(parameter, control, knots)


def parse_fvb_functions(stb: bytes) -> list[FunctionValueListParameter | None]:
    block = 0x20
    if stb[block + 4:block + 8] != b"JFVB":
        raise ValueError("first demo38_01 STB block is not JFVB")
    fvb = block + 8
    if stb[fvb:fvb + 4] != b"FVB\0" or be_u16(stb, fvb + 4) != 0xFEFF:
        raise ValueError("invalid embedded FVB header")
    function_count = be_u32(stb, fvb + 12)
    offset = fvb + 16
    functions: list[FunctionValueListParameter | None] = []
    for _ in range(function_count):
        size = be_u32(stb, offset)
        function_type = be_u16(stb, offset + 4)
        id_size = be_u16(stb, offset + 6)
        cursor = offset + 8 + align4(id_size)
        end = offset + size
        interpolation = 0
        pairs: list[tuple[float, float]] | None = None
        while cursor < end:
            payload_size, kind, content, next_offset = paragraph(stb, cursor)
            if kind == 0x16 and payload_size == 4:
                interpolation = be_u32(stb, content)
            elif kind == 1 and function_type == 5:
                count = be_u32(stb, content)
                values = struct.unpack_from(
                    ">" + "f" * (count * 2), stb, content + 4
                )
                pairs = list(zip(values[0::2], values[1::2]))
            if payload_size == 0:
                break
            cursor = next_offset
        if function_type == 5:
            if pairs is None:
                raise ValueError("FVB list-parameter function has no data")
            functions.append(FunctionValueListParameter(pairs, interpolation))
        else:
            functions.append(None)
        offset += size
    return functions


def parse_camera_segments(stb: bytes):
    block_count = be_u32(stb, 0x0C)
    offset = 0x20
    camera_block = None
    for _ in range(block_count):
        size = be_u32(stb, offset)
        if stb[offset + 4:offset + 8] == b"JCMR":
            camera_block = (offset, size)
            break
        offset += size
    if camera_block is None:
        raise ValueError("demo38_01 STB has no JCMR block")
    block, size = camera_block
    id_size = be_u16(stb, block + 10)
    cursor = block + 12 + align4(id_size)
    end = block + size
    segments = []
    while cursor < end:
        payload_size, kind, content, next_offset = paragraph(stb, cursor)
        if payload_size == 0:
            break
        if kind != 0x404F2 or payload_size != 48:
            raise ValueError(f"unexpected JCMR paragraph {kind:#x}/{payload_size}")
        fov = be_u32(stb, content)
        if be_u32(stb, content + 4) != 0x404D2:
            raise ValueError("unexpected JCMR roll operation")
        roll = be_u32(stb, content + 8)
        if be_u32(stb, content + 12) != 0x0C0312:
            raise ValueError("unexpected JCMR eye operation")
        eye = tuple(be_u32(stb, content + offset) for offset in (16, 20, 24))
        if be_u32(stb, content + 28) != 0x0C0392:
            raise ValueError("unexpected JCMR target operation")
        target = tuple(be_u32(stb, content + offset) for offset in (32, 36, 40))
        sequence = be_u32(stb, content + 44)
        if sequence >> 24 != 2:
            raise ValueError("unexpected JCMR wait sequence")
        segments.append((sequence & 0x00FFFFFF, fov, roll, eye, target))
        cursor = next_offset
    if len(segments) != 7 or sum(segment[0] for segment in segments) != EXPECTED_SOURCE_FRAMES:
        raise ValueError("demo38_01 camera duration does not match source contract")
    return segments


def export_dpcm(stb: bytes) -> bytes:
    if stb[:4] != b"STB\0" or be_u16(stb, 4) != 0xFEFF:
        raise ValueError("invalid STB header")
    if be_u32(stb, 8) != len(stb):
        raise ValueError("STB size field does not match input")
    functions = parse_fvb_functions(stb)
    segments = parse_camera_segments(stb)
    samples = []
    for global_frame in range(EXPECTED_SOURCE_FRAMES + 1):
        if global_frame == EXPECTED_SOURCE_FRAMES:
            segment_index = len(segments) - 1
            local_frame = segments[-1][0]
        else:
            elapsed = 0
            for segment_index, segment in enumerate(segments):
                if global_frame < elapsed + segment[0]:
                    local_frame = global_frame - elapsed
                    break
                elapsed += segment[0]
        _, fov_index, roll_index, eye_indices, target_indices = segments[segment_index]
        time_seconds = local_frame / float(SOURCE_FPS)
        values = []
        for index in (*eye_indices, *target_indices, fov_index, roll_index):
            if index >= len(functions) or functions[index] is None:
                raise ValueError(f"camera references unsupported FVB function {index}")
            values.append(float(functions[index].value(time_seconds)))
        if not all(math.isfinite(value) for value in values):
            raise ValueError(f"non-finite camera sample at source frame {global_frame}")
        samples.append(values)

    total_size = DPCM_HEADER_BYTES + len(samples) * DPCM_SAMPLE_BYTES
    output = bytearray(DPCM_HEADER_BYTES)
    output[:4] = b"DPCM"
    struct.pack_into("<HHI", output, 4, 1, DPCM_HEADER_BYTES, total_size)
    struct.pack_into(
        "<IIIIII", output, 16, SOURCE_FPS, EXPECTED_SOURCE_FRAMES,
        len(samples), DPCM_SAMPLE_BYTES, DPCM_HEADER_BYTES, 1,
    )
    for sample in samples:
        output += struct.pack("<8f", *sample)
    checksum_view = bytearray(output)
    checksum_view[12:16] = b"\0\0\0\0"
    checksum = zlib.crc32(checksum_view) & 0xFFFFFFFF
    struct.pack_into("<I", output, 12, checksum)
    return bytes(output)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    dpcm = export_dpcm(load_demo_stb(args.source))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(dpcm)
    print(
        "DUSKLIGHT_STARTUP_CAMERA_EXPORT_OK "
        f"source=demo38_01 fps={SOURCE_FPS} frames={EXPECTED_SOURCE_FRAMES} "
        f"samples={EXPECTED_SOURCE_FRAMES + 1} bytes={len(dpcm)} "
        f"sha256={hashlib.sha256(dpcm).hexdigest()}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
