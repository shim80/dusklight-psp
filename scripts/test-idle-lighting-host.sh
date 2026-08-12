#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"

BUILD="$(assert_project_path "build/host/link-playable")"
DATA="$(assert_project_path "build/assets/dusklight-psp/data/common")"
cmake --build "$BUILD" -j2
args=("$DATA/link.dpsk" "$DATA/link.dptx" "$DATA/link.dpan" "$DATA/hud.dpui")
"$BUILD/link_dpan_pose_semantics_host_test" \
  "$DATA/link.dpsk" "$DATA/link.dpan" "$PROJECT_ROOT/.tmp/waits-dpan.csv"
"$BUILD/link_idle_foot_slip_host_test" "${args[@]}"
"$BUILD/render_color_packing_host_test"
"$BUILD/render_normal_pipeline_host_test" "${args[@]}"
"$BUILD/render_material_lighting_host_test" "$DATA/link.dptx"
"$BUILD/render_light_space_host_test"
"$BUILD/render_gu_state_isolation_host_test"
printf '%s\n' 'IDLE_LIGHTING_HOST_SUITE_OK tests=7'
