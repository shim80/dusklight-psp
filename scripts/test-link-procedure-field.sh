#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

BUILD="$(assert_project_path "build/host/canonical-runtime")"
REPORT="$(assert_project_path "build/reports/link-procedure")"
VANILLA="$(assert_project_path \
  ".tools/reference/dusklight-desktop/source-vanilla")"
safe_mkdir build/reports/link-procedure

cmake -S "$PROJECT_ROOT/test/canonical-runtime" -B "$BUILD" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD" --target link_procedure_checkpoint_host_test
"$BUILD/link_procedure_checkpoint_host_test" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dprm" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dptx" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dpcl" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dpsc" \
  "$REPORT/psp-checkpoints.csv"

python3 -B \
  "$PROJECT_ROOT/tools/dusk_link_procedure_audit/dusk_link_procedure_audit.py" \
  --desktop-header "$VANILLA/include/d/actor/d_a_alink.h" \
  --desktop-source "$VANILLA/src/d/actor/d_a_alink.cpp" \
  --desktop-trace \
    "$PROJECT_ROOT/build/reports/parity/link_idle_full_cycle/desktop.dtrc-v3.jsonl" \
  --psp-trace \
    "$PROJECT_ROOT/build/reports/parity/link_idle_full_cycle/psp.dtrc-v3.jsonl" \
  --psp-checkpoints "$REPORT/psp-checkpoints.csv" \
  --timeline "$REPORT/procedure-timeline.csv" \
  --output "$REPORT/procedure-audit.json"

! rg -n 'procedure[[:space:]]*=[[:space:]]*3' \
  "$PROJECT_ROOT/dusklight-main/platforms/psp" \
  "$PROJECT_ROOT/test/room-transition" >/dev/null ||
  die "procédure source hardcodée dans le runtime PSP"
! rg -n '\"procedure\"[[:space:]]*:[[:space:]]*%u' \
  "$PROJECT_ROOT/test/room-transition" >/dev/null ||
  die "motion phase encore présentée comme procédure"

printf '%s\n' \
  "LINK_PROCEDURE_FIELD_HOST_TEST_OK classification=RUNTIME_SOURCE_STATE_IMPLEMENTED hardcoded=false"
