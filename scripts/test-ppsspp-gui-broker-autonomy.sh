#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"
"$SCRIPT_DIR/ensure-ppsspp-gui-broker.sh" >/dev/null
STATE="$(assert_project_path ".test-data/ppsspp-gui-broker")"
LOG="$(assert_project_path "logs/ppsspp-gui-broker-autonomy.log")"
fixture="$(assert_project_path ".tools/dusklight-ppsspp-gui-broker/DusklightPpssppGuiBroker.app/Contents/Resources/worker-version.fixture")"
start_pid="$(/usr/bin/python3 - "$STATE/heartbeat.json" <<'PY'
import json, pathlib, sys
print(json.loads(pathlib.Path(sys.argv[1]).read_text())["supervisor_pid"])
PY
)"
start_workers="$(/usr/bin/python3 - "$STATE/GUI_BROKER.METRICS.json" <<'PY'
import json, pathlib, sys
print(json.loads(pathlib.Path(sys.argv[1]).read_text()).get("workers_spawned", 0))
PY
)"

"$SCRIPT_DIR/run-ppsspp-core-smoke-gui.sh" >>"$LOG" 2>&1
"$SCRIPT_DIR/run-ppsspp-dusklight-psp.sh" --run --mode smoke \
  --presentation game --backend opengl --transport gui --timeout 120 \
  >>"$LOG" 2>&1
source_id="$(/usr/bin/python3 - "$STATE/processing" <<'PY'
import json, pathlib, sys
candidates=[]
for path in pathlib.Path(sys.argv[1]).glob("*/request.json"):
    value=json.loads(path.read_text())
    if value.get("protocol_version") == 2 and value.get("scenario_id") == "smoke" and value.get("identity_scope") == "parity":
        candidates.append((path.stat().st_mtime, value["request_id"]))
print(max(candidates)[1])
PY
)"
printf 'A\n' >"$fixture"
recovery_a="$($SCRIPT_DIR/recover-ppsspp-gui-artifacts.sh --source-request "$source_id" 2>>"$LOG")"
printf '%s\n' "$recovery_a" >>"$LOG"
grep -q '"worker_fixture_version": "A"' <<<"$recovery_a"

"$SCRIPT_DIR/run-ppsspp-dusklight-psp.sh" --run --mode smoke \
  --presentation game --backend opengl --transport gui --timeout 120 \
  >>"$LOG" 2>&1
"$SCRIPT_DIR/run-ppsspp-dusklight-psp.sh" --run --mode parity_trace \
  --scenario link_idle_full_cycle --presentation game --backend opengl \
  --transport gui --timeout 300 >>"$LOG" 2>&1

invalid_id="$(timestamp_utc)-autonomy-invalid"
/usr/bin/python3 - "$STATE/processing/$source_id/request.json" "$STATE" "$invalid_id" <<'PY'
import hashlib, json, pathlib, sys, time
source, state, request_id = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3]
value=json.loads(source.read_text()); value["request_id"]=request_id
value["scenario_id"]="controlled_invalid"; value["runner_request"]["request_id"]=request_id
value["runner_request"]["eboot_sha256"]="0"*64
value["fingerprint"]=hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()
mailbox=state / "mailbox.request.json"; acknowledgement=state / "mailbox.response.json"
deadline=time.monotonic()+5
while mailbox.read_text().strip() and time.monotonic()<deadline: time.sleep(0.05)
if mailbox.read_text().strip(): raise SystemExit("mailbox broker occupée")
mailbox.write_text(json.dumps(value, sort_keys=True)); reply={}
while time.monotonic()<deadline:
    reply=json.loads(acknowledgement.read_text())
    if reply.get("request_id")==request_id: break
    time.sleep(0.05)
if not reply.get("accepted"):
    raise SystemExit(f"requête invalide contrôlée refusée au transport: {reply.get('error')}")
PY
deadline=$((SECONDS + 30))
while [ ! -s "$STATE/responses/$invalid_id.json" ] && [ "$SECONDS" -lt "$deadline" ]; do sleep 0.2; done
grep -q '"classification": "HOST_FAILURE"' "$STATE/responses/$invalid_id.json"

printf 'B\n' >"$fixture"
recovery_b="$($SCRIPT_DIR/recover-ppsspp-gui-artifacts.sh --source-request "$source_id" 2>>"$LOG")"
printf '%s\n' "$recovery_b" >>"$LOG"
grep -q '"worker_fixture_version": "B"' <<<"$recovery_b"

end_pid="$(/usr/bin/python3 - "$STATE/heartbeat.json" <<'PY'
import json, pathlib, sys
print(json.loads(pathlib.Path(sys.argv[1]).read_text())["supervisor_pid"])
PY
)"
read -r workers markers metrics manual_restarts confirmations < <(/usr/bin/python3 - "$STATE/GUI_BROKER.METRICS.json" <<'PY'
import json, pathlib, sys
value=json.loads(pathlib.Path(sys.argv[1]).read_text())
print(
    value["workers_spawned"], value["markers_validated"],
    value["metrics_files_collected"], value["manual_restart_count"],
    value["user_confirmation_prompts"],
)
PY
)
[ "$start_pid" = "$end_pid" ] || die "supervisor PID instable"
[ $((workers - start_workers)) -ge 7 ] || die "moins de sept workers frais"
[ "$markers" -ge 18 ] || die "18 marqueurs non validés"
[ "$metrics" -ge 12 ] || die "métriques non récupérées"
[ "$manual_restarts" -eq 0 ] || die "redémarrage manuel détecté"
[ "$confirmations" -eq 0 ] || die "confirmation utilisateur détectée"
printf '%s\n' \
  "READY_AUTONOMOUS_PPSSPP_GUI_BROKER supervisor_pid_stable=true workers_loaded_fresh=true markers_18_valid=true metrics_retrieved=true request_after_failure_valid=true manual_restart_count=0 user_confirmation_prompts_after_bootstrap=0"
