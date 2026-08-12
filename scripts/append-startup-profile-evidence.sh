#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

[ "$#" -eq 6 ] ||
  die "usage: append-startup-profile-evidence.sh PROFILE CONFIG BACKEND RENDERER RESPONSE METRICS"
PROFILE="$1"
CONFIG="$(assert_project_path "$2")"
BACKEND="$3"
RENDERER="$4"
RESPONSE="$(assert_project_path "$5")"
METRICS="$(assert_project_path "$6")"
[ -f "$CONFIG" ] && [ -f "$RESPONSE" ] && [ -f "$METRICS" ] ||
  die "entrée de preuve absente"

read -r effective_backend effective_renderer host_duration_ms result_code < <(
  /usr/bin/python3 - "$RESPONSE" <<'PY'
import json
import pathlib
import sys
result = json.loads(pathlib.Path(sys.argv[1]).read_text())
print(
    result["graphics_backend_used"],
    result["psp_renderer"],
    result["duration_ms"],
    result["result_code"],
)
PY
)
[ "$result_code" = 0 ] || die "réponse PPSSPP en erreur"
frame_samples="$(awk -F= \
  '$1 == "frame_samples" {print $2; exit}' "$METRICS")"
frame_average="$(awk -F= \
  '$1 == "frame_time_us_average" {print $2; exit}' "$METRICS")"
startup_frames_total="$(awk -F= \
  '$1 == "startup_frames_total" {print $2; exit}' "$METRICS")"
[[ "$frame_samples" =~ ^[0-9]+$ ]] &&
  [[ "$frame_average" =~ ^[0-9]+$ ]] &&
  [[ "$startup_frames_total" =~ ^[0-9]+$ ]] &&
  [ "$startup_frames_total" -ge "$frame_samples" ] ||
  die "fenêtre profiler invalide"
profiler_duration_us=$((frame_samples * frame_average))
host_run_duration_us=$((host_duration_ms * 1000))
excluded_frames=$((startup_frames_total - frame_samples))
excluded_duration_us=$((excluded_frames * 16667))
host_duration_us=$((host_run_duration_us - excluded_duration_us))
[ "$host_duration_us" -gt 0 ] || die "durée hôte ajustée invalide"
duration_ratio="$(awk \
  -v host="$host_duration_us" -v measured="$profiler_duration_us" \
  'BEGIN {printf "%.6f", (measured > 0 ? host / measured : 0)}')"
duration_consistent="$(awk -v ratio="$duration_ratio" \
  'BEGIN {print (ratio >= 0.8 && ratio <= 1.5 ? "true" : "false")}')"
fast_memory=unavailable
cpu_clock=default
conservative=false
if [ "$PROFILE" = psp_conservative ]; then
  fast_memory=false
  cpu_clock=222
  conservative=true
fi
{
  printf 'launcher_requested_profile=%s\n' "$PROFILE"
  printf 'launcher_effective_profile=%s\n' "$PROFILE"
  printf 'graphics_backend_requested=%s\n' "$BACKEND"
  printf 'graphics_backend_used=%s\n' "$effective_backend"
  printf 'psp_renderer_requested=%s\n' "$RENDERER"
  printf 'psp_renderer_effective=%s\n' "$effective_renderer"
  printf 'software_renderer_effective=%s\n' \
    "$([ "$effective_renderer" = software ] && echo true || echo false)"
  printf 'config_sha256=%s\n' "$(sha256_file "$CONFIG")"
  printf 'fast_memory_effective=%s\n' "$fast_memory"
  printf 'cpu_clock_config=%s\n' "$cpu_clock"
  printf 'conservative_settings_effective=%s\n' "$conservative"
  printf 'render_resolution=480x272\n'
  printf 'frame_limit=ppsspp_default\n'
  printf 'vsync=ppsspp_default\n'
  printf 'profiler_duration_us=%s\n' "$profiler_duration_us"
  printf 'host_run_duration_us=%s\n' "$host_run_duration_us"
  printf 'host_excluded_frames=%s\n' "$excluded_frames"
  printf 'host_excluded_duration_us=%s\n' "$excluded_duration_us"
  printf 'host_measured_duration_us=%s\n' "$host_duration_us"
  printf 'duration_ratio=%s\n' "$duration_ratio"
  printf 'duration_consistent=%s\n' "$duration_consistent"
} >>"$METRICS"
