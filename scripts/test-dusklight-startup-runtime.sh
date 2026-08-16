#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

BUILD="$(assert_project_path "build/host/canonical-runtime")"
safe_mkdir "$BUILD"
cmake -S "$PROJECT_ROOT/test/canonical-runtime" \
  -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD" --target \
  startup_runtime_host_test startup_ui_host_test \
  startup_name_entry_host_test \
  startup_name_ui_host_test \
  startup_title_asset_host_test startup_camera_parity_host_test \
  startup_first_playable_host_test \
  frame_profiler_host_test
"$BUILD/startup_runtime_host_test"
"$BUILD/startup_name_entry_host_test"
"$BUILD/frame_profiler_host_test"
"$BUILD/startup_camera_parity_host_test"
if [ -f "$PROJECT_ROOT/build/assets/dusklight-startup/startup_logos.dpsu" ] &&
   [ -f "$PROJECT_ROOT/build/assets/dusklight-startup/title_ui.dpsu" ] &&
   [ -f "$PROJECT_ROOT/build/assets/dusklight-startup/file_select.dpsu" ]; then
  "$BUILD/startup_ui_host_test" \
    "$PROJECT_ROOT/build/assets/dusklight-startup/startup_logos.dpsu" \
    "$PROJECT_ROOT/build/assets/dusklight-startup/title_ui.dpsu" \
    "$PROJECT_ROOT/build/assets/dusklight-startup/file_select.dpsu"
fi
if [ -f "$PROJECT_ROOT/build/assets/dusklight-psp/data/startup/file_select.dpsu" ]; then
  "$BUILD/startup_name_ui_host_test" \
    "$PROJECT_ROOT/build/assets/dusklight-psp/data/startup/file_select.dpsu"
fi
if [ -f "$PROJECT_ROOT/build/assets/dusklight-startup/title_room.dprm" ]; then
  "$BUILD/startup_title_asset_host_test" \
    "$PROJECT_ROOT/build/assets/dusklight-startup/title_room.dprm" \
    "$PROJECT_ROOT/build/assets/dusklight-startup/title_room.dptx" \
    "$PROJECT_ROOT/build/assets/dusklight-startup/title_logo.dprm" \
    "$PROJECT_ROOT/build/assets/dusklight-startup/title_logo.dptx"
fi
if [ -f "$PROJECT_ROOT/build/assets/dusklight-startup/fsp108_room.dprm" ]; then
  "$BUILD/startup_first_playable_host_test" \
    "$PROJECT_ROOT/build/assets/dusklight-startup/fsp108_room.dprm" \
    "$PROJECT_ROOT/build/assets/dusklight-startup/fsp108_room.dptx" \
    "$PROJECT_ROOT/build/assets/dusklight-startup/fsp108_room.dpcl" \
    "$PROJECT_ROOT/build/assets/dusklight-startup/fsp108_room.dpsc"
fi

if git -C "$PROJECT_ROOT" ls-files -z |
  tr '\0' '\n' |
  rg -i '\.(iso|gcm|wbfs|dol|rel|arc|bmd|bdl|bck|btk|bpk|brk|bti|blo|bfn|thp|ast)$'
then
  die "ressource commerciale ou format source brut suivi par Git"
fi

printf '%s\n' \
  "DUSKLIGHT_STARTUP_RUNTIME_TESTS_OK package=DPST1 profiler=v1 raw_assets_tracked=false"
