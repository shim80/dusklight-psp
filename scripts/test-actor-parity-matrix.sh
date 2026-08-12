#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

TOOL="$(assert_project_path \
  "tools/dusk_actor_parity_matrix/dusk_actor_parity_matrix.py")"
OUTPUT="$(assert_project_path "build/reports/PARITY_ACTOR_MATRIX.csv")"

python3 -B "$TOOL" --output "$OUTPUT" --self-test-negatives
python3 -B - "$OUTPUT" <<'PY'
import csv
import collections
import pathlib
import sys

with pathlib.Path(sys.argv[1]).open(newline="", encoding="utf-8") as stream:
    rows = list(csv.DictReader(stream))
counts = collections.Counter(row["implementation_type"] for row in rows)
expected = {
    "ORIGINAL_SOURCE": 18,
    "TRANSFORM_ONLY_ADAPTER": 9,
    "PROCEDURAL_FALLBACK": 2,
    "MISSING": 719,
}
if len(rows) != 748 or counts != expected:
    raise SystemExit(f"matrice acteur invalide: {len(rows)} {counts}")
if len({row["parity_actor_id"] for row in rows}) != len(rows):
    raise SystemExit("collision ParityActorId")
if sum(row["psp_trace_current"] == "true" for row in rows) != 10:
    raise SystemExit("couverture PSP native courante inattendue")
PY

printf '%s\n' \
  "ACTOR_PARITY_MATRIX_HOST_OK actors=748 original=18 transform_only=9 procedural=2 missing=719 psp_current=10 match=0"
