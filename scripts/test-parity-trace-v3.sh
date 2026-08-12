#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

BUILD_DIR="$(assert_project_path "build/host/canonical-runtime")"
TMP="$(assert_project_path ".tmp/parity-trace-v3")"
COMPARE="$(assert_project_path \
  "tools/dusk_parity_compare/dusk_parity_compare.py")"
EXTRACT="$(assert_project_path \
  "tools/dusk_desktop_parity_trace/dusk_desktop_parity_trace.py")"
TOLERANCES="$(assert_project_path "reference/parity/tolerances.toml")"
safe_mkdir .tmp/parity-trace-v3

cmake -S "$PROJECT_ROOT/test/canonical-runtime" \
  -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD_DIR" --target parity_trace_host_test
"$BUILD_DIR/parity_trace_host_test"

python3 -B -m json.tool \
  "$PROJECT_ROOT/reference/parity/dtrc-v3.schema.json" >/dev/null
python3 -B -m json.tool \
  "$PROJECT_ROOT/reference/parity/dtrc-v3-1.schema.json" >/dev/null
python3 -B - "$TMP" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
event = {
    "schema": "dusklight.parity.dtrc.v3",
    "schema_version": 3,
    "run_id": "negative-host",
    "scenario_id": "link_walk",
    "frame": 1,
    "game_tick": 1,
    "scene_generation": 1,
    "scene_id": "scene:c6ddafee:9:0:11:1",
    "stage": "D_MN10",
    "room": 9,
    "layer": 0,
    "source_table": 748274432,
    "source_index": 0,
    "source_name_hash": 2382947721,
    "process_id": 253,
    "profile_id": 253,
    "actor_generation": 1,
    "model_resource_id": 0,
    "model_instance_id": 0,
    "event_type": "actor_transform",
    "payload": {
        "actor_id": "actor:c6ddafee:9:0:2c99c300:0:253:1",
        "current_position": [0.0, 450.0, -4650.0],
    },
}
encoded = json.dumps(event, sort_keys=True) + "\n"
(root / "desktop.jsonl").write_text(encoded)
(root / "same.jsonl").write_text(encoded)
different = json.loads(encoded)
different["payload"]["current_position"][0] = 100.0
(root / "different.jsonl").write_text(
    json.dumps(different, sort_keys=True) + "\n"
)
(root / "missing.jsonl").write_text("")
(root / "missing-field.jsonl").write_text(
    json.dumps({
        **event,
        "payload": {"actor_id": event["payload"]["actor_id"]},
    }, sort_keys=True) + "\n"
)
(root / "collision.jsonl").write_text(encoded + encoded)
v31 = json.loads(encoded)
v31.update({
    "schema": "dusklight.parity.dtrc.v3.1",
    "schema_revision": 1,
    "build_identity": "sha256:" + "0" * 64,
    "lifecycle_checkpoint": "frame_present",
})
v31_encoded = json.dumps(v31, sort_keys=True) + "\n"
(root / "v3-1.jsonl").write_text(v31_encoded)
(root / "v3-1-same.jsonl").write_text(v31_encoded)
(root / "v3-1-invalid.jsonl").write_text(
    json.dumps({
        key: value for key, value in v31.items()
        if key != "lifecycle_checkpoint"
    }, sort_keys=True) + "\n"
)
shadow = json.loads(encoded)
shadow["event_type"] = "actor_shadow"
shadow["payload"] = {
    "actor_id": event["payload"]["actor_id"],
    "origin": [0.0, 550.0, -4650.0],
}
causal_desktop = [shadow, event]
causal_psp = [json.loads(json.dumps(shadow)), json.loads(encoded)]
causal_psp[0]["payload"]["origin"][0] = 10.0
causal_psp[1]["payload"]["current_position"][0] = 20.0
(root / "causal-desktop.jsonl").write_text(
    "".join(json.dumps(item, sort_keys=True) + "\n"
            for item in causal_desktop)
)
(root / "causal-psp.jsonl").write_text(
    "".join(json.dumps(item, sort_keys=True) + "\n"
            for item in causal_psp)
)
pressed = {
    **event,
    "frame": 2,
    "game_tick": 2,
    "event_type": "input_change",
    "payload": {
        "actor_id": event["payload"]["actor_id"],
        "normalized": 0.518519,
        "stick_angle": 0,
    },
}
released = {
    **pressed,
    "frame": 3,
    "game_tick": 3,
    "payload": {
        "actor_id": event["payload"]["actor_id"],
        "normalized": 0.0,
        "stick_angle": 0,
    },
}
(root / "desktop-reference.log").write_text(
    "[REFTRACE] "
    + json.dumps({
        "schema": "dusklight.desktop.reference.v3",
        "type": "input_change",
        "pad_read": 120,
        "raw_stick": [0, 60],
    })
    + "\n[REFTRACE] "
    + json.dumps(pressed)
    + "\n[REFTRACE] "
    + json.dumps(released)
    + "\n"
)
PY

