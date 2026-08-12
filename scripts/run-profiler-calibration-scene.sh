#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

ACTION="${1:---plan}"
case "$ACTION" in --plan|--run) ;; *) die "usage: run-profiler-calibration-scene.sh [--plan|--run]" ;; esac

BUILD="$(assert_project_path "build/psp/profiler-calibration")"
EBOOT="$BUILD/EBOOT.PBP"
printf '%s\n' \
  "PROFILER_CALIBRATION_PLAN scene=ProfilerCalibrationScene renderer=software backend=opengl"
[ "$ACTION" = --run ] || exit 0

bash -c \
  '. scripts/env.sh >/dev/null && psp-cmake -S test/profiler-calibration -B build/psp/profiler-calibration -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build/psp/profiler-calibration'
[ -f "$EBOOT" ] || die "EBOOT calibration absent"

request_id="$(timestamp_utc)-profiler-calibration"
"$SCRIPT_DIR/ppsspp-gui-runner-request.sh" --run \
  --request-id "$request_id" \
  --eboot "$EBOOT" \
  --game-id DUSKLIGHT_PROFILER_CALIBRATION \
  --config "$(assert_project_path "test/gu-smoke/ppsspp-software.ini")" \
  --mode smoke \
  --presentation debug \
  --backend opengl \
  --renderer software \
  --timeout 120 \
  --marker \
    "PROFILER_CALIBRATION.OK=DUSKLIGHT_PSP_PROFILER_CALIBRATION_OK"

game_dir="$(assert_project_path \
  ".test-data/ppsspp/gui-runner/sessions/$request_id/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PROFILER_CALIBRATION")"
metrics="$game_dir/PROFILER_CALIBRATION.METRICS"
[ -s "$metrics" ] &&
  grep -q '^scene=ProfilerCalibrationScene$' "$metrics" &&
  grep -q '^coherent_variation=true$' "$metrics" &&
  grep -q '^error_code=0$' "$metrics" ||
  die "métriques de microbenchmark invalides"
cp -- "$metrics" \
  "$(assert_project_path "artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP/PROFILER_CALIBRATION_SCENE.METRICS")"
printf '%s\n' \
  "PROFILER_CALIBRATION_SCENE_OK transport=persistent_gui_broker profile=functional"
