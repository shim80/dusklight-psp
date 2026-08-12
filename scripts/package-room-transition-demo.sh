#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

EBOOT="$(assert_project_path "build/psp/room-transition-demo/EBOOT.PBP")"
COMMON="$(assert_project_path "build/assets/link-playable")"
ROOMS="$(assert_project_path "build/assets/room-transition")"
DESTINATION="$(assert_project_path \
  "artifacts/psp-room-transition-demo/PSP/GAME/DUSKLIGHT_ROOM_TRANSITION")"
DATA="$DESTINATION/data"

[ -f "$EBOOT" ] || die "EBOOT transition absent"
safe_mkdir artifacts/psp-room-transition-demo/PSP/GAME/DUSKLIGHT_ROOM_TRANSITION/data/common
safe_mkdir artifacts/psp-room-transition-demo/PSP/GAME/DUSKLIGHT_ROOM_TRANSITION/data/stages/D_MN10/R09
safe_mkdir artifacts/psp-room-transition-demo/PSP/GAME/DUSKLIGHT_ROOM_TRANSITION/data/stages/D_MN10/R02
cp -- "$EBOOT" "$DESTINATION/EBOOT.PBP"
printf '%s' interactive >"$DESTINATION/TRANSITION.MODE"
printf '%s' \
  "seed=0x4455534B route=source_triggers checkpoints=A,B,A acceleration=none" \
  >"$DESTINATION/TRANSITION.SCENARIO"
for package in link.dpsk link.dptx link.dpan hud.dpui; do
  [ -f "$COMMON/$package" ] || die "package common absent : $package"
  cp -- "$COMMON/$package" "$DATA/common/$package"
done
for room in R09 R02; do
  for package in room.dprm room.dptx room.dpcl room.dpsc ROOM.MANIFEST; do
    [ -f "$ROOMS/stages/D_MN10/$room/$package" ] ||
      die "package $room absent : $package"
    cp -- "$ROOMS/stages/D_MN10/$room/$package" \
      "$DATA/stages/D_MN10/$room/$package"
  done
done
printf 'ROOM_TRANSITION_PACKAGE_OK destination=%s\n' "$DESTINATION"
