#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

EXPECTATION="${1:---expect-conflation}"
case "$EXPECTATION" in
  --expect-conflation|--expect-separated) ;;
  *) die "attente de vitesse Link inconnue : $EXPECTATION" ;;
esac

BUILD="$(assert_project_path "build/host/canonical-runtime")"
REPORT="$(assert_project_path "build/reports/causal-parity/link-move-speed-checkpoints.csv")"
mkdir -p "$(dirname -- "$REPORT")"
cmake -S "$PROJECT_ROOT/test/canonical-runtime" -B "$BUILD" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD" --target link_move_speed_checkpoint_host_test
"$BUILD/link_move_speed_checkpoint_host_test" "$EXPECTATION" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dprm" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dptx" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dpcl" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dpsc" \
  "$PROJECT_ROOT/build/reports/parity/link_run/desktop.dtrc-v3.jsonl" \
  "$REPORT"
echo "LINK_MOVE_SPEED_CHECKPOINT_REPORT_OK path=$REPORT"
