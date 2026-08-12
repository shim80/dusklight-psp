#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
TMP="$(assert_project_path ".tmp/causal-parity-test")"
TOOL="$(assert_project_path \
  "tools/dusk_causal_parity/dusk_causal_parity.py")"
safe_mkdir .tmp/causal-parity-test

python3 -B - "$TMP" <<'PY'
import json, pathlib, sys
root = pathlib.Path(sys.argv[1])
build_id = "sha256:" + "1" * 64
identity = {
    "format": "DUSKLIGHT_PARITY_BUILD_ID_V2",
    "parity_build_id": build_id,
    "psp_source_commit": "2" * 40,
    "eboot_sha256": "3" * 64,
    "elf_sha256": "4" * 64,
    "resource_manifest_sha256": "5" * 64,
    "package_set_sha256": "6" * 64,
    "trace_schema_version": "DTRC_V3_1",
    "parity_contract_version": "DUSKLIGHT_DESKTOP_PSP_PARITY_CONTRACT_V1",
    "worker_sha256": "7" * 64,
    "scenario_set_sha256": "8" * 64,
}
(root / "identity.metrics").write_text(
    "".join(f"{key}={value}\n" for key, value in identity.items())
)
base = {
    "schema": "dusklight.parity.dtrc.v3.1", "schema_version": 3,
    "schema_revision": 1, "lifecycle_checkpoint": "frame_present",
    "run_id": "test", "scenario_id": "link_idle_full_cycle",
    "frame": 0, "game_tick": 0, "scene_generation": 1,
    "scene_id": "scene:test", "stage": "D_MN10", "room": 9,
    "layer": 0, "source_table": 1, "source_index": 0,
    "source_name_hash": 2, "process_id": 253, "profile_id": 253,
    "actor_generation": 1, "model_resource_id": 0,
    "model_instance_id": 0, "event_type": "actor_transform",
}
desktop = {**base, "build_identity": "desktop:" + "a" * 40,
           "payload": {"actor_id": "actor:test", "procedure": 3,
                       "current_position": [0, 0, 0]}}
psp = {**base, "build_identity": build_id,
       "payload": {"actor_id": "actor:test", "procedure": None,
                   "procedure_raw": None, "procedure_valid": False,
                   "current_position": [0, 0, 0]}}
for name, value in (("desktop", desktop), ("psp", psp)):
    (root / f"{name}.jsonl").write_text(json.dumps(value) + "\n")
desktop_tail = {**desktop, "frame": 1, "game_tick": 1,
                "payload": {**desktop["payload"], "procedure": 4}}
(root / "desktop.jsonl").write_text(
    json.dumps(desktop) + "\n" + json.dumps(desktop_tail) + "\n"
)
stale = {**psp, "build_identity": "sha256:" + "9" * 64}
(root / "stale.jsonl").write_text(json.dumps(stale) + "\n")
synthetic = {**psp, "payload": {**psp["payload"], "synthetic": True}}
(root / "synthetic.jsonl").write_text(json.dumps(synthetic) + "\n")
(root / "ambiguous.jsonl").write_text(
    json.dumps(psp) + "\n" + json.dumps(psp) + "\n"
)
PY

run_tool() {
  python3 -B "$TOOL" \
    --desktop "$TMP/desktop.jsonl" --psp "$1" \
    --scenario link_idle_full_cycle \
    --tolerances "$PROJECT_ROOT/reference/parity/tolerances.toml" \
    --contract DUSKLIGHT_DESKTOP_PSP_PARITY_CONTRACT_V1 \
    --identity "$TMP/identity.metrics" \
    --desktop-commit "$(printf 'a%.0s' {1..40})" \
    --maximum-ticks 1 \
    --output "$2"
}

run_tool "$TMP/psp.jsonl" "$TMP/output"
grep -q '"reason": "missing_source_state"' \
  "$TMP/output/earliest_causal_divergence.json" ||
  die "état source manquant non prioritaire"
for negative in stale synthetic ambiguous; do
  if run_tool "$TMP/$negative.jsonl" "$TMP/$negative-output" \
      >"$TMP/$negative.stdout" 2>"$TMP/$negative.stderr"; then
    die "preuve causale invalide acceptée : $negative"
  fi
done

printf '%s\n' \
  "CAUSAL_PARITY_ENGINE_HOST_TEST_OK stale_build_negative=true synthetic_negative=true ambiguous_identity_negative=true missing_source_state=true additive_provenance_ignored=true"
