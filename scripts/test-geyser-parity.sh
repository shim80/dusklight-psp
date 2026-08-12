#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

BUILD="$(assert_project_path "build/host/real-actor")"
V1="$(assert_project_path "build/assets/first-real-room/room.dpsc")"
V2="$(assert_project_path "build/assets/first-real-actor/room.dpsc")"
SOURCE="$(assert_project_path \
  "dusklight-main/src/d/actor/d_a_obj_geyser.cpp")"
HEADER="$(assert_project_path \
  "dusklight-main/include/d/actor/d_a_obj_geyser.h")"
RUNTIME="$(assert_project_path \
  "dusklight-main/platforms/psp/src/actor_runtime.cpp")"

for proof in \
  'class daObjGeyser_c' \
  'g_profile_Obj_Geyser' \
  'actionOff2' \
  'actionOnWait2' \
  'actionOn2' \
  'actionDisappear'; do
  grep -q "$proof" "$HEADER" "$SOURCE" ||
    die "preuve geyser source absente : $proof"
done
for proof in \
  'update_reactive' \
  'update_periodic' \
  'emit_particle' \
  'draw_actor_backend'; do
  grep -q "$proof" "$RUNTIME" \
    "$PROJECT_ROOT/dusklight-main/platforms/psp/src/actor_render.cpp" ||
    die "surface geyser PSP absente : $proof"
done

cmake -S "$PROJECT_ROOT/test/real-actor" \
  -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD" --target real_actor_runtime_host_test
"$BUILD/real_actor_runtime_host_test" "$V1" "$V2"

printf '%s\n' \
  "GEYSER_PARITY_HOST_OK records=2 source_class=daObjGeyser_c psp_implementation=PROCEDURAL_FALLBACK logic=PARTIAL_PARITY representation=EXPECTED_PLATFORM_DIFFERENCE"
