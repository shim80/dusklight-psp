#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"

[ "$#" -eq 1 ] || die "usage : $0 BENCHMARK.METRICS"
METRICS="$(assert_project_path "$1")"
[ -f "$METRICS" ] || die "métriques benchmark absentes"

value() {
  awk -F= -v key="$1" '$1 == key {print substr($0, length(key) + 2); exit}' \
    "$METRICS"
}

required=(
  benchmark_contract benchmark_version build_commit mode scene profile
  source_scene_fidelity native_width native_height warmup_frames
  measured_frames frame_time_us_average frame_time_us_median
  frame_time_us_p95 frame_time_us_p99 frame_time_us_worst
  fps_average fps_1_percent_low fps_0_1_percent_low
  cpu_time_us_average ge_submit_us_average ge_time_estimated_us_average
  memory_used_bytes_average memory_peak_bytes
  edram_used_bytes_average edram_peak_bytes
  draw_calls_average command_list_bytes_average command_list_bytes_p95
  actor_cost_us_average skinning_cost_us_average
  lighting_cost_us_average shadow_cost_us_average hud_cost_us_average
  transition_cost_us_average idle_motion_energy
  foot_contact_stability root_world_variance
  functional_capture_count framebuffer_hash_fnv1a
  allocations_during_frames validation_target error_code
)
for key in "${required[@]}"; do
  [ -n "$(value "$key")" ] || die "champ benchmark absent : $key"
done

[ "$(value benchmark_contract)" = DUSKLIGHT_PSP_RENDERING_CONTRACT_V1 ] ||
  die "contrat benchmark inattendu"
[ "$(value benchmark_version)" = 1 ] || die "version benchmark inattendue"
[ "$(value native_width)" = 480 ] &&
  [ "$(value native_height)" = 272 ] ||
  die "résolution benchmark non PSP"
[ "$(value error_code)" = 0 ] || die "benchmark en erreur"
[ "$(value allocations_during_frames)" = 0 ] ||
  die "allocations détectées pendant la mesure"
case "$(value profile)" in
  functional)
    [ "$(value framebuffer_readback)" = true ] &&
      [ "$(value captures)" = true ] &&
      [ "$(value functional_capture_count)" -eq 2 ] ||
      die "profil functional incomplet"
    ;;
  performance|psp_conservative)
    [ "$(value framebuffer_readback)" = false ] &&
      [ "$(value captures)" = false ] ||
      die "profil performance contaminé par une capture"
    ;;
  *) die "profil benchmark invalide" ;;
esac

printf 'DUSKLIGHT_BENCHMARK_METRICS_OK scene=%s profile=%s frames=%s\n' \
  "$(value scene)" "$(value profile)" "$(value measured_frames)"
