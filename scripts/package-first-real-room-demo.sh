#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

EBOOT="$(assert_project_path "build/psp/real-room-demo/EBOOT.PBP")"
LINK="$(assert_project_path "build/assets/link-playable")"
ROOM="$(assert_project_path "build/assets/first-real-room")"
DESTINATION="$(assert_project_path \
  "artifacts/psp-first-real-room-demo/PSP/GAME/DUSKLIGHT_REAL_ROOM")"
DATA="$DESTINATION/data"

[ -f "$EBOOT" ] || die "EBOOT real room absent"
safe_mkdir artifacts/psp-first-real-room-demo/PSP/GAME/DUSKLIGHT_REAL_ROOM/data
cp -- "$EBOOT" "$DESTINATION/EBOOT.PBP"
printf '%s' interactive >"$DESTINATION/ROOM.MODE"
for package in link.dpsk link.dptx link.dpan hud.dpui; do
  [ -f "$LINK/$package" ] || die "package Link absent : $package"
  cp -- "$LINK/$package" "$DATA/$package"
done
for package in room.dprm room.dptx room.dpcl room.dpsc ROOM.MANIFEST; do
  [ -f "$ROOM/$package" ] || die "package room absent : $package"
  cp -- "$ROOM/$package" "$DATA/$package"
done
printf '%s\n' "FIRST_REAL_ROOM_PACKAGE_OK destination=$DESTINATION"
