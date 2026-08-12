#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

CURRENT="$(assert_project_path "build/psp/dusklight/EBOOT.PBP")"
BENCH="$(assert_project_path "artifacts/dusklight-psp-benchmarks")"
PACKAGE="$(assert_project_path \
  "artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP")"
OUTPUT="$(assert_project_path \
  "build/reports/parity-performance-status.metrics")"

current_hash="$(shasum -a 256 "$CURRENT" | awk '{print $1}')"
package_hash="$(shasum -a 256 "$PACKAGE/EBOOT.PBP" | awk '{print $1}')"
profiles=0
metrics=0
current_profiles=0
for profile in functional performance psp_conservative; do
  manifest="$BENCH/$profile/RUN.MANIFEST"
  [ -s "$manifest" ] || die "manifest benchmark absent : $profile"
  grep -q '^transport=launchservices_gui$' "$manifest" ||
    die "transport benchmark non canonique : $profile"
  grep -q '^network_used=false$' "$manifest" ||
    die "réseau benchmark inattendu : $profile"
  recorded="$(awk -F= '$1 == "eboot_sha256" {print $2}' "$manifest")"
  [ "$recorded" != "$current_hash" ] || current_profiles=$((current_profiles + 1))
  profiles=$((profiles + 1))
  for scene in boot_intro title link_idle room_stress transition; do
    [ -s "$BENCH/$profile/$scene.metrics" ] ||
      die "métrique benchmark absente : $profile/$scene"
    metrics=$((metrics + 1))
  done
done
[ "$profiles" -eq 3 ] && [ "$metrics" -eq 15 ] ||
  die "baseline benchmark incomplète"
[ "$current_profiles" -eq 0 ] ||
  die "baseline inattendue pour le build courant"
[ "$package_hash" = "$current_hash" ] ||
  die "paquet canonique non réconcilié avec le build courant"
identity="$PROJECT_ROOT/build/reports/PARITY_BUILD_ID.metrics"
[ -s "$identity" ] || die "PARITY_BUILD_ID absent"
build_id="$(awk -F= '$1 == "parity_build_id" {print $2}' "$identity")"
[[ "$build_id" =~ ^sha256:[0-9a-f]{64}$ ]] ||
  die "PARITY_BUILD_ID invalide"
grep -q '^contract_id=DUSKLIGHT_PSP_RENDERING_CONTRACT_V2_1$' \
  "$PACKAGE/BENCHMARK_V2_1.METRICS" ||
  die "contrat v2.1 absent"

{
  printf 'current_eboot_sha256=%s\n' "$current_hash"
  printf 'packaged_eboot_sha256=%s\n' "$package_hash"
  printf 'parity_build_id=%s\n' "$build_id"
  printf 'baseline_profiles=3\n'
  printf 'baseline_scene_runs=15\n'
  printf 'current_profile_runs=0\n'
  printf 'benchmark_v1_regression=PENDING_GUI_EXECUTION\n'
  printf 'benchmark_v2_1_regression=PENDING_GUI_EXECUTION\n'
  printf 'performance_judgement=PENDING_GUI_EXECUTION\n'
  printf 'error_code=0\n'
} >"$OUTPUT"

printf '%s\n' \
  "PARITY_PERFORMANCE_PROVENANCE_OK baseline_runs=15 current_runs=0 status=PENDING_GUI_EXECUTION"
