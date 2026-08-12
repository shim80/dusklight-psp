#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=lib/ppsspp-host-backend.sh
. "$SCRIPT_DIR/lib/ppsspp-host-backend.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

MODE=plan
TIMEOUT_SECONDS=30
BACKEND="${DUSKLIGHT_PPSSPP_BACKEND:-opengl}"
while [ "$#" -gt 0 ]; do
  case "$1" in
    --plan) MODE=plan ;;
    --run) MODE=run ;;
    --backend)
      shift
      [ "$#" -gt 0 ] || die "--backend exige auto, opengl ou vulkan"
      BACKEND="$1"
      ;;
    --timeout)
      shift
      [ "$#" -gt 0 ] || die "--timeout exige un nombre de secondes"
      TIMEOUT_SECONDS="$1"
      ;;
    -h|--help)
      printf '%s\n' \
        "Usage: run-ppsspp-core-smoke.sh [--plan|--run] [--backend auto|opengl|vulkan] [--timeout secondes]"
      exit 0
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] ||
  die "timeout invalide : $TIMEOUT_SECONDS"
ppsspp_validate_backend "$BACKEND" ||
  die "backend PPSSPP invalide : $BACKEND"

EBOOT_SOURCE="$(assert_project_path "build/psp/core-smoke/EBOOT.PBP")"
STATE_ROOT="$(assert_project_path ".test-data/ppsspp")"
HOME_DIR="$(assert_project_path ".test-data/ppsspp/home")"
CONFIG_HOME="$(assert_project_path ".test-data/ppsspp/home/.config")"
GAME_DIR="$(assert_project_path ".test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_CORE_SMOKE")"
EBOOT_DEST="$GAME_DIR/EBOOT.PBP"
MARKER="$GAME_DIR/CORE.OK"
SUCCESS_TOKEN="DUSKLIGHT_PSP_CORE_OK"
SOFTWARE_CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"

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

PPSSPP="$(find_ppsspp)"
assert_project_path "$PPSSPP" >/dev/null

printf 'Mode            : %s\n' "$MODE"
printf 'PPSSPP          : %s\n' "${PPSSPP#"$PROJECT_ROOT"/}"
printf 'EBOOT source    : %s\n' "${EBOOT_SOURCE#"$PROJECT_ROOT"/}"
printf 'Profil isolé    : %s\n' "${STATE_ROOT#"$PROJECT_ROOT"/}"
printf 'Marqueur        : PSP/GAME/DUSKLIGHT_CORE_SMOKE/CORE.OK\n'
printf 'Jeton de succès : %s\n' "$SUCCESS_TOKEN"
printf 'Timeout         : %s secondes\n' "$TIMEOUT_SECONDS"
printf 'Backend demandé: %s\n' "$BACKEND"

if [ "$MODE" = plan ]; then
  printf '%s\n' "Plan terminé : PPSSPP n'a pas été exécuté."
  exit 0
fi

[ -x "$PPSSPP" ] ||
  die "binaire PPSSPP absent ou non exécutable : ${PPSSPP#"$PROJECT_ROOT"/}"
[ -f "$EBOOT_SOURCE" ] || die "EBOOT absent; compiler d'abord test/core-smoke"

safe_mkdir .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_CORE_SMOKE
safe_mkdir .test-data/ppsspp/xdg-cache
safe_mkdir .tmp/ppsspp
safe_mkdir logs/ppsspp
safe_mkdir .test-data/ppsspp/config
cp -- "$EBOOT_SOURCE" "$EBOOT_DEST"
if [ -e "$MARKER" ]; then
  [ ! -L "$MARKER" ] || die "marqueur symbolique refusé : $MARKER"
  rm -f -- "$MARKER"
fi

RUN_ID="$(timestamp_utc)"
STDOUT_LOG="$(assert_project_path "logs/ppsspp/core-smoke-$RUN_ID.stdout.log")"
STDERR_LOG="$(assert_project_path "logs/ppsspp/core-smoke-$RUN_ID.stderr.log")"
RESULT_LOG="$(assert_project_path "logs/ppsspp/core-smoke-$RUN_ID.result.log")"
PREFLIGHT_LOG="$(assert_project_path "logs/ppsspp/core-smoke-$RUN_ID.preflight.log")"
SELECTED_BACKEND="$(ppsspp_resolve_backend "$BACKEND" smoke)"
RUN_CONFIG="$(assert_project_path \
  ".test-data/ppsspp/config/core-smoke-$SELECTED_BACKEND-software.ini")"
