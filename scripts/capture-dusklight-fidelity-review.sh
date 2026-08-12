#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

NO_DEPS=false
PACKAGE_ONLY=false
BACKEND="${DUSKLIGHT_PPSSPP_BACKEND:-opengl}"
while [ "$#" -gt 0 ]; do
  case "$1" in
    --no-deps) NO_DEPS=true ;;
    --package-only)
      NO_DEPS=true
      PACKAGE_ONLY=true
      ;;
    --backend)
      shift
      [ "$#" -gt 0 ] || die "--backend exige une valeur"
      BACKEND="$1"
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
case "$BACKEND" in auto|opengl|vulkan) ;; *)
  die "backend invalide : $BACKEND"
esac

if [ "$NO_DEPS" = false ]; then
  [ -z "${DUSKLIGHT_ORCHESTRATOR_ACTIVE:-}" ] ||
    die "un enfant d'orchestrateur doit recevoir --no-deps"
  export DUSKLIGHT_PPSSPP_BACKEND="$BACKEND"
  exec "$SCRIPT_DIR/test-dusklight-psp-runtime.sh" \
    --target psp.canonical.fidelity_review
fi

SOURCE="$(assert_project_path \
  ".test-data/ppsspp/captures/fidelity")"
DESTINATION="$(assert_project_path \
  "artifacts/dusklight-psp-fidelity-review")"
CONVERTER="$(assert_project_path "build/host/tools/psp5650_to_ppm")"
safe_mkdir build/host/tools
safe_mkdir artifacts/dusklight-psp-fidelity-review
command -v sips >/dev/null 2>&1 ||
  die "sips est requis pour la conversion PNG locale"

c++ -std=c++20 -O2 -Wall -Wextra -Werror \
  "$PROJECT_ROOT/tools/psp5650_to_ppm.cpp" -o "$CONVERTER"
if [ "$PACKAGE_ONLY" = false ]; then
  "$SCRIPT_DIR/run-ppsspp-dusklight-psp.sh" \
    --run --mode fidelity_review --presentation game \
    --backend "$BACKEND" --timeout 900
fi

find "$DESTINATION" -type f -delete
capture_count=0
for raw in "$SOURCE"/*.5650; do
  [ -f "$raw" ] || die "aucun framebuffer fidelity_review"
  base="$(basename "$raw" .5650)"
  ppm="$PROJECT_ROOT/.tmp/$base.ppm"
  "$CONVERTER" "$raw" "$ppm"
  sips -s format png "$ppm" --out "$DESTINATION/$base.png" >/dev/null
  rm -f -- "$ppm"
  cp -- "$SOURCE/$base.txt" "$DESTINATION/$base.txt"
  capture_count=$((capture_count + 1))
done
[ "$capture_count" -eq 26 ] ||
  die "le bundle de fidélité exige exactement 26 captures"
cp -- "$SOURCE/pivot_review.csv" "$DESTINATION/pivot_review.csv"
cp -- "$SOURCE/movement_review.csv" "$DESTINATION/movement_review.csv"
cp -- "$SOURCE/FIDELITY.REVIEW.METRICS" \
  "$DESTINATION/FIDELITY.REVIEW.METRICS"

{
  printf '%s\n\n' '# Revue de fidélité Dusklight PSP'
  printf '%s\n\n' \
    'Captures automatiques déterministes. Acceptation manuelle : **pending**.'
  printf '%s\n' '| Capture | Frame | Room | Présentation | Position | Yaw | Animation | HUD |'
  printf '%s\n' '|---|---:|---|---|---|---:|---|---|'
  for metadata in "$DESTINATION"/[0-9][0-9]_*.txt; do
    name="$(awk -F= '$1=="name"{print $2}' "$metadata")"
    frame="$(awk -F= '$1=="frame"{print $2}' "$metadata")"
    room="$(awk -F= '$1=="room"{print $2}' "$metadata")"
    presentation="$(awk -F= '$1=="presentation"{print $2}' "$metadata")"
    position="$(awk -F= '$1=="position"{print $2}' "$metadata")"
    yaw="$(awk -F= '$1=="yaw"{print $2}' "$metadata")"
    animation="$(awk -F= '$1=="animation"{print $2}' "$metadata")"
    hud="$(awk -F= '$1=="hud_mode"{print $2}' "$metadata")"
    printf '| [%s](%s) | %s | %s | %s | `%s` | %s | %s | %s |\n' \
      "$name" "$name" "$frame" "$room" "$presentation" "$position" \
      "$yaw" "$animation" "$hud"
  done
  printf '\n- [Mesures du pivot](pivot_review.csv)\n'
  printf '%s\n' '- [Mesures du mouvement](movement_review.csv)'
  printf '%s\n' '- `user_manual_acceptance=pending`'
} >"$DESTINATION/REVIEW.md"

printf 'DUSKLIGHT_FIDELITY_REVIEW_OK captures=26 manual_acceptance=pending\n'
