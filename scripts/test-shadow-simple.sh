#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

ROOMS="$PROJECT_ROOT/build/assets/room-transition/stages/D_MN10"
for room in R09 R02; do
  [ -f "$ROOMS/$room/room.dpcl" ] &&
    [ -f "$ROOMS/$room/room.dpsc" ] ||
    die "packages ombre absents : $room"
done

cmake -S "$PROJECT_ROOT/test/room-transition" \
  -B "$PROJECT_ROOT/build/host/room-transition" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/host/room-transition"
"$PROJECT_ROOT/build/host/room-transition/shadow_runtime_host_test" \
  "$ROOMS/R09" "$ROOMS/R02"

printf '%s\n' \
  "HOST_SHADOW_SIMPLE_OK rooms=2 dpcl_receivers=true"
