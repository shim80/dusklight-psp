#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

MODE=plan
TIMEOUT_SECONDS=30
while [ "$#" -gt 0 ]; do
  case "$1" in
    --plan) MODE=plan ;;
    --run) MODE=run ;;
    --timeout)
      shift
      [ "$#" -gt 0 ] || die "--timeout exige un nombre de secondes"
      TIMEOUT_SECONDS="$1"
      ;;
    -h|--help)
      printf '%s\n' \
        "Usage: run-ppsspp-3d-smoke.sh [--plan|--run] [--timeout secondes]"
      exit 0
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] ||
  die "timeout invalide : $TIMEOUT_SECONDS"

EBOOT_SOURCE="$(assert_project_path "build/psp/3d-smoke/EBOOT.PBP")"
PPSSPP_CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
STATE_ROOT="$(assert_project_path ".test-data/ppsspp")"
HOME_DIR="$(assert_project_path ".test-data/ppsspp/home")"
CONFIG_HOME="$(assert_project_path ".test-data/ppsspp/home/.config")"
GAME_DIR="$(assert_project_path ".test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_3D_SMOKE")"
EBOOT_DEST="$GAME_DIR/EBOOT.PBP"
MARKER="$GAME_DIR/3D.OK"
METRICS="$GAME_DIR/3D.METRICS"
SUCCESS_TOKEN="DUSKLIGHT_PSP_3D_OK"

find_ppsspp() {
  local candidate
  if [ -n "${PPSSPP_BIN:-}" ]; then
    candidate="$(assert_project_path "$PPSSPP_BIN")"
    [ -f "$candidate" ] || die "PPSSPP_BIN n'est pas un fichier : $candidate"
    printf '%s\n' "$candidate"
    return
  fi
  for candidate in \
    "$PROJECT_ROOT/.tools/ppsspp/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL" \
    "$PROJECT_ROOT/.tools/ppsspp/PPSSPP.app/Contents/MacOS/PPSSPP" \
    "$PROJECT_ROOT/.tools/ppsspp/PPSSPP.AppImage"; do
    if [ -f "$candidate" ]; then
      printf '%s\n' "$candidate"
      return
    fi
  done
  printf '%s\n' \
    "$PROJECT_ROOT/.tools/ppsspp/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL"
}

metric_value() {
  local key="$1"
  awk -F= -v key="$key" '$1 == key {print substr($0, length(key) + 2); exit}' \
    "$METRICS"
}

validate_metrics() {
  local expected key available reserved remaining used capacity prefix
  [ -f "$METRICS" ] && [ ! -L "$METRICS" ] || return 1
  while IFS='|' read -r key expected; do
    [ "$(metric_value "$key")" = "$expected" ] || return 1
  done <<'EOF'
visible_width|480
visible_height|272
stride|512
framebuffer_format|GU_PSM_8888
depth_format|16-bit
vram_reserved|1392640
command_list_capacity|65536
vertex_format|GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_INDEX_16BIT|GU_TRANSFORM_3D
vertex_stride|20
vertex_count|24
vertex_bytes|480
control_vertex_count|9
control_vertex_bytes|180
index_format|GU_INDEX_16BIT
index_count|36
index_bytes|72
control_index_count|3
control_index_bytes|6
triangle_count|15
draw_call_count|4
texture_format|GU_PSM_8888
texture_width|64
texture_height|64
texture_storage|main_ram
texture_bytes|16384
depth_test_enabled|true
depth_function|GU_GEQUAL
depth_write_enabled|true
culling_enabled|true
front_face_winding|GU_CW
cache_writeback_operations|6
cache_invalidate_operations|2
frame_number_validated|1
guard_regions_valid|true
matrices_finite|true
synchronization|complete
diagnostic_only|true
error_code|0
EOF

  available="$(metric_value vram_available)"
  reserved="$(metric_value vram_reserved)"
  remaining="$(metric_value vram_remaining)"
  used="$(metric_value command_list_bytes_used)"
  capacity="$(metric_value command_list_capacity)"
  [[ "$available" =~ ^[0-9]+$ ]] &&
    [[ "$reserved" =~ ^[0-9]+$ ]] &&
    [[ "$remaining" =~ ^[0-9]+$ ]] &&
    [[ "$used" =~ ^[0-9]+$ ]] &&
    [[ "$capacity" =~ ^[0-9]+$ ]] || return 1
  [ "$available" -ge "$reserved" ] || return 1
  [ $((available - reserved)) -eq "$remaining" ] || return 1
  [ "$used" -gt 0 ] && [ "$used" -le "$capacity" ] || return 1

  for prefix in \
    background cube_top_left cube_bottom_right depth culling; do
    [ "$(metric_value "pixel_${prefix}_expected")" = \
      "$(metric_value "pixel_${prefix}_actual")" ] || return 1
  done
}

PPSSPP="$(find_ppsspp)"
assert_project_path "$PPSSPP" >/dev/null

