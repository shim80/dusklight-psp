#!/usr/bin/env python3
"""Host contract tests for the opaque-order evidence comparator."""

from __future__ import annotations

import csv
import importlib.util
import json
import pathlib
import shutil
import sys


ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/opaque_order_compare/opaque_order_compare.py"
WORK = ROOT / ".tmp/opaque-order-evidence-host"
spec = importlib.util.spec_from_file_location("opaque_order_compare", TOOL)
module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(module)


def state(variant: str, indices: list[int]) -> dict:
    draws = []
    for slot, source_index in enumerate(indices):
        draws.append({
            "output_slot": slot,
            "source_index": source_index,
            "source": "room" if source_index < 2 else "link",
            "actor_id": 0 if source_index < 2 else 253,
            "material_id": 10 + source_index,
            "shape_id": 20 + source_index,
            "texture_id": 30 + source_index,
            "depth_test": True,
            "depth_write": True,
            "source_order_dependent": False,
        })
    return {
        "schema": "dusklight.psp.opaque-order.v1",
        "visual_build_id": "sha256:" + "a" * 64,
        "scene": "F_SP108", "room": 0, "layer": 0,
        "variant": variant, "seed": module.SEED,
        "presentation": "opaque_only",
        "warmup_frames": 1, "measured_frames": 1,
        "simulation_updates": 1,
        "camera": {
            "eye": [1.0, 2.0, 3.0], "center": [4.0, 5.0, 6.0],
            "fov": 52.0, "near": 20.0, "far": 12000.0,
        },
        "depth_policy": {
            "clear": 0, "mapping": "source_reversed", "write": True,
        },
        "lighting": "off", "fog": "off", "shadows": "off",
        "draw_count": len(draws), "excluded_non_writing_draws": 0,
        "framebuffer_hash_fnv1a": "00000000", "draws": draws,
        "complete_draw_manifest": True, "complete_state_per_draw": True,
        "error_code": 0,
    }


def write_acquisition(name: str, variant: str, indices: list[int]) -> None:
    directory = WORK / name
    directory.mkdir(parents=True, exist_ok=True)
    value = state(variant, indices)
    (directory / "OPAQUE_ORDER_STATE.json").write_text(
        json.dumps(value), encoding="utf-8"
    )
    (directory / "OPAQUE_ORDER.OK").write_bytes(
        b"DUSKLIGHT_PSP_OPAQUE_ORDER_OK"
    )
    (directory / "OPAQUE_ORDER.METRICS").write_text(
        "error_code=0\n", encoding="utf-8"
    )
    (directory / "framebuffer.5650").write_bytes(
        bytes(module.FRAMEBUFFER_BYTES)
    )
    with (directory / "OPAQUE_ORDER_DRAWS.csv").open(
        "w", newline="", encoding="utf-8"
    ) as stream:
        writer = csv.DictWriter(stream, fieldnames=value["draws"][0].keys())
        writer.writeheader()
        writer.writerows({
            key: str(item).lower() if isinstance(item, bool) else item
            for key, item in draw.items()
        } for draw in value["draws"])
    (directory / "OPAQUE_ORDER_TRACE.jsonl").write_text(
        "".join(json.dumps({
            "schema": "dusklight.psp.opaque-order-trace.v1",
            "variant": variant, **draw,
        }, separators=(",", ":")) + "\n" for draw in value["draws"]),
        encoding="utf-8",
    )


def reset() -> None:
    shutil.rmtree(WORK, ignore_errors=True)
    count = 4
    write_acquisition("source", "source_order", list(range(count)))
    write_acquisition("reverse", "reverse_order", list(reversed(range(count))))
    write_acquisition(
        "permutation", "deterministic_permutation",
        module.expected_permutation(count),
    )


def main() -> int:
    reset()
    result = module.compare(WORK)
    validation = module.validate(WORK)
    if result["error_code"] != 0 or validation["error_code"] != 0:
        return 1
    negative_cases = 0

    framebuffer = WORK / "reverse/framebuffer.5650"
    framebuffer.write_bytes(b"\x01\x00" + framebuffer.read_bytes()[2:])
    result = module.compare(WORK)
    negative_cases += 1
    if result["error_code"] == 0 or result["pixel_difference_count"] != 1:
        return 2

    reset()
    value_path = WORK / "permutation/OPAQUE_ORDER_STATE.json"
    value = json.loads(value_path.read_text(encoding="utf-8"))
    value["draws"] = list(reversed(value["draws"]))
    for slot, draw in enumerate(value["draws"]):
        draw["output_slot"] = slot
    value_path.write_text(json.dumps(value), encoding="utf-8")
    with (WORK / "permutation/OPAQUE_ORDER_DRAWS.csv").open(
        "w", newline="", encoding="utf-8"
    ) as stream:
        writer = csv.DictWriter(stream, fieldnames=value["draws"][0].keys())
        writer.writeheader()
        writer.writerows({
            key: str(item).lower() if isinstance(item, bool) else item
            for key, item in draw.items()
        } for draw in value["draws"])
    (WORK / "permutation/OPAQUE_ORDER_TRACE.jsonl").write_text(
        "".join(json.dumps({
            "schema": "dusklight.psp.opaque-order-trace.v1",
            "variant": "deterministic_permutation", **draw,
        }, separators=(",", ":")) + "\n" for draw in value["draws"]),
        encoding="utf-8",
    )
    result = module.compare(WORK)
    negative_cases += 1
    if result["permutation_valid"] or result["error_code"] == 0:
        return 3

    reset()
    (WORK / "source/OPAQUE_ORDER.OK").write_text("invalid", encoding="utf-8")
    negative_cases += 1
    try:
        module.compare(WORK)
    except module.EvidenceError:
        pass
    else:
        return 4

    shutil.rmtree(WORK, ignore_errors=True)
    print(
        "OPAQUE_ORDER_EVIDENCE_HOST_OK "
        f"variants=3 negative_cases={negative_cases}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
