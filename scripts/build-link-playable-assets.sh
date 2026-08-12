#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] ||
  die "DUSKLIGHT_GAME_IMAGE est requis"
[ -f "$DUSKLIGHT_GAME_IMAGE" ] ||
  die "image de jeu introuvable"
[ ! -L "$DUSKLIGHT_GAME_IMAGE" ] ||
  die "un lien symbolique n'est pas accepté comme image source"

"$SCRIPT_DIR/verify-link-loader-sources.sh"
"$SCRIPT_DIR/build-link-loader-probe.sh"

OUTPUT="$PROJECT_ROOT/build/assets/link-playable"
FIRST="$PROJECT_ROOT/build/assets/link-playable-pass1"
SECOND="$PROJECT_ROOT/build/assets/link-playable-pass2"
mkdir -p "$OUTPUT" "$FIRST" "$SECOND"

convert_once() {
  local destination="$1"
  env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_DPSK_OUTPUT="$destination/link.dpsk" \
    DUSKLIGHT_DPTX_OUTPUT="$destination/link.dptx" \
    DUSKLIGHT_DPAN_OUTPUT="$destination/link.dpan" \
    DUSKLIGHT_DPUI_OUTPUT="$destination/hud.dpui" \
    DUSKLIGHT_HUD_INVENTORY_OUTPUT="$destination/HUD.INVENTORY" \
    "$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe" \
    >/dev/null
}

convert_once "$FIRST"
convert_once "$SECOND"
for package in link.dpsk link.dptx link.dpan hud.dpui HUD.INVENTORY; do
  cmp "$FIRST/$package" "$SECOND/$package"
  cp "$FIRST/$package" "$OUTPUT/$package"
done

cmake -S "$PROJECT_ROOT/test/link-playable" \
  -B "$PROJECT_ROOT/build/host/link-playable" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/host/link-playable"
"$PROJECT_ROOT/build/host/link-playable/playable_package_host_test" \
  "$OUTPUT/link.dpsk" \
  "$OUTPUT/link.dptx" \
  "$OUTPUT/link.dpan" \
  "$OUTPUT/hud.dpui"
"$PROJECT_ROOT/build/host/link-playable/dpui_v2_host_test" \
  "$OUTPUT/hud.dpui" \
  "$PROJECT_ROOT/build/reports/original-hud-atlas.ppm"

manifest="$OUTPUT/PLAYABLE.MANIFEST"
{
  printf '%s\n' \
    "format=PLAYABLE_MANIFEST_V1" \
    "dpsk_version=1" \
    "dptx_version=1" \
    "dpan_version=1" \
    "dpui_version=2" \
    "dpui_source_layout=zelda_game_image.blo" \
    "dpui_source_layout_size=604x448" \
    "dpui_source_archive_count=3" \
    "dpui_original_asset_count=20" \
    "dpui_source_font_count=1" \
    "dpui_procedural_game_sprites=0" \
    "joint_count=35" \
    "triangle_count=4329" \
    "texture_count=29" \
    "material_count=27" \
    "animation_count=4" \
    "texture_edram_budget=1150000"
  for package in link.dpsk link.dptx link.dpan hud.dpui HUD.INVENTORY; do
    printf '%s_size=%s\n' "$package" "$(stat -f %z "$OUTPUT/$package")"
    printf '%s_sha256=%s\n' \
      "$package" "$(shasum -a 256 "$OUTPUT/$package" | awk '{print $1}')"
  done
} >"$manifest"

printf '%s\n' "LINK_PLAYABLE_ASSETS_OK output=$OUTPUT"