python3 -B "$EXTRACT" "$TMP/desktop-reference.log" \
  --trace "$TMP/extracted.jsonl" --metrics "$TMP/extracted-metrics.json"
python3 -B - "$TMP/extracted.jsonl" <<'PY'
import json
import pathlib
import sys

events = [
    json.loads(line)
    for line in pathlib.Path(sys.argv[1]).read_text().splitlines()
]
raw = [event["payload"]["raw_stick"] for event in events]
if raw != [[0, 60], [0, 0]]:
    raise SystemExit(f"corrélation raw_stick invalide: {raw}")
PY

python3 -B "$COMPARE" "$TMP/desktop.jsonl" "$TMP/same.jsonl" \
  --tolerances "$TOLERANCES" --output "$TMP/same.json"
python3 -B "$COMPARE" "$TMP/desktop.jsonl" "$TMP/different.jsonl" \
  --tolerances "$TOLERANCES" --output "$TMP/different.json"
python3 -B "$COMPARE" "$TMP/desktop.jsonl" "$TMP/missing.jsonl" \
  --tolerances "$TOLERANCES" --output "$TMP/missing.json"
python3 -B "$COMPARE" "$TMP/desktop.jsonl" "$TMP/missing-field.jsonl" \
  --tolerances "$TOLERANCES" --output "$TMP/missing-field.json"
python3 -B "$COMPARE" "$TMP/causal-desktop.jsonl" "$TMP/causal-psp.jsonl" \
  --tolerances "$TOLERANCES" --output "$TMP/causal.json"
python3 -B "$COMPARE" "$TMP/v3-1.jsonl" "$TMP/v3-1-same.jsonl" \
  --tolerances "$TOLERANCES" --output "$TMP/v3-1-same.json"
[ "$(python3 -B -c 'import json,sys;print(json.load(open(sys.argv[1]))["status"])' \
  "$TMP/same.json")" = MATCH_WITH_TOLERANCE ] ||
  die "auto-comparaison DTRC v3 invalide"
[ "$(python3 -B -c 'import json,sys;print(json.load(open(sys.argv[1]))["status"])' \
  "$TMP/different.json")" = PARTIAL_PARITY ] ||
  die "divergence numérique non détectée"
[ "$(python3 -B -c 'import json,sys;print(json.load(open(sys.argv[1]))["status"])' \
  "$TMP/missing.json")" = PARTIAL_PARITY ] ||
  die "événement manquant non détecté"
[ "$(python3 -B -c 'import json,sys;print(json.load(open(sys.argv[1]))["status"])' \
  "$TMP/missing-field.json")" = PARTIAL_PARITY ] ||
  die "champ de payload manquant non détecté"
grep -Fq 'payload_field_missing_on_psp' "$TMP/missing-field.json" ||
  die "raison du champ de payload manquant absente"
[ "$(python3 -B -c 'import json,sys;print(json.load(open(sys.argv[1]))["first_causal_divergence"]["event_type"])' \
  "$TMP/causal.json")" = actor_transform ] ||
  die "ordre causal des divergences invalide"
if python3 -B "$COMPARE" "$TMP/collision.jsonl" "$TMP/same.jsonl" \
    --tolerances "$TOLERANCES" >"$TMP/collision.stdout" \
    2>"$TMP/collision.stderr"; then
  die "collision d'identité non détectée"
fi
grep -Fq 'stable identity collision' "$TMP/collision.stderr" ||
  die "raison de collision d'identité absente"
if python3 -B "$COMPARE" "$TMP/v3-1-invalid.jsonl" \
    "$TMP/v3-1-same.jsonl" --tolerances "$TOLERANCES" \
    >"$TMP/v3-1-invalid.stdout" 2>"$TMP/v3-1-invalid.stderr"; then
  die "checkpoint v3.1 absent non détecté"
fi
grep -Fq 'missing lifecycle checkpoint' "$TMP/v3-1-invalid.stderr" ||
  die "raison du checkpoint v3.1 absent manquante"
[ "$(python3 -B -c 'import json,sys;print(json.load(open(sys.argv[1]))["status"])' \
  "$TMP/v3-1-same.json")" = MATCH_WITH_TOLERANCE ] ||
  die "auto-comparaison DTRC v3.1 invalide"

printf '%s\n' \
  "PARITY_TRACE_V3_1_TEST_OK legacy_v3=true additive_v3_1=true lifecycle_checkpoint=true exact=true numeric_negative=true missing_event_negative=true missing_field_negative=true identity_collision_negative=true raw_stick_correlation=true causal_order=true"
