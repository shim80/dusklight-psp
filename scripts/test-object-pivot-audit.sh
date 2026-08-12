#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

BUILD="$(assert_project_path "build/host/canonical-runtime")"
DATA="$(assert_project_path "build/assets/dusklight-psp/data")"
AUDIT="$(assert_project_path \
  "tools/dusk_object_pivot_audit/dusk_object_pivot_audit.py")"
OUTPUT="$(assert_project_path "build/reports/object-pivot-audit.json")"

python3 -B "$AUDIT" --output "$OUTPUT" --self-test-negatives
python3 -B - "$OUTPUT" <<'PY'
import json
import pathlib
import sys

audit = json.loads(pathlib.Path(sys.argv[1]).read_text())
required = {
    "rigid_model_resources": 7,
    "reference_vertices": 42,
    "vertices_recentered": 0,
    "bounds_centers_used_as_pivot": 0,
    "actor_transforms_baked": 0,
    "presentation_offsets_baked": 0,
    "unproven_actor_specific_offsets": 0,
    "movebg_collision_missing": 0,
    "cross_platform_matches": 0,
    "cross_platform_partial": 7,
}
for field, expected in required.items():
    if audit.get(field) != expected:
        raise SystemExit(
            f"audit objet invalide: {field}={audit.get(field)!r}, "
            f"attendu={expected!r}"
        )
if any(item["cross_platform_status"] == "MATCH" for item in audit["objects"]):
    raise SystemExit("MATCH interdit avant DTRC desktop et PSP")
PY

cmake -S "$PROJECT_ROOT/test/canonical-runtime" \
  -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD" --target \
  original_rendered_actor_host_test \
  original_dynamic_actor_host_test \
  original_door_host_test \
  original_spinner_switch_host_test \
  original_tbox_host_test

"$BUILD/original_rendered_actor_host_test" \
  "$DATA/objects/L4HsMato/model.dprm" \
  "$DATA/objects/L4HsMato/textures.dptx" \
  "$DATA/objects/L4HsMato/collision.dpcl" \
  "$DATA/stages/D_MN10/R09/room.dpsc" \
  "$DATA/stages/D_MN10/R02/room.dpsc"
"$BUILD/original_dynamic_actor_host_test" \
  "$DATA/objects/P_Gear/small/model.dprm" \
  "$DATA/objects/P_Gear/small/textures.dptx" \
  "$DATA/objects/P_Gear/large/model.dprm" \
  "$DATA/objects/P_Gear/large/textures.dptx" \
  "$DATA/stages/D_MN10/R09/room.dpsc"
"$BUILD/original_door_host_test" \
  "$DATA/objects/L4R02Gate/model.dprm" \
  "$DATA/objects/L4R02Gate/textures.dptx" \
  "$DATA/objects/L4R02Gate/collision.dpcl" \
  "$DATA/stages/D_MN10/R02/room.dpsc"
"$BUILD/original_spinner_switch_host_test" "$DATA"
"$BUILD/original_tbox_host_test" \
  "$DATA" "$DATA/stages/D_MN10/R02/room.dpsc"

printf '%s\n' \
  "OBJECT_PIVOT_HOST_SUITE_OK models=7 source_classes=5 landmarks=42 negative_cases=7 cross_platform=PARTIAL_PARITY"
