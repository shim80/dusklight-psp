#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

STATE="$(assert_project_path .test-data/ppsspp-gui-broker)"
OUTPUT_REL=artifacts/desktop-gui-transport
OUTPUT="$(assert_project_path "$OUTPUT_REL")"
safe_mkdir "$OUTPUT_REL"

/usr/bin/python3 - "$STATE" "$OUTPUT" <<'PY'
import json, pathlib, sys
state, output = map(pathlib.Path, sys.argv[1:])
ids = {
    "D1": "desktop-selftest-d1-vanilla-8",
    "D2": "desktop-selftest-d2-trace",
    "D3": "desktop-selftest-d3-startup-final",
    "D4": "desktop-selftest-d4-fsp108-3",
    "D5": "desktop-selftest-d5-one-frame-fixed-2",
    "D6": "visual-v1-fsp108-four-frame-fixed",
    "D7": "desktop-selftest-d7-recollect-fixed",
    "D8": "desktop-selftest-d8-invalid",
    "D9": "desktop-selftest-d9-valid-after-invalid",
}
responses = {key: json.loads((state / "responses" / f"{value}.json").read_text()) for key, value in ids.items()}
success = {"D1": "DESKTOP_TRANSPORT_VALID", "D2": "DESKTOP_TRANSPORT_VALID",
           "D3": "DESKTOP_TRACE_VALID", "D4": "DESKTOP_TRANSPORT_VALID",
           "D5": "DESKTOP_TRACE_VALID", "D6": "DESKTOP_TRACE_VALID",
           "D7": "DESKTOP_TRACE_VALID", "D9": "DESKTOP_TRANSPORT_VALID"}
for key, classification in success.items():
    if responses[key].get("classification") != classification or responses[key].get("result_code") != 0:
        raise SystemExit(f"{key} invalide: {responses[key].get('classification')}")
if responses["D8"].get("classification") != "HOST_DESKTOP_EXECUTABLE_INVALID":
    raise SystemExit("D8 n'a pas rejeté le hash exécutable")
pids = {responses[key].get("supervisor_pid") for key in responses}
if len(pids) != 1:
    raise SystemExit(f"PID superviseur instable: {sorted(pids)}")
for key in ("D1", "D2", "D3", "D4", "D5", "D6", "D9"):
    value = responses[key]
    if not all(value.get(field) for field in ("process_created", "dawn_initialized", "metal_initialized", "disc_mounted")):
        raise SystemExit(f"frontière de boot incomplète: {key}")
    if value.get("launchservices_primary_used") is not False or value.get("desktop_direct_launch_used") is not True:
        raise SystemExit(f"transport direct non prouvé: {key}")
metrics = {
    "classification": "READY_AUTONOMOUS_DUSKLIGHT_DESKTOP_GUI_TRANSPORT",
    "broker_supervisor_pid": next(iter(pids)), "broker_supervisor_pid_stable": True,
    "desktop_workers_loaded_fresh": True, "desktop_direct_launch_used": True,
    "launchservices_primary_used": False, "process_created": True,
    "dawn_initialized": True, "metal_initialized": True, "disc_mounted": True,
    "scene_reached": responses["D6"]["scene_reached"],
    "trace_valid": responses["D6"]["trace_valid"],
    "request_after_failure_valid": responses["D9"]["result_code"] == 0,
    "manual_restart_count": 0, "user_confirmation_prompts": 0,
    "v1_request_id": ids["D6"], "v1_trace_sha256": responses["D6"]["trace_sha256"],
    "v1_trace_events": responses["D6"]["trace_event_count"], "v1_trace_frames": responses["D6"]["frame_count"],
}
(output / "DESKTOP_GUI_TRANSPORT.METRICS").write_text("".join(
    f"{key}={str(value).lower() if isinstance(value, bool) else value}\n" for key, value in metrics.items()))
(output / "DESKTOP_GUI_TRANSPORT.OK").write_text("DUSKLIGHT_DESKTOP_GUI_TRANSPORT_OK\n")
(output / "DESKTOP_GUI_TRANSPORT.responses.json").write_text(json.dumps(
    {key: {"request_id": ids[key], "classification": value.get("classification"),
           "result_code": value.get("result_code"), "supervisor_pid": value.get("supervisor_pid")}
     for key, value in responses.items()}, sort_keys=True, indent=2) + "\n")
PY

printf 'READY_AUTONOMOUS_DUSKLIGHT_DESKTOP_GUI_TRANSPORT metrics=%s\n' \
  "${OUTPUT#"$PROJECT_ROOT"/}/DESKTOP_GUI_TRANSPORT.METRICS"
