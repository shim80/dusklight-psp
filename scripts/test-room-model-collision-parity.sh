#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

BUILD="$(assert_project_path "build/host/canonical-runtime")"
DATA="$(assert_project_path "build/assets/dusklight-psp/data/stages")"

cmake -S "$PROJECT_ROOT/test/canonical-runtime" \
  -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD" --target room_model_collision_parity_host_test

run_room() {
  local stage="$1" room_index="$2" room_name="$3" base
  base="$DATA/$stage/$room_name"
  "$BUILD/room_model_collision_parity_host_test" \
    "$stage" "$room_index" \
    "$base/room.dprm" \
    "$base/room.dptx" \
    "$base/room.dpcl" \
    "$base/room.dpsc"
}

run_room F_SP108 1 R01
run_room D_MN10 9 R09
run_room D_MN10 2 R02

printf '%s\n' \
  "ROOM_MODEL_COLLISION_PARITY_SUITE_OK rooms=3 local_status=MATCH_WITH_TOLERANCE cross_platform_status=PARTIAL_PARITY"
