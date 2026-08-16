#!/usr/bin/env python3
"""Compile the two reference demo01_01 shots from source STB/BMG data.

The tool reads legally supplied source files and emits only a compact JSON
description. It never copies source binary payloads into the output.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Any


def be16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def be32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def bef32(data: bytes, offset: int) -> float:
    return struct.unpack_from(">f", data, offset)[0]


def aligned(value: int, alignment: int = 4) -> int:
    return (value + alignment - 1) & -alignment


def variable_uint(data: bytes, offset: int) -> tuple[int, int, int]:
    first = be16(data, offset)
    if first & 0x8000:
        return (
            ((first & 0x7FFF) << 16) | be16(data, offset + 2),
            be32(data, offset + 4),
            8,
        )
    return first, be16(data, offset + 2), 4


@dataclass(frozen=True)
class Block:
    kind: str
    name: str
    content: int
    end: int


def stb_blocks(data: bytes) -> list[Block]:
    if data[:4] != b"STB\0" or be16(data, 4) != 0xFEFF:
        raise ValueError("not a big-endian STB file")
    if be16(data, 6) != 3 or be32(data, 8) != len(data):
        raise ValueError("unexpected demo01 STB version or size")
    result: list[Block] = []
    offset = 0x20
    for _ in range(be32(data, 12)):
        size = be32(data, offset)
        if size < 8 or offset + size > len(data):
            raise ValueError("STB block outside file")
        raw_kind = data[offset + 4 : offset + 8]
        kind = (
            raw_kind.decode("ascii")
            if raw_kind != b"\xff\xff\xff\xff"
            else "CTRL"
        )
        if kind == "JFVB":
            result.append(Block(kind, "", offset + 8, offset + size))
        elif kind != "CTRL":
            id_size = be16(data, offset + 10)
            name = data[offset + 12 : offset + 12 + id_size]
            name = name.rstrip(b"\0").decode("ascii")
            content = offset + 12 + aligned(id_size)
            result.append(Block(kind, name, content, offset + size))
        else:
            result.append(Block(kind, "control", offset + 8, offset + size))
        offset += size
    if offset != len(data):
        raise ValueError("STB block count does not cover file")
    return result


def paragraphs(data: bytes, begin: int, size: int) -> list[tuple[int, bytes]]:
    result: list[tuple[int, bytes]] = []
    offset = begin
    end = begin + size
    while offset < end:
        payload_size, kind, header_size = variable_uint(data, offset)
        payload = offset + header_size
        result.append((kind, data[payload : payload + payload_size]))
        offset = payload + aligned(payload_size)
    if offset != end:
        raise ValueError("paragraph bundle is not aligned")
    return result


def sequence_events(data: bytes, block: Block) -> list[tuple[int, list[tuple[int, bytes]]]]:
    result: list[tuple[int, list[tuple[int, bytes]]]] = []
    offset = block.content
    frame = 0
    while offset < block.end:
        head = be32(data, offset)
        offset += 4
        kind = head >> 24
        parameter = head & 0xFFFFFF
        if kind == 0:
            break
        if kind == 2:
            frame += parameter
        elif kind == 0x80:
            result.append((frame, paragraphs(data, offset, parameter)))
            offset += parameter
        else:
            raise ValueError(
                f"unsupported sequence opcode {kind:#x} in {block.name}"
            )
    return result


def fvb_functions(data: bytes, block: Block) -> list[dict[str, Any]]:
    if data[block.content : block.content + 4] != b"FVB\0":
        raise ValueError("JFVB does not contain FVB")
    fvb = block.content
    if be16(data, fvb + 4) != 0xFEFF or be16(data, fvb + 6) != 0x100:
        raise ValueError("unexpected FVB byte order or version")
    offset = fvb + 16
    result: list[dict[str, Any]] = []
    for _ in range(be32(data, fvb + 12)):
        size = be32(data, offset)
        kind = be16(data, offset + 4)
        id_size = be16(data, offset + 6)
        cursor = offset + 8 + aligned(id_size)
        item: dict[str, Any] = {"kind": kind, "interpolation": 0}
        for paragraph, raw in paragraphs(data, cursor, offset + size - cursor):
            if paragraph == 1 and kind == 2:
                item["constant"] = struct.unpack(">f", raw)[0]
            elif paragraph == 1 and kind == 5:
                count = be32(raw, 0)
                item["keys"] = [
                    (bef32(raw, 4 + index * 8), bef32(raw, 8 + index * 8))
                    for index in range(count)
                ]
            elif paragraph == 0x12:
                item["range"] = (bef32(raw, 0), bef32(raw, 4))
            elif paragraph == 0x16:
                item["interpolation"] = be32(raw, 0)
        result.append(item)
        offset += size
    if offset != fvb + be32(data, fvb + 8):
        raise ValueError("FVB block count does not cover payload")
    return result


def function_value(function: dict[str, Any], seconds: float) -> float:
    if function["kind"] == 2:
        return float(function["constant"])
    if function["kind"] != 5:
        raise ValueError(f"unsupported FVB function {function['kind']}")
    keys = function["keys"]
    if seconds <= keys[0][0]:
        return float(keys[0][1])
    if seconds >= keys[-1][0]:
        return float(keys[-1][1])
    for index in range(1, len(keys)):
        right_time, right_value = keys[index]
        if seconds < right_time:
            left_time, left_value = keys[index - 1]
            if left_value == right_value:
                return float(left_value)
            interpolation = function["interpolation"]
            amount = (seconds - left_time) / (right_time - left_time)
            if interpolation == 1:
                return left_value + (right_value - left_value) * amount
            if interpolation == 2:
                smooth = amount * amount * (3.0 - 2.0 * amount)
                return left_value + (right_value - left_value) * smooth
            raise ValueError(
                "selected shot crosses an unsupported non-constant FVB segment"
            )
    raise AssertionError("unreachable FVB segment")


def camera_assignments(data: bytes, block: Block) -> list[tuple[int, list[int]]]:
    state = [-1] * 10
    result: list[tuple[int, list[int]]] = []
    group_variables = {
        24: (0, 1, 2),
        28: (3, 4, 5),
        39: (6,),
        38: (7,),
        42: (8, 9),
    }
    for frame, bundle in sequence_events(data, block):
        changed = False
        for paragraph, raw in bundle:
            group, operation = paragraph >> 5, paragraph & 0x1F
            if group not in group_variables or operation != 0x12:
                continue
            variables = group_variables[group]
            if len(raw) != len(variables) * 4:
                raise ValueError("camera FVB assignment has wrong size")
            for index, variable in enumerate(variables):
                state[variable] = be32(raw, index * 4)
            changed = True
        if changed:
            result.append((frame, list(state)))
    return result


def fixed_data_words(raw: bytes) -> list[int]:
    if not raw:
        return []
    descriptor = raw[0]
    width_code = descriptor & 7
    widths = (0, 1, 2, 4, 8, 16, 32, 64)
    width = widths[width_code]
    count_offset = 2 if descriptor & 8 else 1
    count = raw[1] if descriptor & 8 else 1
    if width != 4 or count_offset + count * width > len(raw):
        return []
    return [be32(raw, count_offset + index * width) for index in range(count)]


def actor_states(data: bytes, block: Block) -> list[dict[str, Any]]:
    translation = [0.0, 0.0, 0.0]
    rotation = [0.0, 0.0, 0.0]
    resources: dict[tuple[int, int], int] = {}
    result: list[dict[str, Any]] = []
    animation_start = 0
    for frame, bundle in sequence_events(data, block):
        changed = False
        for paragraph, raw in bundle:
            group, operation = paragraph >> 5, paragraph & 0x1F
            if group == 12 and operation == 2 and len(raw) == 12:
                translation = list(struct.unpack(">3f", raw))
                changed = True
            elif group == 16 and operation == 2 and len(raw) == 12:
                rotation = list(struct.unpack(">3f", raw))
                changed = True
            elif paragraph == 0x80:
                for word in fixed_data_words(raw):
                    if word >> 30 != 2:
                        continue
                    owner = (word >> 24) & 0xF
                    resource_kind = (word >> 16) & 0xF
                    resources[(owner, resource_kind)] = word & 0xFFFF
                    if resource_kind == 2 and owner in (0, 1):
                        animation_start = frame
                    changed = True
            elif group == 57 and operation == 0x19 and len(raw) == 4:
                word = be32(raw, 0)
                resources[((word >> 24) & 0xF, (word >> 16) & 0xF)] = (
                    word & 0xFFFF
                )
                changed = True
        if changed:
            result.append(
                {
                    "frame": frame,
                    "translation": list(translation),
                    "rotation_degrees": list(rotation),
                    "resources": dict(resources),
                    "animation_start": animation_start,
                }
            )
    return result


def current(items: list[Any], frame: int, key) -> Any:
    eligible = [item for item in items if key(item) <= frame]
    if not eligible:
        raise ValueError(f"no state at source frame {frame}")
    return eligible[-1]


def bmg_message(data: bytes, message_index: int) -> dict[str, Any]:
    if data[:8] != b"MESGbmg1":
        raise ValueError("not a BMG1 file")
    sections: dict[str, int] = {}
    offset = 0x20
    for _ in range(be32(data, 12)):
        sections[data[offset : offset + 4].decode("ascii")] = offset
        offset += be32(data, offset + 4)
    info, text, message_ids = (
        sections["INF1"], sections["DAT1"], sections["MID1"]
    )
    count = be16(data, info + 8)
    stride = be16(data, info + 10)
    if message_index >= count:
        raise ValueError("message index outside BMG")
    string_offset = be32(data, info + 16 + message_index * stride)
    cursor = text + 8 + string_offset
    output: list[str] = []
    tags: list[str] = []
    while data[cursor] != 0:
        if data[cursor] == 0x1A:
            length = data[cursor + 1]
            tags.append(data[cursor : cursor + length].hex())
            cursor += length
        else:
            output.append(bytes((data[cursor],)).decode("cp1252"))
            cursor += 1
    return {
        "index": message_index,
        "id": be32(data, message_ids + 16 + message_index * 4),
        "text": "".join(output),
        "control_tags": tags,
    }


def compile_shot(
    name: str,
    message_index: int,
    message_events: list[tuple[int, int]],
    cameras: list[tuple[int, list[int]]],
    functions: list[dict[str, Any]],
    link_states: list[dict[str, Any]],
    rusl_states: list[dict[str, Any]],
    bmg: bytes | None,
) -> dict[str, Any]:
    message_frame = next(
        frame for frame, index in message_events if index == message_index
    )
    camera_frame, assignments = current(cameras, message_frame, lambda x: x[0])
    seconds = (message_frame - camera_frame) / 30.0
    values = [function_value(functions[index], seconds) for index in assignments]
    link = current(link_states, message_frame, lambda x: x["frame"])
    rusl = current(rusl_states, message_frame, lambda x: x["frame"])
    result = {
        "name": name,
        "message": (
            bmg_message(bmg, message_index)
            if bmg is not None
            else {"index": message_index}
        ),
        "source_timeline_frame": message_frame,
        "camera_event_frame": camera_frame,
        "camera_local_seconds": seconds,
        "camera": {
            "eye": values[0:3],
            "center": values[3:6],
            "fov": values[6],
            "roll": values[7],
            "near": values[8],
            "far": values[9],
        },
        "link": {
            "translation": link["translation"],
            "rotation_degrees": link["rotation_degrees"],
            "body_bck_id": link["resources"].get((1, 2)),
            "face_bck_id": link["resources"].get((9, 2)),
            "animation_elapsed_frame": message_frame - link["animation_start"],
        },
        "rusl": {
            "translation": rusl["translation"],
            "rotation_degrees": rusl["rotation_degrees"],
            "model_id": rusl["resources"].get((0, 1)),
            "body_bck_id": rusl["resources"].get((0, 2)),
            "btk_id": rusl["resources"].get((0, 3)),
            "btp_id": rusl["resources"].get((0, 5)),
            "animation_elapsed_frame": message_frame - rusl["animation_start"],
        },
    }
    if not all(math.isfinite(value) for value in values):
        raise ValueError("compiled camera contains non-finite values")
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("stb", type=Path)
    parser.add_argument("--bmg", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    stb = args.stb.read_bytes()
    bmg = args.bmg.read_bytes() if args.bmg else None
    blocks = stb_blocks(stb)
    by_name = {block.name: block for block in blocks if block.name}
    fvb = next(block for block in blocks if block.kind == "JFVB")
    functions = fvb_functions(stb, fvb)
    cameras = camera_assignments(stb, by_name["camera"])
    messages: list[tuple[int, int]] = []
    for frame, bundle in sequence_events(stb, by_name["message"]):
        for paragraph, raw in bundle:
            if paragraph >> 5 == 0x42 and paragraph & 0x1F == 0x19:
                messages.append((frame, be32(raw, 0)))
    output = {
        "format": "DUSKLIGHT_DEMO01_COMPACT_TIMELINE_V1",
        "source": {
            "stb_sha256": hashlib.sha256(stb).hexdigest(),
            "stb_size": len(stb),
            "bmg_sha256": hashlib.sha256(bmg).hexdigest() if bmg else None,
        },
        "timebase_hz": 30,
        "shots": [
            compile_shot(
                "wide", 1512, messages, cameras, functions,
                actor_states(stb, by_name["Link"]),
                actor_states(stb, by_name["d_act0"]), bmg,
            ),
            compile_shot(
                "rusl_closeup", 1514, messages, cameras, functions,
                actor_states(stb, by_name["Link"]),
                actor_states(stb, by_name["d_act0"]), bmg,
            ),
        ],
    }
    rendered = json.dumps(output, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
