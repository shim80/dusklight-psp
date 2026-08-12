#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

EBOOT="$(assert_project_path "build/psp/core-smoke/EBOOT.PBP")"
CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
[ -f "$EBOOT" ] || die "EBOOT core smoke historique absent"

request_id="$(timestamp_utc)-core-smoke"
"$SCRIPT_DIR/ppsspp-gui-runner-request.sh" --run \
  --request-id "$request_id" \
  --eboot "$EBOOT" \
  --game-id DUSKLIGHT_CORE_SMOKE \
  --config "$CONFIG" \
  --mode smoke \
  --presentation game \
  --backend opengl \
  --renderer software \
  --timeout 120 \
  --marker "CORE.OK=DUSKLIGHT_PSP_CORE_OK"

response="$(assert_project_path \
  ".test-data/ppsspp/gui-runner/requests/$request_id/response.json")"
marker="$(assert_project_path \
  ".test-data/ppsspp/gui-runner/sessions/$request_id/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_CORE_SMOKE/CORE.OK")"
[ -f "$response" ] && [ -f "$marker" ] ||
  die "réponse ou marqueur core smoke absent"
[ "$(cat "$marker")" = DUSKLIGHT_PSP_CORE_OK ] ||
  die "contenu core smoke historique invalide"
classification="$(/usr/bin/python3 - "$response" <<'PY'
import json
import pathlib
import sys
print(json.loads(pathlib.Path(sys.argv[1]).read_text())["classification"])
PY
)"
[ "$classification" = PSP_EBOOT_STARTED_AND_MARKERS_VALID ] ||
  [ "$classification" = MARKERS_VALID_METRICS_VALID ] ||
  die "classification core smoke invalide : $classification"

stable="$(assert_project_path \
  ".test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_CORE_SMOKE")"
safe_mkdir \
  .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_CORE_SMOKE
cp -- "$marker" "$stable/CORE.OK"
printf '%s\n' \
  "PSP_HISTORICAL_CORE_SMOKE_OK transport=gui marker=DUSKLIGHT_PSP_CORE_OK"
