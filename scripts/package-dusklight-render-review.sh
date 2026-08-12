#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

SOURCE="$(assert_project_path ".test-data/ppsspp/captures/render-review")"
DESTINATION="$(assert_project_path "artifacts/dusklight-psp-render-review")"
CONVERTER="$(assert_project_path "build/host/tools/psp5650_to_ppm")"
EBOOT="$(assert_project_path "build/psp/dusklight/EBOOT.PBP")"
MANIFEST="$(assert_project_path "build/assets/dusklight-psp/data/RESOURCE.MANIFEST")"

[ -f "$SOURCE/RENDER.REVIEW.METRICS" ] ||
  die "captures render-review absentes"
[ -f "$EBOOT" ] && [ -f "$MANIFEST" ] ||
  die "EBOOT ou manifeste absent"
command -v sips >/dev/null 2>&1 ||
  die "sips est requis pour la conversion PNG locale"

safe_mkdir build/host/tools
safe_mkdir artifacts/dusklight-psp-render-review
c++ -std=c++20 -O2 -Wall -Wextra -Werror \
  "$PROJECT_ROOT/tools/psp5650_to_ppm.cpp" -o "$CONVERTER"
find "$DESTINATION" -type f -delete

capture_count=0
for raw in "$SOURCE"/[0-9][0-9]_*.5650; do
  [ -f "$raw" ] || die "aucun framebuffer render-review"
  base="$(basename "$raw" .5650)"
  ppm="$PROJECT_ROOT/.tmp/$base.ppm"
  "$CONVERTER" "$raw" "$ppm"
  sips -s format png "$ppm" --out "$DESTINATION/$base.png" >/dev/null
  rm -f -- "$ppm"
  cp -- "$SOURCE/$base.txt" "$DESTINATION/$base.txt"
  capture_count=$((capture_count + 1))
done
[ "$capture_count" -eq 28 ] ||
  die "le bundle de rendu exige exactement 28 captures"
cp -- "$SOURCE/RENDER.REVIEW.METRICS" \
  "$DESTINATION/RENDER.REVIEW.METRICS"

eboot_hash="$(shasum -a 256 "$EBOOT" | awk '{print $1}')"
manifest_hash="$(shasum -a 256 "$MANIFEST" | awk '{print $1}')"
movebg_receivers="$(awk -F= \
  '$1=="movebg_receiver_triangles"{print $2}' \
  "$DESTINATION/20_link_shadow_movebg.txt")"
{
  printf '%s\n\n' '# Revue du rendu Dusklight PSP'
  printf '%s\n\n' \
    'Captures PSP déterministes produites par PPSSPP depuis le profil isolé du projet.'
  printf '%s\n\n' \
    'Les images sont des framebuffers PSP 5650 convertis localement en PNG. Acceptation manuelle : **pending**.'
  printf '%s\n\n' \
    "- EBOOT SHA-256 : \`$eboot_hash\`" \
    "- RESOURCE.MANIFEST SHA-256 : \`$manifest_hash\`"
  printf '%s\n' \
    '| Capture | Frame | Room | Position | Animation | Root final | Environnement | Fog | Éclairage | Ombre |'
  printf '%s\n' \
    '|---|---:|---|---|---|---|---|---|---|---|'
  for metadata in "$DESTINATION"/[0-9][0-9]_*.txt; do
    name="$(awk -F= '$1=="name"{print $2}' "$metadata")"
    frame="$(awk -F= '$1=="frame"{print $2}' "$metadata")"
    room="$(awk -F= '$1=="room"{print $2}' "$metadata")"
    position="$(awk -F= '$1=="position"{print $2}' "$metadata")"
    animation="$(awk -F= '$1=="animation"{print $2}' "$metadata")"
    root="$(awk -F= '$1=="final_root"{print $2}' "$metadata")"
    environment="$(awk -F= '$1=="ambient_room"{print $2}' "$metadata")"
    fog="$(awk -F= '$1=="fog_enabled"{print $2}' "$metadata")"
    lighting="$(awk -F= '$1=="lighting_mode"{print $2}' "$metadata")"
    shadow="$(awk -F= '$1=="shadow_mode"{print $2}' "$metadata")"
    printf '| [%s](%s) | %s | %s | `%s` | %s | `%s` | %s | %s | %s | %s |\n' \
      "$name" "$name" "$frame" "$room" "$position" "$animation" \
      "$root" "$environment" "$fog" "$lighting" "$shadow"
  done
  printf '\n%s\n' \
    "- EBOOT hash commun à toutes les captures : \`$eboot_hash\`" \
    "- Package hash commun à toutes les captures : \`$manifest_hash\`" \
    '- `lighting=source_approx` et `shadows=projected_link` définissent le profil game.' \
    '- Les captures `debug_*` sont les seules à activer les visuels debug.' \
    "- La capture MoveBG rapporte \`movebg_receiver_triangles=$movebg_receivers\` ; une valeur nulle signifie qu’aucune plateforme pertinente n’était sous Link dans cette frame, pas que le test hôte MoveBG est absent." \
    '- `user_manual_acceptance=pending`'
} >"$DESTINATION/REVIEW.md"

printf '%s\n' \
  "DUSKLIGHT_RENDER_REVIEW_OK captures=28 manual_acceptance=pending"
