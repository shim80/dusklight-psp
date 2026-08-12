#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

MODE=plan
TIMEOUT_SECONDS=90
while [ "$#" -gt 0 ]; do
  case "$1" in
    --plan) MODE=plan ;;
    --run) MODE=run ;;
    --timeout)
      shift
      [ "$#" -gt 0 ] || die "--timeout exige une valeur"
      TIMEOUT_SECONDS="$1"
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] ||
  die "timeout invalide"

EBOOT="$(assert_project_path "build/psp/link-demo/EBOOT.PBP")"
PACKAGE="$(assert_project_path "build/assets/link-demo/link.dpmd")"
STATE_ROOT="$(assert_project_path ".test-data/ppsspp")"
HOME_DIR="$STATE_ROOT/home"
CONFIG_HOME="$HOME_DIR/.config"
GAME_DIR="$CONFIG_HOME/ppsspp/PSP/GAME/DUSKLIGHT_LINK_DEMO"
DATA_DIR="$GAME_DIR/data"
EBOOT_DEST="$GAME_DIR/EBOOT.PBP"
PACKAGE_DEST="$DATA_DIR/link.dpmd"
MODE_FILE="$GAME_DIR/LINK_DEMO.MODE"
CONFIG_FILE="$GAME_DIR/LINK_BENCH.CONFIG"
DEMO_MARKER="$GAME_DIR/LINK_DEMO.OK"
BENCH_MARKER="$GAME_DIR/LINK_BENCH.OK"
METRICS="$GAME_DIR/LINK_BENCH.METRICS"
PPSSPP_CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
TOKEN="DUSKLIGHT_PSP_LINK_BENCH_OK"

find_ppsspp() {
  local candidate
  for candidate in \
    "${PPSSPP_BIN:-}" \
    "$PROJECT_ROOT/.tools/ppsspp/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL" \
    "$PROJECT_ROOT/.tools/ppsspp/PPSSPP.app/Contents/MacOS/PPSSPP" \
    "$PROJECT_ROOT/.tools/ppsspp/PPSSPP.AppImage"; do
    if [ -n "$candidate" ] && [ -f "$candidate" ]; then
      assert_project_path "$candidate"
      return
    fi
  done
  die "PPSSPP isolé absent"
}

metric() {
  awk -F= -v key="$1" \
    '$1 == key {print substr($0, length(key) + 2); exit}' "$METRICS"
}

scenario_metric() {
  local scenario="$1" key="$2"
  awk -F= -v scenario="$scenario" -v key="$key" '
    $1 == "scenario_begin" {inside = ($2 == scenario)}
    inside && $1 == key {print substr($0, length(key) + 2); exit}
    $1 == "scenario_end" && $2 == scenario {inside = 0}
  ' "$METRICS"
}

numeric() {
  [[ "$1" =~ ^[0-9]+$ ]]
}

validate_scenario() {
  local name="$1" culling="$2" key value
  [ "$(scenario_metric "$name" scenario_name)" = "$name" ] &&
    [ "$(scenario_metric "$name" culling_enabled)" = "$culling" ] &&
    [ "$(scenario_metric "$name" sample_count)" = 600 ] || return 1
  for key in \
    cpu_update_us adapter_us gu_submission_us ge_sync_us \
    presentation_us frame_total_us; do
    for suffix in min mean median p90 p95 p99 max \
      mean_absolute_deviation; do
      value="$(scenario_metric "$name" "${key}_${suffix}")"
      numeric "$value" || return 1
    done
  done
  for key in \
    frames_over_16667_us frames_over_33333_us \
    frames_over_50000_us frames_over_66667_us \
    command_list_bytes_min command_list_bytes_mean \
    command_list_bytes_max cache_writeback_operations_average \
    cache_invalidate_operations_average; do
    value="$(scenario_metric "$name" "$key")"
    numeric "$value" || return 1
  done
}