printf 'Mode            : %s\n' "$MODE"
printf 'PPSSPP          : %s\n' "${PPSSPP#"$PROJECT_ROOT"/}"
printf 'EBOOT source    : %s\n' "${EBOOT_SOURCE#"$PROJECT_ROOT"/}"
printf 'Profil isolé    : %s\n' "${STATE_ROOT#"$PROJECT_ROOT"/}"
printf 'Marqueur        : PSP/GAME/DUSKLIGHT_3D_SMOKE/3D.OK\n'
printf 'Métriques       : PSP/GAME/DUSKLIGHT_3D_SMOKE/3D.METRICS\n'
printf 'Jeton de succès : %s\n' "$SUCCESS_TOKEN"
printf 'Timeout         : %s secondes\n' "$TIMEOUT_SECONDS"

if [ "$MODE" = plan ]; then
  printf '%s\n' "Plan terminé : PPSSPP n'a pas été exécuté."
  exit 0
fi

[ -x "$PPSSPP" ] ||
  die "binaire PPSSPP absent ou non exécutable : ${PPSSPP#"$PROJECT_ROOT"/}"
[ -f "$EBOOT_SOURCE" ] || die "EBOOT absent; compiler d'abord test/3d-smoke"

safe_mkdir .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_3D_SMOKE
safe_mkdir .test-data/ppsspp/xdg-cache
safe_mkdir .tmp/ppsspp
safe_mkdir logs/ppsspp

for stale in EBOOT.PBP 3D.OK 3D.METRICS FRAME.PPM; do
  stale_path="$GAME_DIR/$stale"
  if [ -e "$stale_path" ]; then
    [ ! -L "$stale_path" ] ||
      die "fichier symbolique refusé dans le répertoire 3D : $stale"
    rm -f -- "$stale_path"
  fi
done
cp -- "$EBOOT_SOURCE" "$EBOOT_DEST"

RUN_ID="$(timestamp_utc)"
STDOUT_LOG="$(assert_project_path "logs/ppsspp/3d-smoke-$RUN_ID.stdout.log")"
STDERR_LOG="$(assert_project_path "logs/ppsspp/3d-smoke-$RUN_ID.stderr.log")"
RESULT_LOG="$(assert_project_path "logs/ppsspp/3d-smoke-$RUN_ID.result.log")"
METRICS_LOG="$(assert_project_path "logs/ppsspp/3d-smoke-$RUN_ID.metrics.log")"
EXPECTED_FILE="$(assert_project_path ".tmp/3d-smoke-expected-$RUN_ID.txt")"
printf '%s' "$SUCCESS_TOKEN" > "$EXPECTED_FILE"

pid=""
cleanup() {
  if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  fi
  rm -f -- "$EXPECTED_FILE"
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

result=ECHEC
detail="Le processus s'est arrêté sans marqueur et métriques 3D valides."
start_seconds=$SECONDS
while [ $((SECONDS - start_seconds)) -lt "$TIMEOUT_SECONDS" ]; do
  if [ -f "$MARKER" ] &&
      [ ! -L "$MARKER" ] &&
      cmp -s -- "$EXPECTED_FILE" "$MARKER" &&
      validate_metrics; then
    result=SUCCES
    detail="Marqueur exact, métriques, gardes, matrices et pixels validés."
    break
  fi
  if ! kill -0 "$pid" 2>/dev/null; then
    wait "$pid" 2>/dev/null || true
    pid=""
    break
  fi
  sleep 1
done

if [ "$result" = SUCCES ]; then
  cp -- "$METRICS" "$METRICS_LOG"
  sleep 2
elif [ -f "$METRICS" ] && [ ! -L "$METRICS" ]; then
  cp -- "$METRICS" "$METRICS_LOG"
  detail="Métriques 3D produites mais validation ou marqueur en échec."
elif [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
  detail="Timeout de ${TIMEOUT_SECONDS} secondes sans résultat 3D valide."
fi
cleanup
pid=""
trap - EXIT INT TERM

{
  printf 'date_utc=%s\n' "$RUN_ID"
  printf 'result=%s\n' "$result"
  printf 'eboot=%s\n' "${EBOOT_SOURCE#"$PROJECT_ROOT"/}"
  printf 'profile=%s\n' "${STATE_ROOT#"$PROJECT_ROOT"/}"
  printf 'marker=%s\n' "${MARKER#"$PROJECT_ROOT"/}"
  printf 'metrics=%s\n' "${METRICS_LOG#"$PROJECT_ROOT"/}"
  printf 'stdout=%s\n' "${STDOUT_LOG#"$PROJECT_ROOT"/}"
  printf 'stderr=%s\n' "${STDERR_LOG#"$PROJECT_ROOT"/}"
  printf 'detail=%s\n' "$detail"
} > "$RESULT_LOG"

printf 'Résultat : %s\n' "$result"
printf 'Stdout   : %s\n' "${STDOUT_LOG#"$PROJECT_ROOT"/}"
printf 'Stderr   : %s\n' "${STDERR_LOG#"$PROJECT_ROOT"/}"
printf 'Métriques: %s\n' "${METRICS_LOG#"$PROJECT_ROOT"/}"
printf 'Journal  : %s\n' "${RESULT_LOG#"$PROJECT_ROOT"/}"
[ "$result" = SUCCES ] || exit 1
