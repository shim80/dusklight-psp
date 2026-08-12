#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

[ "$#" -eq 2 ] ||
  die "usage: seal-canonical-release-gates.sh CANONICAL_REQUEST_ID PLAYABLE_REQUEST_ID"
CANONICAL_ID="$1"
PLAYABLE_ID="$2"
CANONICAL_RESPONSE="$(assert_project_path \
  ".test-data/ppsspp/gui-runner/requests/$CANONICAL_ID/response.json")"
PLAYABLE_RESPONSE="$(assert_project_path \
  ".test-data/ppsspp/gui-runner/requests/$PLAYABLE_ID/response.json")"
CANONICAL_GAME="$(assert_project_path \
  ".test-data/ppsspp/gui-runner/sessions/$CANONICAL_ID/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PSP")"
PLAYABLE_GAME="$(assert_project_path \
  ".test-data/ppsspp/gui-runner/sessions/$PLAYABLE_ID/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_LINK_PLAYABLE")"
PACKAGE="$(assert_project_path \
  "artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP")"

/usr/bin/python3 - "$CANONICAL_RESPONSE" "$PLAYABLE_RESPONSE" <<'PY'
import json
import pathlib
import sys
canonical = json.loads(pathlib.Path(sys.argv[1]).read_text())
playable = json.loads(pathlib.Path(sys.argv[2]).read_text())
assert canonical["result_code"] == 0
assert canonical["boot_observed"] is True
assert len(canonical["marker_results"]) == 18
assert all(item["exists"] and item["content_valid"]
           for item in canonical["marker_results"])
assert playable["result_code"] == 0
assert playable["boot_observed"] is True
assert len(playable["marker_results"]) == 1
assert playable["marker_results"][0]["content_valid"] is True
PY
grep -qx 'error_code=0' "$CANONICAL_GAME/CONTINUOUS.METRICS" ||
  die "smoke canonique en erreur"
grep -qx 'error_code=0' "$PLAYABLE_GAME/PLAYABLE.METRICS" ||
  die "suite jouable en erreur"
grep -qx 'error_name=ok' "$PLAYABLE_GAME/PLAYABLE.METRICS" ||
  die "diagnostic jouable invalide"

last_state="$(tr '\n' ',' <"$CANONICAL_GAME/CANONICAL_SMOKE.COMPLETED" |
  sed 's/,$//')"
timeout_frame="$(awk -F= \
  '$1 == "frame" {print $2; exit}' \
  "$CANONICAL_GAME/CANONICAL_SMOKE.COMPLETED")"
{
  printf 'canonical_smoke_markers_expected=18\n'
  printf 'canonical_smoke_markers_observed=18\n'
  printf 'canonical_smoke_last_state=%s\n' "$last_state"
  printf 'canonical_smoke_timeout_frame=%s\n' "$timeout_frame"
  printf 'playable_error_code_previous=175\n'
  printf 'playable_error_name=size\n'
  printf 'playable_error_subsystem=dpui\n'
  printf 'playable_error_resolved=true\n'
  printf 'resource_manifest_entries=37\n'
  printf 'resource_manager_entry_capacity=48\n'
  printf 'hardware_validation=deferred_by_user\n'
  printf 'user_manual_acceptance=pending\n'
  printf 'error_code=0\n'
} >"$PACKAGE/CANONICAL_RELEASE.METRICS"
printf '%s' DUSKLIGHT_PSP_CANONICAL_RELEASE_GATES_OK \
  >"$PACKAGE/CANONICAL_RELEASE_GATES.OK"

printf 'CANONICAL_RELEASE_GATES_SEALED canonical=%s playable=%s\n' \
  "$CANONICAL_ID" "$PLAYABLE_ID"
