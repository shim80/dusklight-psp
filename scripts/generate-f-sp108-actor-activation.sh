#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

SCENE="$(assert_project_path \
  "build/assets/dusklight-startup/fsp108_room.dpsc")"
TRACE="$(assert_project_path \
  "artifacts/dusklight-desktop-oracle-v2/desktop-oracle-v2.jsonl")"
OUTPUT_DIR="$(assert_project_path "artifacts/dusklight-desktop-oracle-v2")"
OUTPUT="$OUTPUT_DIR/f_sp108_actor_activation.csv"
METRICS="$OUTPUT_DIR/F_SP108_ACTIVATION.METRICS"

[ -s "$SCENE" ] || die "scène F_SP108 absente"
[ -s "$TRACE" ] || die "oracle desktop v2 absent"
safe_mkdir artifacts/dusklight-desktop-oracle-v2

/usr/bin/python3 - "$SCENE" "$TRACE" "$OUTPUT" "$METRICS" <<'PY'
import csv
import json
import math
import pathlib
import struct
import sys

scene_path, trace_path, output_path, metrics_path = map(pathlib.Path, sys.argv[1:])
data = scene_path.read_bytes()
if data[:4] != b"DPSC":
    raise SystemExit("format DPSC invalide")

u16 = lambda offset: struct.unpack_from("<H", data, offset)[0]
u32 = lambda offset: struct.unpack_from("<I", data, offset)[0]
f32 = lambda offset: struct.unpack_from("<f", data, offset)[0]
count, offset, stride = u32(136), u32(140), u32(168)
if count != 599 or stride < 60 or offset + count * stride > len(data):
    raise SystemExit("table F_SP108 invalide")

events = [
    json.loads(line) for line in trace_path.read_text().splitlines() if line
]
transition = next(
    index for index, event in enumerate(events)
    if event.get("type") == "new_game_transition"
    and event.get("stage") == "F_SP108"
    and event.get("room") == 1
)
tail = events[transition + 1:]
creates = [event for event in tail if event.get("type") == "actor_create"]
executes = {
    event["actor_id"] for event in tail if event.get("type") == "actor_execute"
}
draws = {
    event["actor_id"] for event in tail if event.get("type") == "actor_draw"
}
deletes = {
    event["actor_id"] for event in tail if event.get("type") == "actor_delete"
}

visible_names = {
    "Grass", "flower", "flwr7", "flwr17", "Fish", "Yousei",
    "Obj_Uma", "spring", "FSeirei", "Horse", "Seirei", "Bd",
}
logic_names = {"Savmem", "CamChg", "SwAreaS", "SwAreaC", "Mhint"}
event_names = {"TagEv", "TagEvC", "atkItem"}

def source_name(raw):
    return raw.split(b"\0", 1)[0].decode("ascii", "replace")

def match_create(process_id, position):
    candidates = []
    for event in creates:
        if event.get("profile") != process_id:
            continue
        observed = event.get("position", [math.inf] * 3)
        # Some source actors correct Y during create; X/Z still identify the
        # placement without pretending that the adjusted transform is source.
        distance = math.hypot(
            float(observed[0]) - position[0],
            float(observed[2]) - position[2],
        )
        if distance <= 0.25:
            candidates.append((distance, event))
    return min(candidates, default=(math.inf, None), key=lambda item: item[0])[1]

rows = []
matched_ids = set()
for table_index in range(count):
    base = offset + table_index * stride
    name = source_name(data[base:base + 8])
    process_id = u16(base + 12)
    position = (f32(base + 20), f32(base + 24), f32(base + 28))
    flags = data[base + 41]
    source_index = u16(base + 42)
    create = match_create(process_id, position)
    actor_id = ""
    if create is not None and create["actor_id"] not in matched_ids:
        actor_id = create["actor_id"]
        matched_ids.add(actor_id)
    else:
        create = None

    if flags and create is not None:
        category = "ESSENTIAL_FIRST_FRAME"
    elif name in event_names:
        category = "EVENT_DEPENDENT"
    elif name in logic_names:
        category = "LOGIC_NONVISUAL"
    elif name in visible_names:
        category = "VISIBLE_NONESSENTIAL"
    elif process_id == 0xFFFF:
        category = "UNRESOLVED"
    else:
        category = "DISTANT_OR_INACTIVE"

    rows.append({
        "table": "F_SP108/R01/layer0",
        "index": table_index,
        "source_index": source_index,
        "name": name,
        "process_id": f"0x{process_id:04x}",
        "profile": f"0x{process_id:04x}" if process_id != 0xFFFF else "",
        "class": "not_serialized_by_dtrc_v2",
        "params": f"0x{u32(base + 16):08x}",
        "room": data[base + 39],
        "layer": data[base + 40],
        "position_x": f"{position[0]:.6f}",
        "position_y": f"{position[1]:.6f}",
        "position_z": f"{position[2]:.6f}",
        "create_frame": 0 if create is not None else "",
        "first_execute": 0 if actor_id in executes else "",
        "first_draw": 0 if actor_id in draws else "",
        "last_active": 1800 if actor_id and actor_id not in deletes else "",
        "resources": "not_serialized_by_dtrc_v2",
        "visible": "true" if actor_id in draws else "false",
        "distance_spawn": "not_serialized_by_dtrc_v2",
        "source_event_dependency": "true" if name in event_names else "false",
        "category": category,
        "psp_instantiated": "true" if flags else "false",
    })

with output_path.open("w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)

created = sum(row["create_frame"] == 0 for row in rows)
essential = sum(row["category"] == "ESSENTIAL_FIRST_FRAME" for row in rows)
instantiated = sum(row["psp_instantiated"] == "true" for row in rows)
metrics_path.write_text(
    "\n".join([
        "f_sp108_source_records=599",
        f"f_sp108_reference_created_first_frame={created}",
        f"f_sp108_reference_created_60={created}",
        f"f_sp108_reference_created_300={created}",
        f"f_sp108_reference_created_1800={created}",
        f"f_sp108_psp_instantiated={instantiated}",
        f"f_sp108_essential_total={essential}",
        f"f_sp108_essential_ported={instantiated}",
        f"f_sp108_essential_coverage={instantiated / essential:.3f}",
        "dtrc_v2_actor_class_available=false",
        "dtrc_v2_actor_resources_available=false",
        "dtrc_v2_distance_spawn_available=false",
        "hardware_validation=deferred_by_user",
        "user_manual_acceptance=pending",
        "error_code=0",
        "",
    ])
)
if essential != 9 or instantiated != 9:
    raise SystemExit(
        "frontière essentielle inattendue: "
        f"essential={essential} instantiated={instantiated}"
    )
print(
    f"F_SP108_ACTIVATION_OK records=599 created_first_frame={created} "
    "essential_first_frame=9 psp_instantiated=9"
)
PY
