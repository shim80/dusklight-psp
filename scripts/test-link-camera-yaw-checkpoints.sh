#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

EXPECTATION="${1:---expect-mismatch}"
case "$EXPECTATION" in
  --expect-mismatch|--expect-parity) ;;
  *) die "attente camera_yaw inconnue : $EXPECTATION" ;;
esac

BUILD="$(assert_project_path "build/host/canonical-runtime")"
REPORT="$(assert_project_path "build/reports/causal-parity/link-camera-yaw-checkpoints.csv")"
mkdir -p "$(dirname -- "$REPORT")"
cmake -S "$PROJECT_ROOT/test/canonical-runtime" -B "$BUILD" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD" --target link_camera_yaw_checkpoint_host_test
"$BUILD/link_camera_yaw_checkpoint_host_test" "$EXPECTATION" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dprm" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dptx" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dpcl" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dpsc" \
  "$PROJECT_ROOT/build/reports/parity/link_turn_180/desktop.dtrc-v3.jsonl" \
  "$REPORT"
echo "LINK_CAMERA_YAW_CHECKPOINT_REPORT_OK path=$REPORT"
