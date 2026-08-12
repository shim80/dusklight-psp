#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

variant=trace-relwithdebinfo
scenario=f_sp108_render_state_v1
trace_frames=4
stage=F_SP108
room=1
layer=13
timeout=180
invalid_hash=false
collect_existing=false
source_request=
trace_input=
request_id="$(timestamp_utc)-desktop-render-v1"
while [ "$#" -gt 0 ]; do
  case "$1" in
    --variant) variant="$2"; shift 2 ;;
    --scenario) scenario="$2"; shift 2 ;;
    --trace-frames) trace_frames="$2"; shift 2 ;;
    --stage) stage="$2"; shift 2 ;;
    --room) room="$2"; shift 2 ;;
    --layer) layer="$2"; shift 2 ;;
    --timeout) timeout="$2"; shift 2 ;;
    --request-id) request_id="$2"; shift 2 ;;
    --invalid-hash) invalid_hash=true; shift ;;
    --collect-existing) collect_existing=true; shift ;;
    --source-request) source_request="$2"; shift 2 ;;
    --trace-input) trace_input="$2"; shift 2 ;;
    *) die "argument desktop broker inconnu : $1" ;;
  esac
done
case "$variant" in vanilla-relwithdebinfo|trace-relwithdebinfo) ;;
  *) die "variante desktop broker invalide" ;;
esac
case "$scenario" in desktop_transport_selftest|f_sp108_render_state_v1) ;;
  *) die "scénario desktop broker invalide" ;;
esac
case "$request_id" in ''|*[!A-Za-z0-9._-]*) die "request_id invalide" ;;
esac

"$SCRIPT_DIR/ensure-ppsspp-gui-broker.sh" >/dev/null
STATE="$(assert_project_path .test-data/ppsspp-gui-broker)"
session_id="$request_id"
[ -z "$source_request" ] || session_id="$source_request"
case "$session_id" in ''|*[!A-Za-z0-9._-]*) die "source_request invalide" ;;
esac
SESSION_REL=".tools/dusklight-ppsspp-gui-broker/DusklightPpssppGuiBroker.app/Contents/Resources/desktop-sessions/$session_id"
SESSION="$(assert_project_path "$SESSION_REL")"
safe_mkdir "$SESSION_REL/home"
safe_mkdir "$SESSION_REL/profile"
safe_mkdir "$SESSION_REL/cache"
safe_mkdir "$SESSION_REL/tmp"
"$SCRIPT_DIR/prepare-dusklight-desktop-input-profile.sh" --home "$SESSION/home" >/dev/null
INPUTS="$SESSION/inputs"
safe_mkdir "$SESSION_REL/inputs"
SOURCE_APP="$(assert_project_path "build/reference-desktop/$variant/Dusklight.app")"
STAGED_APP="$INPUTS/Dusklight.app"
/usr/bin/ditto "$SOURCE_APP" "$STAGED_APP"
cp -- "$(assert_project_path reference/desktop/reference-source.lock)" \
  "$INPUTS/reference-source.lock"
cp -- "$(assert_project_path reference/desktop/patches/0009-dusklight-reference-render-state-trace.patch)" \
  "$INPUTS/render-state-trace.patch"
SOURCE_DVD="$(assert_project_path "game iso/Legend of Zelda, The - Twilight Princess.iso")"
[ -e "$INPUTS/twilight-princess.iso" ] || /bin/ln "$SOURCE_DVD" "$INPUTS/twilight-princess.iso"

metadata="$(/usr/bin/python3 - "$PROJECT_ROOT" "$STATE" "$SESSION" "$variant" \
  "$scenario" "$trace_frames" "$stage" "$room" "$layer" "$timeout" \
  "$request_id" "$invalid_hash" "$collect_existing" "$trace_input" <<'PY'
import hashlib, importlib.util, json, pathlib, sys, time
from datetime import datetime, timezone

(root_s, state_s, session_s, variant, scenario, trace_frames_s, stage,
 room_s, layer_s, timeout_s, request_id, invalid_hash_s, collect_s, trace_input) = sys.argv[1:]
root=pathlib.Path(root_s).resolve(); state=pathlib.Path(state_s); session=pathlib.Path(session_s)
source=root / "tools/macos/dusklight-ppsspp-gui-broker/dusklight_desktop_application_adapter.py"
spec=importlib.util.spec_from_file_location("desktop_adapter_hash", source)
adapter=importlib.util.module_from_spec(spec); spec.loader.exec_module(adapter)
source_app=root / f"build/reference-desktop/{variant}/Dusklight.app"
app=session / "inputs/Dusklight.app"
exe=app / "Contents/MacOS/Dusklight"
patch=session / "inputs/render-state-trace.patch"
source_lock=session / "inputs/reference-source.lock"
dvd=session / "inputs/twilight-princess.iso"
if adapter.tree_sha256(source_app) != adapter.tree_sha256(app):
    raise SystemExit("copie du bundle desktop non identique")
visual={}
for line in (root / "build/reports/VISUAL_BUILD_ID.metrics").read_text().splitlines():
    key, sep, value=line.partition("=")
    if sep: visual[key]=value
parity={}
for line in (root / "build/reports/PARITY_BUILD_ID.metrics").read_text().splitlines():
    key, sep, value=line.partition("=")
    if sep: parity[key]=value
