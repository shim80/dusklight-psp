#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"
SOURCE_ID=""
PREPARE_ONLY=false
while [ "$#" -gt 0 ]; do
  case "$1" in
    --source-request) shift; SOURCE_ID="${1:-}" ;;
    --prepare-only) PREPARE_ONLY=true ;;
    *) die "option récupération inconnue : $1" ;;
  esac
  shift
done
[[ "$SOURCE_ID" =~ ^[A-Za-z0-9][A-Za-z0-9._-]{0,95}$ ]] ||
  die "source-request invalide"
$PREPARE_ONLY || "$SCRIPT_DIR/ensure-ppsspp-gui-broker.sh" >/dev/null
STATE="$(assert_project_path ".test-data/ppsspp-gui-broker")"
SOURCE="$STATE/processing/$SOURCE_ID/request.json"
[ -s "$SOURCE" ] || die "requête source absente : $SOURCE_ID"
RECOVERY_ID="$(timestamp_utc)-recover-${SOURCE_ID:0:40}"
QUEUE="$STATE/requests/$RECOVERY_ID.json"
RESPONSE="$STATE/responses/$RECOVERY_ID.json"
/usr/bin/python3 - "$SOURCE" "$QUEUE" "$RECOVERY_ID" "$PROJECT_ROOT" "$STATE" "$PREPARE_ONLY" <<'PY'
import datetime, hashlib, json, pathlib, sys, time
source = pathlib.Path(sys.argv[1])
queue = pathlib.Path(sys.argv[2])
request_id = sys.argv[3]
root = pathlib.Path(sys.argv[4])
state = pathlib.Path(sys.argv[5])
value = json.loads(source.read_text())
if "protocol_version" not in value:
    runner = value["runner_request"]
    prepared = json.loads((
        root / ".test-data/ppsspp/gui-runner/requests" /
        runner["request_id"] / "request.json"
    ).read_text())
    old_markers = prepared["expected_markers"]
    old_game = pathlib.Path(old_markers[0]).parent
    fixture = root / ".tools/dusklight-ppsspp-gui-broker/DusklightPpssppGuiBroker.app/Contents/Resources/worker-version.fixture"
    fixture.write_text("3\n")
    value = {
        "protocol_version": 3,
        "application_type": "ppsspp",
        "application_request": None,
        "request_id": request_id,
        "campaign_id": f"recovery-{runner['request_id']}",
        "scenario_id": "artifact_recovery",
        "fingerprint": "0" * 64,
        "parity_build_id": value["parity_build_id"],
        "visual_build_id": None,
        "identity_scope": "parity",
        "attempt": 1,
        "created_at": datetime.datetime.now(datetime.timezone.utc).isoformat().replace("+00:00", "Z"),
        "timeout": 30,
        "worker_version_required": "3",
        "repository_root": str(root),
        "runner_request": runner,
        "collect_existing_artifacts": True,
        "artifact_expectations": {"minimum_metrics": 12, "minimum_traces": 0, "minimum_captures": 0},
        "client_mirror": {
            "game_directory": str(old_game),
            "result_path": prepared["result_path"],
            "stdout_path": prepared["stdout_path"],
            "stderr_path": prepared["stderr_path"],
        },
        "worker_version_fixture": str(fixture),
    }
value["request_id"] = request_id
value["scenario_id"] = "artifact_recovery"
value["attempt"] += 1
value["created_at"] = datetime.datetime.now(datetime.timezone.utc).isoformat().replace("+00:00", "Z")
value["timeout"] = 30
value["collect_existing_artifacts"] = True
value["protocol_version"] = 3
value["application_type"] = "ppsspp"
value["application_request"] = None
value["worker_version_required"] = "3"
if "visual_build_id" not in value:
    value["visual_build_id"] = None
payload = json.dumps(value, sort_keys=True).encode()
value["fingerprint"] = hashlib.sha256(payload).hexdigest()
if sys.argv[6] == "true":
    queue.write_text(json.dumps(value, sort_keys=True, indent=2) + "\n")
else:
    mailbox = state / "mailbox.request.json"
    acknowledgement = state / "mailbox.response.json"
    deadline = time.monotonic() + 5
    while mailbox.read_text().strip() and time.monotonic() < deadline:
        time.sleep(0.05)
    if mailbox.read_text().strip():
        raise SystemExit("mailbox broker occupée")
    mailbox.write_text(json.dumps(value, sort_keys=True))
    reply = {}
    while time.monotonic() < deadline:
        reply = json.loads(acknowledgement.read_text())
        if reply.get("request_id") == request_id:
            break
        time.sleep(0.05)
    if not reply.get("accepted"):
        raise SystemExit(f"récupération refusée: {reply.get('error')}")
PY
$PREPARE_ONLY && {
  printf 'ARTIFACT_RECOVERY_PREPARED request_id=%s\n' "$RECOVERY_ID"
  exit 0
}
deadline=$((SECONDS + 45))
while [ ! -s "$RESPONSE" ] && [ "$SECONDS" -lt "$deadline" ]; do
  sleep 0.2
done
[ -s "$RESPONSE" ] || die "récupération sans réponse"
cat "$RESPONSE"
/usr/bin/python3 - "$RESPONSE" <<'PY'
import json, pathlib, sys
value = json.loads(pathlib.Path(sys.argv[1]).read_text())
raise SystemExit(0 if value.get("result_code") == 0 else 1)
PY
