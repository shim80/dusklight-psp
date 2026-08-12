#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

PREPARED=""
ACTION=wait
while [ "$#" -gt 0 ]; do
  case "$1" in
    --prepared-request) shift; PREPARED="${1:-}" ;;
    --queue) ACTION=queue ;;
    --wait) ACTION=wait ;;
    *) die "option broker inconnue : $1" ;;
  esac
  shift
done
[ -n "$PREPARED" ] || die "--prepared-request requis"
PREPARED="$(assert_project_path "$PREPARED")"
[ -s "$PREPARED" ] || die "requête préparée absente"

if ! "$SCRIPT_DIR/status-ppsspp-gui-broker.sh" >/dev/null 2>&1; then
  printf '%s\n' \
    'classification=PENDING_GUI_EXECUTION' \
    'reason=PPSSPP_GUI_BROKER_NOT_READY' \
    "manual_command=cd \"$PROJECT_ROOT\" && scripts/start-ppsspp-gui-broker.sh" >&2
  exit 1
fi

STATE="$(assert_project_path ".test-data/ppsspp-gui-broker")"
safe_mkdir .test-data/ppsspp-gui-broker/requests
safe_mkdir .test-data/ppsspp-gui-broker/processing
safe_mkdir .test-data/ppsspp-gui-broker/responses
safe_mkdir .test-data/ppsspp-gui-broker/logs

metadata="$(/usr/bin/python3 - "$PROJECT_ROOT" "$STATE" "$PREPARED" <<'PY'
import json
import hashlib
import os
import pathlib
import shutil
import sys
import time
from datetime import datetime, timezone

root = pathlib.Path(sys.argv[1])
state = pathlib.Path(sys.argv[2])
prepared = pathlib.Path(sys.argv[3])
request = json.loads(prepared.read_text())
client_result = request["result_path"]
client_stdout = request["stdout_path"]
client_stderr = request["stderr_path"]
request_id = request["request_id"]
resources = root / ".tools/dusklight-ppsspp-gui-broker/DusklightPpssppGuiBroker.app/Contents/Resources"
staging = resources / "requests" / request_id
staging.mkdir(parents=True, exist_ok=True)
bundled_ppsspp = resources / "PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL"
if hashlib.sha256(bundled_ppsspp.read_bytes()).hexdigest() != request["ppsspp_sha256"]:
    raise SystemExit("PPSSPP bundle ne correspond pas au hash épinglé")
request["ppsspp_executable"] = str(bundled_ppsspp)
for key, name in (("eboot_path", "EBOOT.PBP"), ("config_source", "ppsspp.ini")):
    source = pathlib.Path(request[key])
    destination = staging / name
    shutil.copy2(source, destination)
    request[key] = str(destination)
for index, package in enumerate(request["packages"]):
    source = pathlib.Path(package["path"])
    destination = staging / f"package-{index:02d}-{source.name}"
    if source.is_dir():
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(source, destination)
    else:
        shutil.copy2(source, destination)
    package["path"] = str(destination)
identity = {}
for line in (root / "build/reports/PARITY_BUILD_ID.metrics").read_text().splitlines():
    key, separator, value = line.partition("=")
    if separator:
        identity[key] = value
build_id = identity["parity_build_id"]
visual_identity = {}
for line in (root / "build/reports/VISUAL_BUILD_ID.metrics").read_text().splitlines():
    key, separator, value = line.partition("=")
    if separator:
        visual_identity[key] = value
visual_build_id = visual_identity.get("visual_build_id")
identity_scope = "parity"
if request["eboot_sha256"] != identity["eboot_sha256"]:
    transport_selftest = (
        request_id.startswith("selftest-") and request["mode"] == "smoke" and
        all(
            pathlib.Path(value).name in {
                "SMOKE.OK", "CONTINUOUS.OK", "LINK_GROUNDING.OK"
            }
            for value in request["expected_markers"]
        )
    )
    core_marker = any(
        pathlib.Path(value).name == "CORE.OK" and
        "DUSKLIGHT_CORE_SMOKE" in pathlib.Path(value).parts
        for value in request["expected_markers"]
    )
    if transport_selftest:
        identity_scope = "transport_selftest"
        build_id = None
    elif request["mode"] != "smoke" or not core_marker:
        raise SystemExit("EBOOT de requête différent du PARITY_BUILD_ID")
    else:
        identity_scope = "historical_core_smoke"
        build_id = None

old_memstick = pathlib.Path(request["memstick_root"])
ppsspp_session = resources / "PPSSPPSDL.app/Contents/Resources/sessions" / request_id
new_memstick = ppsspp_session / "home/.config/ppsspp/PSP"

def relocate(value):
    path = pathlib.Path(value)
    try:
        relative = path.relative_to(old_memstick)
    except ValueError:
        return value
    return str(new_memstick / relative)

old_markers = list(request["expected_markers"])
old_game = (
    pathlib.Path(old_markers[0]).parent
    if old_markers else old_memstick / "GAME" / "DUSKLIGHT_PSP"
)
request["memstick_root"] = str(new_memstick)
request["expected_markers"] = [relocate(path) for path in old_markers]
request["expected_marker_contents"] = {
    relocate(path): value
    for path, value in request["expected_marker_contents"].items()
}
for package in request["packages"]:
    package["destination"] = relocate(package["destination"])
