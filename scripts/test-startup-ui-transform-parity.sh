#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

MATRIX="$(assert_project_path "reference/parity/startup-ui-parity.csv")"
BUILD="$(assert_project_path "build/host/canonical-runtime")"
LINK_BUILD="$(assert_project_path "build/host/link-playable")"
DATA="$(assert_project_path "build/assets/dusklight-psp/data")"

"$SCRIPT_DIR/test-dusklight-startup-runtime.sh"
cmake -S "$PROJECT_ROOT/test/link-playable" \
  -B "$LINK_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$LINK_BUILD" --target dpui_v2_host_test
"$LINK_BUILD/dpui_v2_host_test" \
  "$DATA/common/hud.dpui" \
  "$PROJECT_ROOT/build/reports/startup-ui-hud-reference.ppm"

python3 -B - "$MATRIX" <<'PY'
import csv
import pathlib
import sys

with pathlib.Path(sys.argv[1]).open(newline="", encoding="utf-8") as stream:
    rows = list(csv.DictReader(stream))
if len(rows) != 14:
    raise SystemExit(f"matrice startup/UI incomplète: {len(rows)}")
if any(not row["pivot_source"] for row in rows):
    raise SystemExit("pivot UI/startup sans source")
if any(
    row["status"] == "MATCH"
    and row["surface"] not in {"new_game"}
    for row in rows
):
    raise SystemExit("MATCH global interdit avant recapture Functional")
if sum(row["status"] == "PARTIAL_PARITY" for row in rows) != 6:
    raise SystemExit("classification startup/UI inattendue")
PY

printf '%s\n' \
  "STARTUP_UI_TRANSFORM_PARITY_HOST_OK surfaces=14 partial=6 functional_capture=PENDING_GUI_EXECUTION"