ppsspp_write_backend_config \
  "$SOFTWARE_CONFIG" "$SELECTED_BACKEND" "$RUN_CONFIG" ||
  die "impossible de créer la configuration PPSSPP isolée"
GRAPHICS_ARGUMENT="$(
  ppsspp_graphics_argument "$SELECTED_BACKEND" true)"
ppsspp_write_preflight \
  "$PREFLIGHT_LOG" "$BACKEND" "$SELECTED_BACKEND" "$PPSSPP"
EXPECTED_FILE="$(assert_project_path ".tmp/core-smoke-expected-$RUN_ID.txt")"
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
  "$PPSSPP" --graphics="$GRAPHICS_ARGUMENT" \
  --appendconfig="$RUN_CONFIG" \
  --windowed --escape-exit --pause-menu-exit "$EBOOT_DEST" \
  >"$STDOUT_LOG" 2>"$STDERR_LOG" &
pid=$!

result=ECHEC
detail="Le processus s'est arrêté sans marqueur exact."
timed_out=false
start_seconds=$SECONDS
while [ $((SECONDS - start_seconds)) -lt "$TIMEOUT_SECONDS" ]; do
  if [ -f "$MARKER" ] && cmp -s -- "$EXPECTED_FILE" "$MARKER"; then
    result=SUCCES
    detail="Le marqueur exact a été écrit dans le Memory Stick isolé."
    break
  fi
  if ! kill -0 "$pid" 2>/dev/null; then
    wait "$pid" 2>/dev/null || true
    pid=""
    break
  fi
  sleep 1
done

if [ "$result" != SUCCES ] && [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
  detail="Timeout de ${TIMEOUT_SECONDS} secondes sans marqueur exact."
  timed_out=true
fi
cleanup
pid=""
trap - EXIT INT TERM
if [ "$result" = SUCCES ]; then
  classification=PSP_EBOOT_STARTED
else
  classification="$(
    ppsspp_failure_classification \
      "$STDERR_LOG" "$EBOOT_DEST" "$timed_out")"
fi

{
  printf 'date_utc=%s\n' "$RUN_ID"
  printf 'result=%s\n' "$result"
  printf 'eboot=%s\n' "${EBOOT_SOURCE#"$PROJECT_ROOT"/}"
  printf 'profile=%s\n' "${STATE_ROOT#"$PROJECT_ROOT"/}"
  printf 'marker=%s\n' "${MARKER#"$PROJECT_ROOT"/}"
  printf 'stdout=%s\n' "${STDOUT_LOG#"$PROJECT_ROOT"/}"
  printf 'stderr=%s\n' "${STDERR_LOG#"$PROJECT_ROOT"/}"
  printf 'preflight=%s\n' "${PREFLIGHT_LOG#"$PROJECT_ROOT"/}"
  printf 'backend_requested=%s\n' "$BACKEND"
  printf 'backend_used=%s\n' "$SELECTED_BACKEND"
  printf 'graphics_argument=%s\n' "$GRAPHICS_ARGUMENT"
  printf 'classification=%s\n' "$classification"
  if ppsspp_boot_observed "$STDERR_LOG" "$EBOOT_DEST"; then
    printf 'psp_eboot_started=true\n'
  else
    printf 'psp_eboot_started=false\n'
  fi
  printf 'detail=%s\n' "$detail"
} > "$RESULT_LOG"

printf 'Résultat : %s\n' "$result"
printf 'Stdout   : %s\n' "${STDOUT_LOG#"$PROJECT_ROOT"/}"
printf 'Stderr   : %s\n' "${STDERR_LOG#"$PROJECT_ROOT"/}"
printf 'Journal  : %s\n' "${RESULT_LOG#"$PROJECT_ROOT"/}"
printf 'Classement: %s\n' "$classification"
[ "$result" = SUCCES ] || exit 1