fixture=root / ".tools/dusklight-ppsspp-gui-broker/DusklightPpssppGuiBroker.app/Contents/Resources/worker-version.fixture"
trace_frames=int(trace_frames_s); room=int(room_s); layer=int(layer_s); timeout=int(timeout_s)
expected=sorted(adapter.REQUIRED_EVENT_TYPES) if trace_frames else []
desktop={
 "request_version":1, "app_bundle_path":str(app), "app_bundle_sha256":adapter.tree_sha256(app),
 "desktop_executable_path":str(exe), "desktop_executable_sha256":adapter.sha256(exe),
 "desktop_source_commit":"1bae8a5e6a812217ca33ba533e707ecfa64b1553",
 "desktop_source_lock_path":str(source_lock),
 "desktop_source_lock_sha256":adapter.sha256(source_lock), "desktop_trace_patch_path":str(patch),
 "desktop_trace_patch_sha256":adapter.sha256(patch), "game_image_path":str(dvd),
 "game_image_identity":"GZ2P01-revision-0", "game_image_sha256":adapter.sha256(dvd),
 "game_image_disc_id":"GZ2P01", "game_image_revision":0,
 "profile_root":str(session/"profile"), "home_root":str(session/"home"),
 "cache_root":str(session/"cache"), "tmp_root":str(session/"tmp"),
 "application_support_root":str(session/"home/Library/Application Support/TwilitRealm/Dusklight"),
 "scenario_id":scenario, "stage":stage, "room":room, "layer":layer, "spawn":21,
 "source_event_start":"first_shape_draw", "source_event_end":"fourth_frame_end",
 "capture_frame_count":0, "trace_frame_count":trace_frames, "trace_enabled":trace_frames>0,
 "trace_output_path":str(session/"DESKTOP_RENDER_STATE.TRACE"),
 "trace_manifest_path":str(session/"DESKTOP_RENDER_STATE.MANIFEST"),
 "capture_enabled":False, "capture_output_path":str(session/"desktop.png"),
 "metrics_output_path":str(session/"DESKTOP_RENDER_STATE.METRICS"),
 "marker_output_path":str(session/"DESKTOP_RENDER_TRACE.OK"),
 "expected_markers":{"DESKTOP_RENDER_TRACE.OK":"DUSKLIGHT_DESKTOP_RENDER_TRACE_OK"} if trace_frames else {},
 "expected_trace_event_types":expected, "timeout_seconds":timeout,
 "result_path":str(session/"RESULT.json"), "stdout_path":str(session/"stdout.log"),
 "stderr_path":str(session/"stderr.log"), "crash_log_path":str(session/"crash.log"),
 "trace_input":trace_input, "trace_analog":"", "trace_scenario":"",
 "collect_existing_desktop_artifacts":collect_s=="true",
}
if invalid_hash_s=="true": desktop["desktop_executable_sha256"]="0"*64
fingerprint=hashlib.sha256(json.dumps(desktop,sort_keys=True).encode()+source.read_bytes()).hexdigest()
envelope={
 "protocol_version":3, "application_type":"dusklight_desktop_reference",
 "application_request":desktop, "request_id":request_id,
 "campaign_id":"desktop-visual-pipeline-v1", "scenario_id":scenario,
 "fingerprint":fingerprint, "parity_build_id":parity["parity_build_id"],
 "visual_build_id":visual["visual_build_id"], "identity_scope":"parity", "attempt":1,
 "created_at":datetime.now(timezone.utc).isoformat().replace("+00:00","Z"),
 "timeout":timeout, "worker_version_required":"3", "repository_root":str(root),
 "runner_request":{}, "collect_existing_artifacts":collect_s=="true",
 "artifact_expectations":{"minimum_metrics":0,"minimum_traces":0,"minimum_captures":0},
 "client_mirror":{"result_path":str(session/"RESULT.json"),"stdout_path":str(session/"stdout.log"),"stderr_path":str(session/"stderr.log")},
 "worker_version_fixture":str(fixture),
}
mailbox=state/"mailbox.request.json"; acknowledgement=state/"mailbox.response.json"
deadline=time.monotonic()+5
while mailbox.read_text().strip() and time.monotonic()<deadline: time.sleep(.05)
if mailbox.read_text().strip(): raise SystemExit("mailbox broker occupée")
mailbox.write_text(json.dumps(envelope,sort_keys=True))
reply={}
while time.monotonic()<deadline:
    reply=json.loads(acknowledgement.read_text())
    if reply.get("request_id")==request_id: break
    time.sleep(.05)
if not reply.get("accepted"): raise SystemExit(f"requête desktop refusée: {reply.get('error')}")
print(request_id, timeout, state/"responses"/f"{request_id}.json")
PY
)"
read -r request_id timeout response <<<"$metadata"
deadline=$((SECONDS + timeout + 60))
while [ ! -s "$response" ] && [ "$SECONDS" -lt "$deadline" ]; do sleep 0.2; done
[ -s "$response" ] || die "réponse desktop broker absente : $request_id"
cat "$response"
/usr/bin/python3 - "$response" <<'PY'
import json, pathlib, sys
raise SystemExit(0 if json.loads(pathlib.Path(sys.argv[1]).read_text()).get("result_code") == 0 else 1)
PY
