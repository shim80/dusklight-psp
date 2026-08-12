#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

ROOMS="$PROJECT_ROOT/build/assets/room-transition/stages/D_MN10"
if [ -n "${DUSKLIGHT_GAME_IMAGE:-}" ]; then
  "$SCRIPT_DIR/build-first-room-transition-assets.sh"
fi
for room in R09 R02; do
  [ -f "$ROOMS/$room/room.dpsc" ] ||
    die "package environnement absent : $room"
done

cmake -S "$PROJECT_ROOT/test/room-transition" \
  -B "$PROJECT_ROOT/build/host/room-transition" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/host/room-transition"
"$PROJECT_ROOT/build/host/room-transition/environment_runtime_host_test" \
  "$ROOMS/R09" "$ROOMS/R02"
"$PROJECT_ROOT/build/host/room-transition/room_transition_host_test" \
  "$ROOMS/R09" "$ROOMS/R02" >/dev/null

printf '%s\n' \
  "HOST_ENVIRONMENT_RUNTIME_OK records=2 transition_regression=true"
