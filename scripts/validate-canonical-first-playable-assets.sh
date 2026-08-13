#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

ASSET_ROOT="${1:-$(assert_project_path build/assets/dusklight-psp/data)}"
case "$ASSET_ROOT" in
  /*) ;;
  *) ASSET_ROOT="$(assert_project_path "$ASSET_ROOT")" ;;
esac

[ -d "$ASSET_ROOT" ] || die "canonical asset root absent: $ASSET_ROOT"
[ ! -L "$ASSET_ROOT" ] || die "canonical asset root symlink refused"
[ -s "$ASSET_ROOT/RESOURCE.MANIFEST" ] || die "RESOURCE.MANIFEST absent or empty"
[ ! -L "$ASSET_ROOT/RESOURCE.MANIFEST" ] || die "RESOURCE.MANIFEST symlink refused"

required=(
  "common/link.dpsk"
  "common/link.dptx"
  "common/link.dpan"
  "common/hud.dpui"
  "stages/F_SP108/R01/room.dprm"
  "stages/F_SP108/R01/room.dptx"
  "stages/F_SP108/R01/room.dpcl"
  "stages/F_SP108/R01/room.dpsc"
)

for relative in "${required[@]}"; do
  path="$ASSET_ROOT/$relative"
  [ -f "$path" ] || die "first-playable asset absent: data/$relative"
  [ -s "$path" ] || die "first-playable asset empty: data/$relative"
  [ ! -L "$path" ] || die "first-playable asset symlink refused: data/$relative"
done

printf 'DUSKLIGHT_PSP_FIRST_PLAYABLE_ASSETS_OK files=%u stage=F_SP108 room=1 start=21 layer=0\n' \
  "${#required[@]}"
