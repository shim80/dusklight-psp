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
        "Usage: run-ppsspp-gu-smoke.sh [--plan|--run] [--timeout secondes]"
      exit 0
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] ||
  die "timeout invalide : $TIMEOUT_SECONDS"

EBOOT_SOURCE="$(assert_project_path "build/psp/gu-smoke/EBOOT.PBP")"
PPSSPP_CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
STATE_ROOT="$(assert_project_path ".test-data/ppsspp")"
HOME_DIR="$(assert_project_path ".test-data/ppsspp/home")"
CONFIG_HOME="$(assert_project_path ".test-data/ppsspp/home/.config")"
GAME_DIR="$(assert_project_path ".test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_GU_SMOKE")"
EBOOT_DEST="$GAME_DIR/EBOOT.PBP"
MARKER="$GAME_DIR/GU.OK"
METRICS="$GAME_DIR/GU.METRICS"
SUCCESS_TOKEN="DUSKLIGHT_PSP_GU_OK"

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
  local expected key available reserved remaining prefix
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
draw_buffer_offset|0
display_buffer_offset|557056
depth_buffer_offset|1114112
command_list_bytes|65536
texture_storage|main_ram
texture_width|32
texture_height|32
frames_rendered|1
synchronization|complete
pixels_valid|true
error_code|0
EOF

  available="$(metric_value vram_available)"
  reserved="$(metric_value vram_reserved)"
  remaining="$(metric_value vram_remaining)"
  [[ "$available" =~ ^[0-9]+$ ]] &&
    [[ "$reserved" =~ ^[0-9]+$ ]] &&
    [[ "$remaining" =~ ^[0-9]+$ ]] || return 1
  [ "$available" -ge "$reserved" ] || return 1
  [ $((available - reserved)) -eq "$remaining" ] || return 1

  for prefix in background triangle checker_a checker_b; do
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
printf 'Marqueur        : PSP/GAME/DUSKLIGHT_GU_SMOKE/GU.OK\n'
printf 'Métriques       : PSP/GAME/DUSKLIGHT_GU_SMOKE/GU.METRICS\n'
printf 'Jeton de succès : %s\n' "$SUCCESS_TOKEN"
printf 'Timeout         : %s secondes\n' "$TIMEOUT_SECONDS"

if [ "$MODE" = plan ]; then
  printf '%s\n' "Plan terminé : PPSSPP n'a pas été exécuté."
  exit 0
fi

[ -x "$PPSSPP" ] ||
  die "binaire PPSSPP absent ou non exécutable : ${PPSSPP#"$PROJECT_ROOT"/}"
[ -f "$EBOOT_SOURCE" ] || die "EBOOT absent; compiler d'abord test/gu-smoke"

safe_mkdir .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_GU_SMOKE
safe_mkdir .test-data/ppsspp/xdg-cache
safe_mkdir .tmp/ppsspp
safe_mkdir logs/ppsspp

for stale in EBOOT.PBP GU.OK GU.METRICS FRAME.PPM; do
  stale_path="$GAME_DIR/$stale"
  if [ -e "$stale_path" ]; then
    [ ! -L "$stale_path" ] ||
      die "fichier symbolique refusé dans le répertoire GU : $stale"
    rm -f -- "$stale_path"
  fi
done
cp -- "$EBOOT_SOURCE" "$EBOOT_DEST"

RUN_ID="$(timestamp_utc)"
STDOUT_LOG="$(assert_project_path "logs/ppsspp/gu-smoke-$RUN_ID.stdout.log")"
STDERR_LOG="$(assert_project_path "logs/ppsspp/gu-smoke-$RUN_ID.stderr.log")"
RESULT_LOG="$(assert_project_path "logs/ppsspp/gu-smoke-$RUN_ID.result.log")"
METRICS_LOG="$(assert_project_path "logs/ppsspp/gu-smoke-$RUN_ID.metrics.log")"
EXPECTED_FILE="$(assert_project_path ".tmp/gu-smoke-expected-$RUN_ID.txt")"
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
detail="Le processus s'est arrêté sans marqueur et métriques valides."
start_seconds=$SECONDS
while [ $((SECONDS - start_seconds)) -lt "$TIMEOUT_SECONDS" ]; do
  if [ -f "$MARKER" ] &&
      [ ! -L "$MARKER" ] &&
      cmp -s -- "$EXPECTED_FILE" "$MARKER" &&
      validate_metrics; then
    result=SUCCES
    detail="Marqueur exact, métriques cohérentes et pixels validés."
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
elif [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
  detail="Timeout de ${TIMEOUT_SECONDS} secondes sans résultat GU valide."
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
