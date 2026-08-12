#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

ACTION=queue
TIMEOUT=120
while [ "$#" -gt 0 ]; do
  case "$1" in
    --queue) ACTION=queue ;;
    --run) ACTION=run ;;
    --timeout) shift; TIMEOUT="${1:-}" ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done

if ! "$SCRIPT_DIR/ppsspp-gui-runner-status.sh" >/dev/null 2>&1; then
  "$SCRIPT_DIR/ppsspp-gui-runner-build.sh"
fi
"$SCRIPT_DIR/ppsspp-gui-runner-status.sh"

RUN_ID="$(timestamp_utc)"
CLIENT="$SCRIPT_DIR/ppsspp-gui-runner-request.sh"
CONFIG="test/gu-smoke/ppsspp-software.ini"
SCENARIO="artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP/DUSKLIGHT.SCENARIO"

invoke() {
  "$CLIENT" "--$ACTION" --request-id "$1" --eboot "$2" --game-id "$3" \
    --config "$CONFIG" --mode smoke --presentation game \
    --backend opengl --renderer software --timeout "$TIMEOUT" \
    --marker "$4=$5" "${@:6}"
}

invoke "selftest-minimal-$RUN_ID" \
  build/psp/smoke/EBOOT.PBP DUSKLIGHT_SMOKE \
  SMOKE.OK 'DUSKLIGHT_PSP_SMOKE_OK\n'

invoke "selftest-canonical-$RUN_ID" \
  artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP/EBOOT.PBP \
  DUSKLIGHT_PSP CONTINUOUS.OK DUSKLIGHT_PSP_CONTINUOUS_PORT_OK \
  --package artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP/data=data \
  --package "$SCENARIO=DUSKLIGHT.SCENARIO"

invoke "selftest-grounding-$RUN_ID" \
  build/psp/dusklight/EBOOT.PBP DUSKLIGHT_PSP \
  LINK_GROUNDING.OK DUSKLIGHT_PSP_LINK_GROUNDING_OK \
  --package build/assets/dusklight-psp/data=data \
  --package "$SCENARIO=DUSKLIGHT.SCENARIO"

GROUNDING_REQUEST="$(assert_project_path \
  ".test-data/ppsspp/gui-runner/requests/selftest-grounding-$RUN_ID/request.json")"
RUNNER_SOURCE="$(assert_project_path \
  "tools/macos/ppsspp-gui-runner/runner.py")"
/usr/bin/python3 - "$RUNNER_SOURCE" "$GROUNDING_REQUEST" "$PROJECT_ROOT" <<'PY'
import copy
import importlib.util
import json
import os
import pathlib
import sys

source, request_path, root = sys.argv[1:]
spec = importlib.util.spec_from_file_location("dusklight_gui_runner", source)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
baseline = json.loads(pathlib.Path(request_path).read_text())
module.validate_request(copy.deepcopy(baseline))

depth_fixture = copy.deepcopy(baseline)
depth_fixture["mode"] = "depth_behavior_fixture"
module.validate_request(depth_fixture)

opaque_order = copy.deepcopy(baseline)
opaque_order["mode"] = "opaque_order_invariance"
opaque_order["presentation_profile"] = "opaque_only"
module.validate_request(opaque_order)

cases = {}
cases["mode_unknown"] = {"mode": "arbitrary_psp_mode"}
cases["backend_unknown"] = {"graphics_backend": "metal"}
cases["timeout_excessive"] = {"timeout_seconds": 1801}
cases["eboot_hash_incorrect"] = {"eboot_sha256": "0" * 64}
cases["ppsspp_hash_incorrect"] = {"ppsspp_sha256": "0" * 64}
cases["relative_path"] = {"eboot_path": "build/psp/dusklight/EBOOT.PBP"}
cases["result_outside"] = {"result_path": "/tmp/dusklight-result.json"}
cases["personal_profile"] = {
    "memstick_root": str(pathlib.Path.home() / "Library" / "PPSSPP")
}
outside_marker = "/tmp/DUSKLIGHT.OUTSIDE.OK"
case = copy.deepcopy(baseline)
old_marker = case["expected_markers"][0]
case["expected_markers"] = [outside_marker]
case["expected_marker_contents"] = {
    outside_marker: case["expected_marker_contents"][old_marker]
}
cases["marker_outside"] = case

link = pathlib.Path(root) / ".tmp" / "ppsspp-gui-runner-outside-link"
if link.exists() or link.is_symlink():
    link.unlink()
os.symlink("/tmp", link)
cases["symlink_outside"] = {
    "result_path": str(link / "response.json")
}

