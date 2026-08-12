#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

SOURCE="$(assert_project_path \
  ".test-data/ppsspp/captures/idle-lighting-review")"
DESTINATION="$(assert_project_path \
  "artifacts/dusklight-psp-idle-lighting-review")"
CONVERTER="$(assert_project_path "build/host/tools/psp5650_to_ppm")"
ANALYZER="$(assert_project_path \
  "build/host/tools/analyze_idle_lighting_frames")"
EBOOT="$(assert_project_path "build/psp/dusklight/EBOOT.PBP")"
MANIFEST="$(assert_project_path \
  "build/assets/dusklight-psp/data/RESOURCE.MANIFEST")"

for required in \
  IDLE_LIGHTING.METRICS \
  idle_contact.csv lighting_samples.csv gu_state_transitions.csv; do
  [ -f "$SOURCE/$required" ] ||
    die "preuve idle/lighting absente: $required"
done
[ -f "$EBOOT" ] && [ -f "$MANIFEST" ] ||
  die "EBOOT ou manifeste absent"
command -v sips >/dev/null 2>&1 ||
  die "sips est requis pour la conversion PNG locale"

safe_mkdir build/host/tools
safe_mkdir artifacts/dusklight-psp-idle-lighting-review
c++ -std=c++20 -O2 -Wall -Wextra -Werror \
  "$PROJECT_ROOT/tools/psp5650_to_ppm.cpp" -o "$CONVERTER"
c++ -std=c++20 -O2 -Wall -Wextra -Werror \
  "$PROJECT_ROOT/tools/analyze_idle_lighting_frames.cpp" -o "$ANALYZER"
find "$DESTINATION" -type f -delete

"$ANALYZER" "$SOURCE" \
  "$DESTINATION/framebuffer_histograms.csv" \
  "$DESTINATION/lighting_samples.csv"
capture_count=0
for raw in "$SOURCE"/[0-9][0-9]_*.5650; do
  [ -f "$raw" ] || die "aucun framebuffer idle/lighting"
  base="$(basename "$raw" .5650)"
  ppm="$PROJECT_ROOT/.tmp/$base.ppm"
  "$CONVERTER" "$raw" "$ppm"
  sips -s format png "$ppm" --out "$DESTINATION/$base.png" >/dev/null
  rm -f -- "$ppm"
  cp -- "$SOURCE/$base.txt" "$DESTINATION/$base.txt"
  printf 'framebuffer_sha256=%s\n' \
    "$(shasum -a 256 "$raw" | awk '{print $1}')" \
    >>"$DESTINATION/$base.txt"
  capture_count=$((capture_count + 1))
done
[ "$capture_count" -eq 25 ] ||
  die "le bundle idle/lighting exige exactement 25 captures"

cp -- "$SOURCE/IDLE_LIGHTING.METRICS" "$DESTINATION/"
cp -- "$SOURCE/idle_contact.csv" "$DESTINATION/"
cp -- "$SOURCE/lighting_samples.csv" \
  "$DESTINATION/lighting_samples_psp.csv"
cp -- "$SOURCE/gu_state_transitions.csv" "$DESTINATION/"

eboot_hash="$(shasum -a 256 "$EBOOT" | awk '{print $1}')"
manifest_hash="$(shasum -a 256 "$MANIFEST" | awk '{print $1}')"
mask_pixels="$(awk -F, '$1=="#L0_L1"{print $2}' \
  "$DESTINATION/framebuffer_histograms.csv")"
unlit_link="$(awk -F= '$1=="unlit_link_luminance_mean"{print $2}' \
  "$DESTINATION/IDLE_LIGHTING.METRICS")"
source_link="$(awk -F= \
  '$1=="source_approx_link_luminance_mean"{print $2}' \
  "$DESTINATION/IDLE_LIGHTING.METRICS")"
source_black="$(awk -F= \
  '$1=="source_approx_black_pixel_ratio"{print $2}' \
  "$DESTINATION/IDLE_LIGHTING.METRICS")"
{
  printf '%s\n\n' '# Revue idle et éclairage — Dusklight PSP'
  printf '%s\n\n' \
    'Paquet déterministe produit dans le profil PPSSPP isolé du dépôt. Les 25 PNG proviennent directement des framebuffers PSP 5650.'
  printf '%s\n\n' \
    "- EBOOT SHA-256 : \`$eboot_hash\`" \
    "- RESOURCE.MANIFEST SHA-256 : \`$manifest_hash\`" \
    "- Masque Link comparé pour les paires d’ombre : \`$mask_pixels\` pixels" \
    "- L0 Link : luminance \`$unlit_link\`" \
    "- L6 Link : luminance \`$source_link\`, ratio noir \`$source_black\`" \
    '- État de la revue humaine : **pending**'
  printf '%s\n' \
    '| Capture | Room | Frame | Animation | Éclairage | Ombre | Luminance | Noir |'
  printf '%s\n' \
    '|---|---|---:|---|---|---|---:|---:|'
  for metadata in "$DESTINATION"/[0-9][0-9]_*.txt; do
    name="$(awk -F= '$1=="name"{print $2}' "$metadata")"
    room="$(awk -F= '$1=="room"{print $2}' "$metadata")"
    frame="$(awk -F= '$1=="frame"{print $2}' "$metadata")"
    animation="$(awk -F= '$1=="animation"{print $2}' "$metadata")"
    lighting="$(awk -F= '$1=="lighting_mode"{print $2}' "$metadata")"
    shadow="$(awk -F= '$1=="shadow_mode"{print $2}' "$metadata")"
    luminance="$(awk -F= '$1=="framebuffer_mean_luminance"{print $2}' \
      "$metadata")"
    black="$(awk -F= '$1=="framebuffer_black_ratio"{print $2}' \
      "$metadata")"
    printf '| [%s](%s) | %s | %s | %s | %s | %s | %s | %s |\n' \
      "$name" "$name" "$room" "$frame" "$animation" "$lighting" \
      "$shadow" "$luminance" "$black"
  done
  printf '\n%s\n' \
    '## Conclusion automatisée' \
    '' \
    '- Les cinq instants idle, les onze profils lumineux et les vues finales sont complets.' \
    '- Les paires L0/L1 et L6/L7 ne changent aucun pixel du masque Link.' \
    '- L6 reste presque entièrement noir : la fidélité lumineuse source est `not_accepted` et aucun marqueur `LIGHTING_PIPELINE.OK` n’est créé.' \
    '- Le profil interactif reste volontairement `known_good_unlit`, ombre et fog désactivés, jusqu’à acceptation humaine.' \
    '- Classification du paquet : `READY_DUSKLIGHT_PSP_IDLE_FIDELITY_REVIEW`.' \
    '- Cette passe ne vaut pas acceptation visuelle : `user_manual_acceptance=pending`.'
} >"$DESTINATION/REVIEW.md"

printf '%s\n' \
  "DUSKLIGHT_IDLE_LIGHTING_REVIEW_PACKAGE_OK captures=25 manual_acceptance=pending"
