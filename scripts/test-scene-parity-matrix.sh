#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

TOOL="$(assert_project_path \
  "tools/dusk_scene_parity_matrix/dusk_scene_parity_matrix.py")"
OUTPUT="$(assert_project_path "build/reports/PARITY_SCENE_MATRIX.csv")"

python3 -B "$TOOL" --output "$OUTPUT" --self-test-negatives
python3 -B - "$OUTPUT" <<'PY'
import csv
import pathlib
import sys

with pathlib.Path(sys.argv[1]).open(newline="", encoding="utf-8") as stream:
    rows = list(csv.DictReader(stream))
if len(rows) != 40:
    raise SystemExit("matrice scène incomplète")
if any(row["actor_identity"] != "ParityActorId" for row in rows):
    raise SystemExit("identité scène/acteur non stable")
if any(row["status"] == "MATCH" for row in rows):
    raise SystemExit("MATCH scène interdit sans exécution PSP courante")
if sum(row["desktop_trace_current"] == "true" for row in rows) != 10:
    raise SystemExit("inventaire des traces desktop DTRC v3 inattendu")
if sum(row["psp_trace_current"] == "true" for row in rows) != 2:
    raise SystemExit("inventaire des traces PSP courantes inattendu")
PY

printf '%s\n' \
  "SCENE_PARITY_MATRIX_HOST_OK scenarios=40 desktop_current=10 psp_current=2 partial=40"