validate_result() {
  local key expected
  [ -f "$BENCH_MARKER" ] && [ ! -L "$BENCH_MARKER" ] &&
    [ "$(cat "$BENCH_MARKER")" = "$TOKEN" ] &&
    [ "$(wc -c <"$BENCH_MARKER" | tr -d ' ')" = 27 ] &&
    [ ! -e "$DEMO_MARKER" ] &&
    [ -f "$METRICS" ] && [ ! -L "$METRICS" ] || return 1
  while IFS='|' read -r key expected; do
    [ "$(metric "$key")" = "$expected" ] || return 1
  done <<'EOF'
benchmark_version|1
target_declared|ppsspp
run_label|ppsspp_functional
pose_contract|J3D_PURE_WAITS_FRAME0_NEUTRAL_HANDS_V1
mode|benchmark
triangle_count|4329
vertex_count|3543
index_count|12987
vertex_bytes|56688
index_bytes|25974
chunk_count|5
draw_call_count|5
package_bytes|83296
warmup_frames|120
measured_frames|600
pll_clock_mhz|unknown
framebuffer_format|GU_PSM_8888
depth_format|16-bit
visible_width|480
visible_height|272
stride|512
command_list_capacity|65536
vblank_wait_enabled|true
auto_rotation_speed|15
allocation_count_during_frame|0
guard_regions_valid|true
pixel_checks_before_valid|true
pixel_checks_after_valid|true
synchronization|complete
hardware_validation|false
diagnostic_only|true
error_code|0
EOF
  [ "$(metric package_crc_expected)" = \
    "$(metric package_crc_actual)" ] || return 1
  [ "$(metric dpmd_sha256_expected)" = \
    b15eb6a5a8e077a888462eb7574282ce8cf730ff5baea7a747863edc76e18fd8 ] ||
    return 1
  for key in \
    free_memory_before_load free_memory_after_load \
    free_memory_before_benchmark free_memory_min_during_benchmark \
    free_memory_after_benchmark free_memory_after_release \
    cpu_clock_mhz bus_clock_mhz; do
    numeric "$(metric "$key")" || return 1
  done
  validate_scenario culling_off false &&
    validate_scenario culling_on true
}

printf 'Mode : %s\nProfil PPSSPP : %s\n' "$MODE" \
  "${STATE_ROOT#"$PROJECT_ROOT"/}"
printf '%s\n' \
  "Les timings produits sont diagnostiques et ne représentent pas une PSP."
if [ "$MODE" = plan ]; then
  exit 0
fi

PPSSPP="$(find_ppsspp)"
[ -x "$PPSSPP" ] || die "PPSSPP isolé non exécutable"
[ -f "$EBOOT" ] || die "EBOOT benchmark absent"
[ -f "$PACKAGE" ] || die "link.dpmd absent"
safe_mkdir \
  .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_LINK_DEMO/data
safe_mkdir .test-data/ppsspp/xdg-cache
safe_mkdir .tmp/ppsspp
safe_mkdir logs/link-demo

rm -f -- "$DEMO_MARKER" "$BENCH_MARKER" "$METRICS"
cp -- "$EBOOT" "$EBOOT_DEST"
cp -- "$PACKAGE" "$PACKAGE_DEST"
printf '%s' benchmark >"$MODE_FILE"
printf '%s\n%s\n' \
  target=ppsspp \
  run_label=ppsspp_functional >"$CONFIG_FILE"

RUN_ID="$(timestamp_utc)"
STDOUT_LOG="$PROJECT_ROOT/logs/link-demo/$RUN_ID-benchmark.stdout.log"
STDERR_LOG="$PROJECT_ROOT/logs/link-demo/$RUN_ID-benchmark.stderr.log"
pid=""
cleanup() {
  if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

HOME="$HOME_DIR" \
XDG_CONFIG_HOME="$CONFIG_HOME" \
XDG_CACHE_HOME="$STATE_ROOT/xdg-cache" \
TMPDIR="$PROJECT_ROOT/.tmp/ppsspp" \
  "$PPSSPP" --graphics=software --appendconfig="$PPSSPP_CONFIG" \
  --windowed --escape-exit \
  --pause-menu-exit "$EBOOT_DEST" \
  >"$STDOUT_LOG" 2>"$STDERR_LOG" &
pid=$!
start=$SECONDS
while [ $((SECONDS - start)) -lt "$TIMEOUT_SECONDS" ]; do
  if validate_result; then
    cleanup
    pid=""
    cp -- "$METRICS" \
      "$PROJECT_ROOT/logs/link-demo/$RUN_ID-benchmark.metrics.log"
    printf '%s\n' \
      "LINK_BENCH_PPSSPP_FUNCTIONAL_OK scenarios=2 samples=600"
    exit 0
  fi
  if ! kill -0 "$pid" 2>/dev/null; then
    wait "$pid" 2>/dev/null || true
    pid=""
    break
  fi
  sleep 1
done
cleanup
pid=""
[ ! -f "$METRICS" ] || sed -n '1,240p' "$METRICS" >&2
die "benchmark PPSSPP invalide ou timeout"
