#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

BACKEND="${DUSKLIGHT_PPSSPP_BACKEND:-opengl}"
while [ "$#" -gt 0 ]; do
  case "$1" in
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

SOURCE="$(assert_project_path ".test-data/ppsspp/captures/root-review")"
DESTINATION="$(assert_project_path "artifacts/dusklight-psp-root-review")"
CONVERTER="$(assert_project_path "build/host/tools/psp5650_to_ppm")"
safe_mkdir build/host/tools
safe_mkdir artifacts/dusklight-psp-root-review
command -v sips >/dev/null 2>&1 ||
  die "sips est requis pour la conversion PNG locale"

c++ -std=c++20 -O2 -Wall -Wextra -Werror \
  "$PROJECT_ROOT/tools/psp5650_to_ppm.cpp" -o "$CONVERTER"
"$SCRIPT_DIR/run-ppsspp-dusklight-psp.sh" \
  --run --mode root_review --presentation game \
  --backend "$BACKEND" --transport auto --timeout 900

find "$DESTINATION" -type f -delete
capture_count=0
for raw in "$SOURCE"/[0-9][0-9]_*.5650; do
  [ -f "$raw" ] || die "aucun framebuffer root_review"
  base="$(basename "$raw" .5650)"
  ppm="$PROJECT_ROOT/.tmp/$base.ppm"
  "$CONVERTER" "$raw" "$ppm"
  sips -s format png "$ppm" --out "$DESTINATION/$base.png" >/dev/null
  rm -f -- "$ppm"
  cp -- "$SOURCE/$base.txt" "$DESTINATION/$base.txt"
  capture_count=$((capture_count + 1))
done
[ "$capture_count" -eq 10 ] ||
  die "le bundle root exige exactement 10 captures"
cp -- "$SOURCE/ROOT.REVIEW.METRICS" "$DESTINATION/ROOT.REVIEW.METRICS"

{
  printf '%s\n\n' '# Revue de l’ancrage root de Link'
  printf '%s\n\n' \
    'Captures PSP déterministes via le profil PPSSPP isolé. Acceptation manuelle : **pending**.'
  printf 'EBOOT SHA-256 : `%s`\n\n' \
    "$(shasum -a 256 "$PROJECT_ROOT/build/psp/dusklight/EBOOT.PBP" |
      awk '{print $1}')"
  printf '%s\n' \
    'Les captures 01–09 utilisent le profil `game` sans overlay. La capture 10 utilise le profil `debug` : rouge = origine acteur, vert = root bind, cyan = root final, blanc = liaison verticale.'
  printf '\n%s\n' '| Capture | Frame | Animation | Temps | Présentation | Root final |'
  printf '%s\n' '|---|---:|---|---:|---|---|'
  for metadata in "$DESTINATION"/[0-9][0-9]_*.txt; do
    name="$(awk -F= '$1=="name"{print $2}' "$metadata")"
    frame="$(awk -F= '$1=="frame"{print $2}' "$metadata")"
    animation="$(awk -F= '$1=="animation"{print $2}' "$metadata")"
    animation_time="$(awk -F= '$1=="animation_time"{print $2}' "$metadata")"
    presentation="$(awk -F= '$1=="presentation"{print $2}' "$metadata")"
    final_root="$(awk -F= '$1=="final_root"{print $2}' "$metadata")"
    printf '| [%s](%s) | %s | %s | %s | %s | `%s` |\n' \
      "$name" "$name" "$frame" "$animation" "$animation_time" \
      "$presentation" "$final_root"
  done
  printf '\n%s\n' '- `user_manual_acceptance=pending`'
} >"$DESTINATION/REVIEW.md"

printf 'DUSKLIGHT_LINK_ROOT_REVIEW_OK captures=10 manual_acceptance=pending\n'
