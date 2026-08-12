#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

HOST_BUILD="$(assert_project_path "build/host/link-playable")"
REPORT="$(assert_project_path "build/reports/link-root-anchor-reference.csv")"
ASSETS="$(assert_project_path "build/assets/dusklight-psp/data/common")"

safe_mkdir build/host/link-playable
safe_mkdir build/reports
cmake -S "$PROJECT_ROOT/test/link-playable" -B "$HOST_BUILD" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$HOST_BUILD" \
  --target link_root_anchor_reference_host_test
"$HOST_BUILD/link_root_anchor_reference_host_test" \
  "$ASSETS/link.dpsk" \
  "$ASSETS/link.dptx" \
  "$ASSETS/link.dpan" \
  "$ASSETS/hud.dpui" \
  "$REPORT"

{
  rg -n -A 115 'int daAlink_c::jointControll' \
    "$PROJECT_ROOT/dusklight-main/src/d/actor/d_a_alink.cpp"
  rg -n -A 40 'void daAlink_c::resetRootMtx' \
    "$PROJECT_ROOT/dusklight-main/src/d/actor/d_a_alink.cpp"
  rg -n -A 55 'void sample_clip' \
    "$PROJECT_ROOT/dusklight-main/platforms/psp/src/playable_runtime.cpp"
} >"$PROJECT_ROOT/build/reports/link-root-anchor-source.txt"

printf 'DUSKLIGHT_LINK_ROOT_ANCHOR_ANALYSIS_OK csv=%s\n' \
  "${REPORT#"$PROJECT_ROOT"/}"
