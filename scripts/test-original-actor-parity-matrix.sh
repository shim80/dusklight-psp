#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

MATRIX="$(assert_project_path \
  "reference/parity/original-source-actor-matrix.csv")"
MAP="$(assert_project_path "build/psp/dusklight/dusklight_psp.map")"
BUILD="$(assert_project_path "build/host/canonical-runtime")"
DATA="$(assert_project_path "build/assets/dusklight-psp/data")"

[ -s "$MAP" ] || die "map Allegrex canonique absente"
python3 -B - "$PROJECT_ROOT" "$MATRIX" "$MAP" <<'PY'
import csv
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
matrix_path = pathlib.Path(sys.argv[2])
map_text = pathlib.Path(sys.argv[3]).read_text(errors="replace")
with matrix_path.open(newline="", encoding="utf-8") as stream:
    rows = list(csv.DictReader(stream))
if len(rows) != 8:
    raise SystemExit(f"matrice originale incomplète: {len(rows)}")
if sum(int(row["canonical_placements"]) for row in rows) != 18:
    raise SystemExit("comptage des placements canoniques invalide")
for row in rows:
    source_path = root / row["source_file"]
    source = source_path.read_text(encoding="utf-8")
    if row["implementation_type"] != "ORIGINAL_SOURCE":
        raise SystemExit(f"type invalide: {row['source_name']}")
    if row["profile"] not in source or source_path.name not in map_text:
        raise SystemExit(f"preuve binaire/source absente: {row['source_name']}")
    if row["desktop_same_scenario_trace"] != "false":
        raise SystemExit("trace desktop comparable revendiquée sans preuve")
    if row["status"] == "MATCH":
        raise SystemExit("MATCH interdit sans trace desktop/PSP alignée")
PY

"$SCRIPT_DIR/test-object-pivot-audit.sh"
cmake --build "$BUILD" --target \
  original_scene_exit_host_test \
  original_tier_a_actor_host_test \
  original_tbox_switch_host_test
"$BUILD/original_scene_exit_host_test" \
  "$DATA/stages/D_MN10/R09/room.dpsc" \
  "$DATA/stages/D_MN10/R02/room.dpsc"
"$BUILD/original_tier_a_actor_host_test"
"$BUILD/original_tbox_switch_host_test"

printf '%s\n' \
  "ORIGINAL_ACTOR_PARITY_MATRIX_OK sources=8 canonical_placements=18 host_lifecycle=8 desktop_aligned=0 status=PARTIAL_PARITY"
