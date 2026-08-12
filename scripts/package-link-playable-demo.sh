#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

EBOOT="$(assert_project_path "build/psp/link-playable-demo/EBOOT.PBP")"
ASSETS="$(assert_project_path "build/assets/link-playable")"
DESTINATION="$(assert_project_path \
  "artifacts/psp-link-playable-demo/PSP/GAME/DUSKLIGHT_LINK_PLAYABLE")"
DATA="$DESTINATION/data"

[ -f "$EBOOT" ] || die "EBOOT jouable absent"
for package in link.dpsk link.dptx link.dpan hud.dpui PLAYABLE.MANIFEST; do
  [ -f "$ASSETS/$package" ] || die "package absent : $package"
done
safe_mkdir artifacts/psp-link-playable-demo/PSP/GAME/DUSKLIGHT_LINK_PLAYABLE/data
cp -- "$EBOOT" "$DESTINATION/EBOOT.PBP"
printf '%s' interactive >"$DESTINATION/PLAYABLE.MODE"
cp -- "$ASSETS/link.dpsk" "$DATA/link.dpsk"
cp -- "$ASSETS/link.dptx" "$DATA/link.dptx"
cp -- "$ASSETS/link.dpan" "$DATA/link.dpan"
cp -- "$ASSETS/hud.dpui" "$DATA/hud.dpui"
cp -- "$ASSETS/PLAYABLE.MANIFEST" "$DATA/PLAYABLE.MANIFEST"

printf '%s\n' \
  "LINK_PLAYABLE_PACKAGE_OK destination=$DESTINATION"