try:
    for name, mutation in cases.items():
        candidate = (
            copy.deepcopy(mutation)
            if "request_version" in mutation
            else copy.deepcopy(baseline)
        )
        if "request_version" not in mutation:
            candidate.update(mutation)
        try:
            module.validate_request(candidate)
        except module.InvalidRequest:
            continue
        raise SystemExit(f"negative case accepted: {name}")
finally:
    link.unlink(missing_ok=True)
print(f"PPSSPP_GUI_RUNNER_NEGATIVE_TESTS_OK cases={len(cases)}")

classification_cases = [
    (
        dict(boot_observed=True, markers_valid=True, status=-9,
             host_graphics_error=True, timed_out=True),
        ("PSP_EBOOT_STARTED_AND_MARKERS_VALID", 0),
    ),
    (
        dict(boot_observed=True, markers_valid=False, status=-9,
             host_graphics_error=True, timed_out=True),
        ("HOST_GRAPHICS_INIT_FAILED", 31),
    ),
    (
        dict(boot_observed=True, markers_valid=False, status=-9,
             host_graphics_error=False, timed_out=False),
        ("PSP_EBOOT_STARTED_RUNTIME_FAILURE", 22),
    ),
    (
        dict(boot_observed=True, markers_valid=False, status=0,
             host_graphics_error=False, timed_out=False),
        ("PSP_EBOOT_STARTED_MARKER_FAILURE", 21),
    ),
    (
        dict(boot_observed=False, markers_valid=False, status=-9,
             host_graphics_error=False, timed_out=True),
        ("HOST_RUNNER_TIMEOUT", 32),
    ),
]
for inputs, expected in classification_cases:
    actual = module.classify_attempt(**inputs)
    if actual != expected:
        raise SystemExit(
            f"classification incorrect: inputs={inputs} "
            f"actual={actual} expected={expected}"
        )

if not module.GRAPHICS_FAILURE.search("OpenGL 2.0 or higher."):
    raise SystemExit("signal OpenGL minimal non reconnu")
failed_backend_root = pathlib.Path(root) / ".tmp" / "gui-runner-backend-test"
(failed_backend_root / "SYSTEM").mkdir(parents=True, exist_ok=True)
(failed_backend_root / "SYSTEM" / "FailedGraphicsBackends.txt").write_text(
    "OPENGL\n"
)
try:
    if not module.host_graphics_failure("", failed_backend_root):
        raise SystemExit("FailedGraphicsBackends.txt non reconnu")
finally:
    (
        failed_backend_root / "SYSTEM" / "FailedGraphicsBackends.txt"
    ).unlink(missing_ok=True)
    (failed_backend_root / "SYSTEM").rmdir()
    failed_backend_root.rmdir()
print(
    "PPSSPP_GUI_RUNNER_CLASSIFICATION_TESTS_OK "
    f"cases={len(classification_cases)}"
)

broker = copy.deepcopy(baseline)
old_memstick = pathlib.Path(broker["memstick_root"])
new_memstick = (
    pathlib.Path(root) / ".test-data" / "ppsspp-gui-broker" /
    "processing" / "selftest" / "session" / "home" / ".config" /
    "ppsspp" / "PSP"
)
def relocate(value):
    path = pathlib.Path(value)
    try:
        relative = path.relative_to(old_memstick)
    except ValueError:
        return value
    return str(new_memstick / relative)
broker["memstick_root"] = str(new_memstick)
broker["expected_markers"] = [
    relocate(value) for value in broker["expected_markers"]
]
broker["expected_marker_contents"] = {
    relocate(key): value
    for key, value in broker["expected_marker_contents"].items()
}
for package in broker["packages"]:
    package["destination"] = relocate(package["destination"])
broker["environment_overrides"] = {
    "HOME": str(new_memstick.parents[2]),
    "XDG_CONFIG_HOME": str(new_memstick.parents[1]),
    "XDG_CACHE_HOME": str(new_memstick.parents[3] / "xdg-cache"),
    "TMPDIR": str(new_memstick.parents[3] / "tmp"),
}
module.validate_request(broker)
print("PPSSPP_GUI_RUNNER_BROKER_PROFILE_OK")
PY

if [ "$ACTION" = queue ]; then
  printf '%s\n' \
    "PPSSPP_GUI_RUNNER_SELFTEST_PENDING requests=3 classification=PENDING_GUI_EXECUTION"
else
  printf '%s\n' "gui_runner_core_smoke_boot=true"
  printf '%s\n' "gui_runner_canonical_boot=true"
  printf '%s\n' "gui_runner_grounding_boot=true"
  printf '%s\n' "gui_runner_grounding_marker=true"
  printf '%s\n' "PPSSPP_GUI_RUNNER_SELFTEST_OK requests=3"
fi
