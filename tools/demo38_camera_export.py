#!/usr/bin/env python3
"""Export the source JStudio demo38_01 camera as a PSP DPCM track.

The input is the unmodified ``demo38_01.stb`` resource from the legally
provided GZ2P01 revision-0 image.  No source data is embedded in this tool.
"""

from __future__ import annotations

import argparse
import bisect
import hashlib
import math
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path


EXPECTED_STB_SHA256 = (
    "e335d6d44c002dd25881aedd2f053a226be18cdd254d2049e0d78f2aa88b735d"
)
SOURCE_FPS = 30
EXPECTED_SOURCE_FRAMES = 2400
EXPECTED_SAMPLE_COUNT = 2401
DPCM_HEADER_BYTES = 64
DPCM_SAMPLE_BYTES = 32


class ExportError(RuntimeError):
    pass


def align4(value: int) -> int:
    return (value + 3) & ~3


def u16be(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def u32be(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def f32be(data: bytes, offset: int) -> float:
    return struct.unpack_from(">f", data, offset)[0]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ExportError(message)


def parse_variable_pair(data: bytes, offset: int) -> tuple[int, int, int]:
    first = u16be(data, offset)
    if first & 0x8000:
        size = ((first & 0x7FFF) << 16) | u16be(data, offset + 2)
        return size, u32be(data, offset + 4), 8
    return first, u16be(data, offset + 2), 4


@dataclass(frozen=True)
class ListParameter:
    keys: tuple[tuple[float, float], ...]
    range_begin: float
    range_end: float
    interpolation: int
    outside_begin: int
    outside_end: int

    def _outside(self, value: float) -> float:
        difference = self.range_end - self.range_begin
        relative = value - self.range_begin
        outside = None
        if relative < 0.0:
            outside = self.outside_begin
        elif relative >= difference:
            outside = self.outside_end
        if outside is None or outside == 0:
            return value
        if outside == 1:
            return self.range_begin + math.fmod(relative, difference)
        if outside == 2:
            turn = math.fmod(relative, difference * 2.0)
            if turn < 0.0:
                turn += difference * 2.0
            if turn >= difference:
                turn = difference * 2.0 - turn
            return self.range_begin + turn
        if outside == 3:
            return min(self.range_end, max(self.range_begin, value))
        raise ExportError(f"unsupported FVB outside mode {outside}")

    def sample(self, seconds: float) -> float:
        # All demo38_01 camera functions use progress=0 and adjust=0.  This is
        # JStudio TFunctionValue_list_parameter::getValue with interpolation 3.
        parameter = self._outside(seconds)
        times = [key[0] for key in self.keys]
        upper = bisect.bisect_right(times, parameter)
        if upper == 0:
            return self.keys[0][1]
        if upper == len(self.keys):
            return self.keys[-1][1]
        require(self.interpolation == 3, "camera FVB is not nonuniform B-spline")
        return interpolate_bspline_nonuniform(self.keys, upper, parameter)


def interpolate_bspline_nonuniform(
    keys: tuple[tuple[float, float], ...], upper: int, parameter: float
) -> float:
    """Port of JStudio's update_INTERPOLATE_BSPLINE_dataMore3_."""
    require(len(keys) >= 3, "B-spline needs at least three keys")
    prev_time, prev_value = keys[upper - 1]
    next_time, next_value = keys[upper]

    values = [0.0, prev_value, next_value, 0.0]
    knots = [0.0, 0.0, prev_time, next_time, 0.0, 0.0]
    if upper == 1:
        values[0] = 2.0 * prev_value - next_value
        values[3] = keys[upper + 1][1]
        knots[4] = keys[upper + 1][0]
        knots[1] = 2.0 * prev_time - next_time
        knots[0] = 2.0 * prev_time - knots[4]
        if len(keys) - upper == 2:
            knots[5] = 2.0 * knots[4] - next_time
        else:
            knots[5] = keys[upper + 2][0]
    elif upper == 2:
        values[0] = keys[upper - 2][1]
        knots[1] = keys[upper - 2][0]
        knots[0] = 2.0 * knots[1] - prev_time
        if len(keys) - upper == 1:
            values[3] = 2.0 * next_value - prev_value
            knots[4] = 2.0 * next_time - prev_time
            knots[5] = 2.0 * next_time - knots[1]
        elif len(keys) - upper == 2:
            values[3] = keys[upper + 1][1]
            knots[4] = keys[upper + 1][0]
            knots[5] = 2.0 * knots[4] - next_time
        else:
            values[3] = keys[upper + 1][1]
            knots[4] = keys[upper + 1][0]
            knots[5] = keys[upper + 2][0]
    else:
        values[0] = keys[upper - 2][1]
        knots[1] = keys[upper - 2][0]
        knots[0] = keys[upper - 3][0]
        if len(keys) - upper == 1:
            values[3] = 2.0 * next_value - prev_value
            knots[4] = 2.0 * next_time - prev_time
            knots[5] = 2.0 * next_time - knots[1]
        elif len(keys) - upper == 2:
            values[3] = keys[upper + 1][1]
            knots[4] = keys[upper + 1][0]
            knots[5] = 2.0 * knots[4] - next_time
        else:
            values[3] = keys[upper + 1][1]
            knots[4] = keys[upper + 1][0]
            knots[5] = keys[upper + 2][0]

    k0, k1, k2, k3, k4, k5 = knots
    d0, d1, d2 = parameter - k0, parameter - k1, parameter - k2
    d3, d4, d5 = k3 - parameter, k4 - parameter, k5 - parameter
    inv32 = 1.0 / (k3 - k2)
    blend3 = (d3 * inv32) / (k3 - k1)
    blend2 = (d2 * inv32) / (k4 - k2)
    blend1 = (d3 * blend3) / (k3 - k0)
    blend4 = ((d1 * blend3) + (d4 * blend2)) / (k4 - k1)
    blend5 = (d2 * blend2) / (k5 - k2)
    terms = (
        d3 * blend1,
        (d0 * blend1) + (d4 * blend4),
        (d1 * blend4) + (d5 * blend5),
        d2 * blend5,
    )
    return sum(term * value for term, value in zip(terms, values))


def parse_fvb(data: bytes, offset: int) -> tuple[list[ListParameter], int]:
    require(data[offset : offset + 4] == b"FVB\0", "JFVB payload magic")
    require(u16be(data, offset + 4) == 0xFEFF, "FVB byte order")
    require(u16be(data, offset + 6) == 0x0100, "FVB version")
    count = u32be(data, offset + 12)
    cursor = offset + 16
    functions: list[ListParameter] = []
    for index in range(count):
        block_start = cursor
        size = u32be(data, cursor)
        block_type = u16be(data, cursor + 4)
        id_size = u16be(data, cursor + 6)
        require(size >= 8 and cursor + size <= len(data), "truncated FVB block")
        require(block_type == 5 and id_size == 0, f"FVB[{index}] contract")
        cursor += 8
        end = block_start + size
        range_begin = range_end = None
        interpolation = 0
        outside_begin = outside_end = 0
        keys = None
        while cursor < end:
            paragraph_size, paragraph_type, header_size = parse_variable_pair(data, cursor)
            content = cursor + header_size
            require(content + paragraph_size <= end, "truncated FVB paragraph")
            if paragraph_type == 0:
                require(paragraph_size == 0, "bad FVB terminator")
            elif paragraph_type == 1:
                key_count = u32be(data, content)
                require(paragraph_size == 4 + key_count * 8, "bad FVB key data")
                keys = tuple(
                    (f32be(data, content + 4 + key * 8),
                     f32be(data, content + 8 + key * 8))
                    for key in range(key_count)
                )
            elif paragraph_type == 0x12:
                require(paragraph_size == 8, "bad FVB range")
                range_begin = f32be(data, content)
                range_end = f32be(data, content + 4)
            elif paragraph_type == 0x15:
                require(paragraph_size == 4, "bad FVB outside modes")
                outside_begin = u16be(data, content)
                outside_end = u16be(data, content + 2)
            elif paragraph_type == 0x16:
                require(paragraph_size == 4, "bad FVB interpolation")
                interpolation = u32be(data, content)
            else:
                raise ExportError(f"unsupported FVB paragraph {paragraph_type:#x}")
            cursor = content + align4(paragraph_size)
        require(cursor == end and keys is not None, "incomplete FVB function")
        require(range_begin is not None and range_end is not None, "missing FVB range")
        require(all(keys[i][0] < keys[i + 1][0] for i in range(len(keys) - 1)),
                "unordered FVB keys")
        functions.append(ListParameter(
            keys, range_begin, range_end, interpolation,
            outside_begin, outside_end))
        cursor = end
    return functions, cursor


@dataclass(frozen=True)
class CameraCut:
    start_frame: int
    duration: int
    functions: tuple[int, int, int, int, int, int, int, int]


def parse_camera_cuts(data: bytes, start: int, end: int) -> list[CameraCut]:
    cursor = start
    pending: dict[int, tuple[int, ...]] = {}
    cuts: list[CameraCut] = []
    frame = 0
    expected_groups = {24: 3, 28: 3, 38: 1, 39: 1}
    while cursor < end:
        head = u32be(data, cursor)
        sequence_type = head >> 24
        parameter = head & 0xFFFFFF
        cursor += 4
        if sequence_type == 0:
            require(parameter == 0 and cursor == end, "camera sequence terminator")
            break
        if sequence_type == 2:
            require(set(pending) == set(expected_groups), "incomplete camera cut")
            for group, count in expected_groups.items():
                require(len(pending[group]) == count, "camera function arity")
            ordered = pending[24] + pending[28] + pending[39] + pending[38]
            cuts.append(CameraCut(frame, parameter, ordered))
            frame += parameter
            pending = {}
            continue
        require(sequence_type == 0x80, f"unsupported camera sequence {sequence_type:#x}")
        paragraph_end = cursor + parameter
        require(paragraph_end <= end, "truncated camera paragraph sequence")
        while cursor < paragraph_end:
            size, paragraph_type, header_size = parse_variable_pair(data, cursor)
            content = cursor + header_size
            group, operation = paragraph_type >> 5, paragraph_type & 0x1F
            require(group in expected_groups and operation == 0x12,
                    f"unsupported camera paragraph {paragraph_type:#x}")
            require(size == expected_groups[group] * 4, "camera paragraph size")
            require(group not in pending, "duplicate camera assignment")
            pending[group] = tuple(
                u32be(data, content + index * 4)
                for index in range(expected_groups[group])
            )
            cursor = content + align4(size)
        require(cursor == paragraph_end, "camera paragraph alignment")
    require(not pending, "camera assignments without wait")
    require(frame == EXPECTED_SOURCE_FRAMES, f"camera duration is {frame}, expected 2400")
    require(len(cuts) == 7, f"camera cut count is {len(cuts)}, expected 7")
    return cuts


def parse_source(data: bytes) -> tuple[list[ListParameter], list[CameraCut]]:
    require(hashlib.sha256(data).hexdigest() == EXPECTED_STB_SHA256,
            "unexpected demo38_01.stb SHA-256")
    require(len(data) == 9368, "unexpected STB byte size")
    require(data[:4] == b"STB\0", "STB magic")
    require(u16be(data, 4) == 0xFEFF and u16be(data, 6) == 3, "STB header")
    require(u32be(data, 8) == len(data), "STB total size")
    require(data[16:24] == b"jstudio\0" and u16be(data, 30) == 6,
            "STB JStudio target")
    block_count = u32be(data, 12)
    require(block_count == 10, "unexpected STB block count")

    cursor = 32
    first_size = u32be(data, cursor)
    require(data[cursor + 4 : cursor + 8] == b"JFVB", "first STB block is not JFVB")
    functions, fvb_end = parse_fvb(data, cursor + 8)
    require(fvb_end == cursor + first_size, "JFVB size mismatch")
    cursor += first_size

    camera_range = None
    for _ in range(1, block_count):
        start = cursor
        size = u32be(data, cursor)
        block_type = data[cursor + 4 : cursor + 8]
        flags, id_size = struct.unpack_from(">HH", data, cursor + 8)
        require(flags == 0 and size >= 12 + align4(id_size), "bad STB object block")
        identifier = data[cursor + 12 : cursor + 12 + id_size]
        content = cursor + 12 + align4(id_size)
        cursor = start + size
        require(cursor <= len(data), "truncated STB object")
        if block_type == b"JCMR" and identifier == b"camera\0":
            require(camera_range is None, "duplicate source camera")
            camera_range = (content, cursor)
    require(cursor == len(data), "STB trailing bytes")
    require(camera_range is not None, "source camera object missing")
    cuts = parse_camera_cuts(data, *camera_range)
    require(max(index for cut in cuts for index in cut.functions) < len(functions),
            "camera references missing FVB function")
    return functions, cuts


def sample_camera(
    functions: list[ListParameter], cuts: list[CameraCut], frame: int
) -> tuple[float, ...]:
    require(0 <= frame <= EXPECTED_SOURCE_FRAMES, "source frame outside track")
    cut = cuts[-1]
    for candidate in cuts:
        if frame < candidate.start_frame + candidate.duration:
            cut = candidate
            break
    local_frame = frame - cut.start_frame
    seconds = local_frame / float(SOURCE_FPS)
    values = tuple(functions[index].sample(seconds) for index in cut.functions)
    require(len(values) == 8 and all(math.isfinite(value) for value in values),
            "non-finite source camera sample")
    require(0.0 < values[6] < 180.0, "invalid source camera FOV")
    return values


def build_dpcm(functions: list[ListParameter], cuts: list[CameraCut]) -> bytes:
    samples = b"".join(
        struct.pack("<8f", *sample_camera(functions, cuts, frame))
        for frame in range(EXPECTED_SAMPLE_COUNT)
    )
    total_size = DPCM_HEADER_BYTES + len(samples)
    header = bytearray(DPCM_HEADER_BYTES)
    struct.pack_into(
        "<4sHHIIIIIII", header, 0,
        b"DPCM", 1, DPCM_HEADER_BYTES, total_size, 0,
        SOURCE_FPS, EXPECTED_SOURCE_FRAMES, EXPECTED_SAMPLE_COUNT,
        DPCM_SAMPLE_BYTES, DPCM_HEADER_BYTES,
    )
    struct.pack_into("<I", header, 36, 1)
    output = header + samples
    struct.pack_into("<I", output, 12, zlib.crc32(output) & 0xFFFFFFFF)
    return bytes(output)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source_stb", type=Path)
    parser.add_argument("output_dpcm", type=Path)
    args = parser.parse_args()
    source = args.source_stb.read_bytes()
    functions, cuts = parse_source(source)
    output = build_dpcm(functions, cuts)
    args.output_dpcm.parent.mkdir(parents=True, exist_ok=True)
    args.output_dpcm.write_bytes(output)
    digest = hashlib.sha256(output).hexdigest()
    print(
        "DEMO38_CAMERA_EXPORT_OK "
        f"source_sha256={EXPECTED_STB_SHA256} cuts={len(cuts)} "
        f"source_fps={SOURCE_FPS} source_frames={EXPECTED_SOURCE_FRAMES} "
        f"samples={EXPECTED_SAMPLE_COUNT} bytes={len(output)} sha256={digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
