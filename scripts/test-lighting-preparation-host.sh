#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

BUILD="$(assert_project_path "build/host/link-playable")"
DATA="$(assert_project_path "build/assets/dusklight-psp/data")"

cmake -S "$PROJECT_ROOT/test/link-playable" -B "$BUILD" \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD" --target lighting_preparation_host_test -j2

"$BUILD/lighting_preparation_host_test" \
  "$DATA/common/link.dpsk" \
  "$DATA/common/link.dptx" \
  "$DATA/common/link.dpan" \
  "$DATA/common/hud.dpui" \
  "$DATA/stages/F_SP108/R01/room.dprm" \
  "$DATA/stages/F_SP108/R01/room.dptx" \
  "$DATA/stages/F_SP108/R01/room.dpsc" \
  "$DATA/stages/D_MN10/R09/room.dprm" \
  "$DATA/stages/D_MN10/R09/room.dptx" \
  "$DATA/stages/D_MN10/R09/room.dpsc" \
  "$DATA/stages/D_MN10/R02/room.dprm" \
  "$DATA/stages/D_MN10/R02/room.dptx" \
  "$DATA/stages/D_MN10/R02/room.dpsc"

printf '%s\n' \
  'LIGHTING_PREPARATION_HOST_SUITE_OK tests=1 runtime_profile_unchanged=true'
