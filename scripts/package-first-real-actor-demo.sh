#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

DEST="$PROJECT_ROOT/artifacts/psp-first-real-actor-demo/PSP/GAME/DUSKLIGHT_REAL_ACTOR"
safe_mkdir artifacts/psp-first-real-actor-demo/PSP/GAME/DUSKLIGHT_REAL_ACTOR/data
cp -- "$PROJECT_ROOT/build/psp/real-actor-demo/EBOOT.PBP" "$DEST/EBOOT.PBP"
printf '%s' interactive >"$DEST/ACTOR.MODE"
printf '%s' dual_geyser_source_records_v1 >"$DEST/ACTOR.SCENARIO"
for package in link.dpsk link.dptx link.dpan hud.dpui; do
  cp -- "$PROJECT_ROOT/build/assets/link-playable/$package" "$DEST/data/$package"
done
for package in room.dprm room.dptx room.dpcl; do
  cp -- "$PROJECT_ROOT/build/assets/first-real-room/$package" "$DEST/data/$package"
done
cp -- "$PROJECT_ROOT/build/assets/first-real-actor/room.dpsc" \
  "$DEST/data/room.dpsc"
cp -- "$PROJECT_ROOT/build/assets/first-real-actor/ACTOR.MANIFEST" \
  "$DEST/ACTOR.MANIFEST"
printf 'FIRST_REAL_ACTOR_PACKAGE_OK destination=%s\n' "$DEST"
