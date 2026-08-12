#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

SECTION=all
while [ "$#" -gt 0 ]; do
  case "$1" in
    --section)
      shift
      [ "$#" -gt 0 ] || die "--section exige une valeur"
      SECTION="$1"
      ;;
    --no-deps) ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
case "$SECTION" in link|hud|all) ;; *) die "section inconnue : $SECTION" ;; esac

safe_mkdir build/reports
if [ "$SECTION" = link ] || [ "$SECTION" = all ]; then
  "$SCRIPT_DIR/test-link-fidelity.sh" --target all --no-deps |
    tee "$PROJECT_ROOT/build/reports/link-fidelity-reference.txt"
fi
if [ "$SECTION" = hud ] || [ "$SECTION" = all ]; then
  [ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] ||
    die "DUSKLIGHT_GAME_IMAGE est requis pour l'inventaire HUD"
  "$SCRIPT_DIR/build-link-playable-assets.sh"
  cp -- "$PROJECT_ROOT/build/assets/link-playable/HUD.INVENTORY" \
    "$PROJECT_ROOT/build/reports/original-hud-inventory.txt"
  "$PROJECT_ROOT/build/host/link-playable/dpui_v2_host_test" \
    "$PROJECT_ROOT/build/assets/link-playable/hud.dpui" \
    "$PROJECT_ROOT/build/reports/original-hud-atlas.ppm"
fi
printf 'LINK_FIDELITY_ANALYSIS_OK section=%s\n' "$SECTION"