request["environment_overrides"] = {
    "HOME": str(ppsspp_session / "home"),
    "XDG_CONFIG_HOME": str(ppsspp_session / "home/.config"),
    "XDG_CACHE_HOME": str(ppsspp_session / "xdg-cache"),
    "TMPDIR": str(ppsspp_session / "tmp"),
}
response = state / "responses" / f"{request_id}.json"
request["result_path"] = str(
    state / "processing" / request_id / "runner-response.json"
)
request["stdout_path"] = str(state / "logs" / f"{request_id}.stdout.log")
request["stderr_path"] = str(state / "logs" / f"{request_id}.stderr.log")
request["boot_signal_path"] = str(
    state / "processing" / request_id / "BOOT.OBSERVED"
)
mode = request["mode"]
minimum_metrics = 0
minimum_traces = 0
if mode == "smoke" and old_game.name == "DUSKLIGHT_PSP":
    minimum_metrics = 12
elif mode.startswith("benchmark_") or mode == "startup":
    minimum_metrics = 1
elif mode == "parity_trace":
    minimum_traces = 1
worker = root / "tools/macos/dusklight-ppsspp-gui-broker/request_worker.py"
collector = root / "tools/macos/dusklight-ppsspp-gui-broker/artifact_collector.py"
fixture = resources / "worker-version.fixture"
if not fixture.exists():
    fixture.write_text("3\n")
fingerprint_input = json.dumps(request, sort_keys=True).encode()
fingerprint_input += hashlib.sha256(worker.read_bytes()).digest()
fingerprint_input += hashlib.sha256(collector.read_bytes()).digest()
envelope = {
    "protocol_version": 3,
    "application_type": "ppsspp",
    "application_request": None,
    "request_id": request_id,
    "campaign_id": f"single-{request_id}",
    "scenario_id": mode,
    "fingerprint": hashlib.sha256(fingerprint_input).hexdigest(),
    "parity_build_id": build_id,
    "visual_build_id": visual_build_id,
    "identity_scope": identity_scope,
    "attempt": 1,
    "created_at": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
    "timeout": request["timeout_seconds"],
    "worker_version_required": "3",
    "repository_root": str(root),
    "runner_request": request,
    "collect_existing_artifacts": False,
    "artifact_expectations": {
        "minimum_metrics": minimum_metrics,
        "minimum_traces": minimum_traces,
        "minimum_captures": 0,
    },
    "client_mirror": {
        "game_directory": str(old_game),
        "result_path": client_result,
        "stdout_path": client_stdout,
        "stderr_path": client_stderr,
    },
    "worker_version_fixture": str(fixture),
}
response.unlink(missing_ok=True)
payload = json.dumps(envelope, sort_keys=True).encode()
mailbox = state / "mailbox.request.json"
acknowledgement = state / "mailbox.response.json"
deadline = time.monotonic() + 5
while mailbox.read_text(encoding="utf-8").strip() and time.monotonic() < deadline:
    time.sleep(0.05)
if mailbox.read_text(encoding="utf-8").strip():
    raise SystemExit("mailbox broker occupée")
mailbox.write_bytes(payload)
reply = {}
while time.monotonic() < deadline:
    reply = json.loads(acknowledgement.read_text())
    if reply.get("request_id") == request_id:
        break
    time.sleep(0.05)
if not reply.get("accepted"):
    raise SystemExit(f"requête refusée par le broker: {reply.get('error')}")
print(request_id, request["timeout_seconds"], response, client_result)
PY
)"
read -r request_id timeout response client_result <<<"$metadata"
printf 'PPSSPP_GUI_BROKER_REQUEST_QUEUED request_id=%s\n' "$request_id"
[ "$ACTION" = wait ] || exit 0

# The worker owns the PPSSPP timeout.  Leave enough time for its bounded
# shutdown, artifact collection, and the supervisor's atomic response write.
deadline=$((SECONDS + timeout + 60))
while [ ! -s "$response" ] && [ "$SECONDS" -lt "$deadline" ]; do
  sleep 0.2
done
[ -s "$response" ] || die "réponse broker absente : $request_id"

cp -- "$response" "$client_result"

/usr/bin/python3 - "$STATE/processing/$request_id/request.json" "$response" <<'PY'
import json, pathlib, shutil, sys
envelope = json.loads(pathlib.Path(sys.argv[1]).read_text())
response = json.loads(pathlib.Path(sys.argv[2]).read_text())
request = envelope["runner_request"]
markers = [pathlib.Path(value) for value in request["expected_markers"]]
source_game = markers[0].parent if markers else pathlib.Path(request["memstick_root"])
destination_game = pathlib.Path(envelope["client_mirror"]["game_directory"])
for entry in response.get("collected_artifacts", []):
    source = source_game / entry["path"]
    destination = destination_game / entry["path"]
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
for source_key, destination_key in (("stdout_path", "stdout_path"), ("stderr_path", "stderr_path")):
    source = pathlib.Path(request[source_key])
    destination = pathlib.Path(envelope["client_mirror"][destination_key])
    if source.is_file():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
PY

cat "$response"
/usr/bin/python3 - "$response" <<'PY'
import json
import pathlib
import sys
raise SystemExit(0 if json.loads(pathlib.Path(sys.argv[1]).read_text()).get("result_code") == 0 else 1)
PY
