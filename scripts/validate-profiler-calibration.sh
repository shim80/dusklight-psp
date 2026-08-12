#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

ROOT="$(assert_project_path "artifacts/dusklight-startup-v2")"
PACKAGE="$(assert_project_path \
  "artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP")"
FUNCTIONAL="$ROOT/functional/title_flow.metrics"
PERFORMANCE="$ROOT/performance/title_flow.metrics"
CONSERVATIVE="$ROOT/psp_conservative/title_flow.metrics"
CALIBRATION_SCENE="$PACKAGE/PROFILER_CALIBRATION_SCENE.METRICS"

metric() {
  local file="$1" key="$2"
  awk -F= -v key="$key" \
    '$1 == key {print substr($0, length(key) + 2); exit}' "$file"
}

for file in "$FUNCTIONAL" "$PERFORMANCE" "$CONSERVATIVE"; do
  [ -f "$file" ] || die "métriques de calibration absentes : $file"
  [ "$(metric "$file" contract_id)" = \
      DUSKLIGHT_PSP_RENDERING_CONTRACT_V2_1 ] ||
    die "contrat v2.1 absent"
  [ "$(metric "$file" profiler_timer_source)" = \
      sceKernelGetSystemTimeWide ] ||
    die "source de timer invalide"
  [ "$(metric "$file" memory_peak_available)" = false ] &&
    [ "$(metric "$file" memory_peak_bytes)" = unavailable ] ||
    die "mémoire inconnue représentée de façon ambiguë"
  [ "$(metric "$file" duration_consistent)" = true ] ||
    die "cross-check de durée incohérent"
  [ "$(metric "$file" error_code)" = 0 ] ||
    die "benchmark en erreur"
done
[ -s "$CALIBRATION_SCENE" ] &&
  grep -q '^scene=ProfilerCalibrationScene$' "$CALIBRATION_SCENE" &&
  grep -q '^functional_readback_executed=true$' "$CALIBRATION_SCENE" &&
  grep -q '^coherent_variation=true$' "$CALIBRATION_SCENE" &&
  grep -q '^error_code=0$' "$CALIBRATION_SCENE" ||
  die "ProfilerCalibrationScene non validée"

[ "$(metric "$FUNCTIONAL" psp_renderer_effective)" = software ] &&
  [ "$(metric "$FUNCTIONAL" framebuffer_readback_enabled)" = true ] &&
  [ "$(metric "$FUNCTIONAL" capture_enabled)" = true ] ||
  die "profil Functional non effectif"
[ "$(metric "$PERFORMANCE" psp_renderer_effective)" = hardware ] &&
  [ "$(metric "$PERFORMANCE" framebuffer_readback_enabled)" = false ] ||
  die "profil Performance non effectif"
[ "$(metric "$CONSERVATIVE" psp_renderer_effective)" = hardware ] &&
  [ "$(metric "$CONSERVATIVE" conservative_settings_effective)" = true ] &&
  [ "$(metric "$CONSERVATIVE" fast_memory_effective)" = false ] &&
  [ "$(metric "$CONSERVATIVE" cpu_clock_config)" = 222 ] ||
  die "profil PSP conservative non effectif"

functional_hash="$(metric "$FUNCTIONAL" config_sha256)"
performance_hash="$(metric "$PERFORMANCE" config_sha256)"
conservative_hash="$(metric "$CONSERVATIVE" config_sha256)"
[ "$functional_hash" != "$performance_hash" ] &&
  [ "$performance_hash" != "$conservative_hash" ] &&
  [ "$functional_hash" != "$conservative_hash" ] ||
  die "configurations de profil non distinctes"

benchmark_runs=0
for profile in functional performance psp_conservative; do
  for scene in boot_intro title link_idle room_stress transition; do
    benchmark="$PROJECT_ROOT/artifacts/dusklight-psp-benchmarks/$profile/$scene.metrics"
    "$SCRIPT_DIR/validate-dusklight-benchmark-metrics.sh" "$benchmark" \
      >/dev/null
    benchmark_runs=$((benchmark_runs + 1))
  done
done

{
  printf 'contract_id=DUSKLIGHT_PSP_RENDERING_CONTRACT_V2_1\n'
  printf 'profiler_timer_source=sceKernelGetSystemTimeWide\n'
  printf 'profiler_calibration_valid=true\n'
  printf 'profiler_calibration_scene_valid=true\n'
  printf 'profiles_distinct=true\n'
  printf 'functional_renderer_effective=software\n'
  printf 'performance_renderer_effective=hardware\n'
  printf 'conservative_settings_effective=true\n'
  printf 'memory_peak_available=false\n'
  printf 'ge_time_available=%s\n' \
    "$(metric "$PERFORMANCE" ge_time_available)"
  printf 'duration_consistent=true\n'
  printf 'functional_config_sha256=%s\n' "$functional_hash"
  printf 'performance_config_sha256=%s\n' "$performance_hash"
  printf 'conservative_config_sha256=%s\n' "$conservative_hash"
  printf 'hardware_validation=deferred_by_user\n'
  printf 'user_manual_acceptance=pending\n'
  printf 'error_code=0\n'
} >"$PACKAGE/PROFILER_CALIBRATION.METRICS"
{
  printf 'contract_id=DUSKLIGHT_PSP_RENDERING_CONTRACT_V2_1\n'
  printf 'profiler_calibration_scene_valid=true\n'
  printf 'profiles_distinct=true\n'
  printf 'startup_profile_runs=6\n'
  printf 'benchmark_v1_compatibility_runs=%s\n' "$benchmark_runs"
  printf 'benchmark_v1_scenes=boot_intro,title,link_idle,room_stress,transition\n'
  printf 'functional_renderer_effective=software\n'
  printf 'performance_renderer_effective=hardware\n'
  printf 'conservative_settings_effective=true\n'
  printf 'duration_consistent=true\n'
  printf 'hardware_validation=deferred_by_user\n'
  printf 'user_manual_acceptance=pending\n'
  printf 'error_code=0\n'
} >"$PACKAGE/BENCHMARK_V2_1.METRICS"
printf '%s' DUSKLIGHT_PSP_BENCHMARK_V2_1_OK \
  >"$PACKAGE/BENCHMARK_V2_1.OK"

printf '%s\n' \
  "PROFILER_CALIBRATION_VALID contract=v2.1 profiles=functional,performance,psp_conservative"
