#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

MATRIX="$(assert_project_path "reference/parity/camera-parity-matrix.csv")"
BUILD="$(assert_project_path "build/host/canonical-runtime")"

python3 -B - "$MATRIX" <<'PY'
import csv
import pathlib
import sys

with pathlib.Path(sys.argv[1]).open(newline="", encoding="utf-8") as stream:
    rows = list(csv.DictReader(stream))
if len(rows) != 10:
    raise SystemExit(f"matrice caméra incomplète: {len(rows)}")
if sum(row["status"] == "MATCH_WITH_TOLERANCE" for row in rows) != 1:
    raise SystemExit("seule la caméra titre peut être fermée cross-platform")
if any(row["status"] == "MATCH" for row in rows):
    raise SystemExit("MATCH caméra global interdit sans DTRC v3 alignée")
if any(not row["camera_source"] for row in rows):
    raise SystemExit("caméra sans autorité source")
PY

cmake -S "$PROJECT_ROOT/test/canonical-runtime" \
  -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD" --target \
  startup_camera_parity_host_test \
  link_fidelity_host_test \
  room_model_collision_parity_host_test
"$BUILD/startup_camera_parity_host_test"
"$SCRIPT_DIR/test-link-fidelity.sh" --target locomotion --no-deps
"$SCRIPT_DIR/test-room-model-collision-parity.sh"

printf '%s\n' \
  "CAMERA_PARITY_MATRIX_HOST_OK scenarios=10 closed=1 partial=9 psp_trace=PENDING_GUI_EXECUTION"
