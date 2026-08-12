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
      printf '%s\n' "Usage: run-ppsspp-smoke.sh [--plan|--run] [--timeout secondes]"
      exit 0
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] || die "timeout invalide : $TIMEOUT_SECONDS"

EBOOT_SOURCE="$(assert_project_path "build/psp/smoke/EBOOT.PBP")"
STATE_ROOT="$(assert_project_path ".test-data/ppsspp")"
HOME_DIR="$(assert_project_path ".test-data/ppsspp/home")"
CONFIG_HOME="$(assert_project_path ".test-data/ppsspp/home/.config")"
MEMSTICK_ROOT="$(assert_project_path ".test-data/ppsspp/home/.config/ppsspp")"
GAME_DIR="$(assert_project_path ".test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_SMOKE")"
EBOOT_DEST="$GAME_DIR/EBOOT.PBP"
MARKER="$GAME_DIR/SMOKE.OK"

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
    if [ -f "$candidate" ]; then printf '%s\n' "$candidate"; return; fi
  done
  printf '%s\n' "$PROJECT_ROOT/.tools/ppsspp/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL"
}

PPSSPP="$(find_ppsspp)"
assert_project_path "$PPSSPP" >/dev/null

printf 'Mode            : %s\n' "$MODE"
printf 'PPSSPP          : %s\n' "${PPSSPP#"$PROJECT_ROOT"/}"
printf 'EBOOT source    : %s\n' "${EBOOT_SOURCE#"$PROJECT_ROOT"/}"
printf 'Profil isolé    : %s\n' "${STATE_ROOT#"$PROJECT_ROOT"/}"
printf 'Jeton de succès : DUSKLIGHT_PSP_SMOKE_OK\n'
printf 'Timeout         : %s secondes\n' "$TIMEOUT_SECONDS"

if [ "$MODE" = plan ]; then
  printf '%s\n' "Plan terminé : PPSSPP n'a pas été exécuté."
  exit 0
fi

[ -x "$PPSSPP" ] || die "binaire PPSSPP absent ou non exécutable : ${PPSSPP#"$PROJECT_ROOT"/}"
[ -f "$EBOOT_SOURCE" ] || die "EBOOT absent; compiler d'abord test/smoke"

safe_mkdir .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_SMOKE
safe_mkdir .test-data/ppsspp/xdg-cache
safe_mkdir .tmp/ppsspp
safe_mkdir logs/ppsspp
cp -- "$EBOOT_SOURCE" "$EBOOT_DEST"
if [ -e "$MARKER" ]; then
  [ ! -L "$MARKER" ] || die "marqueur symbolique refusé : $MARKER"
  rm -f -- "$MARKER"
fi

RUN_ID="$(timestamp_utc)"
LOG_FILE="$(assert_project_path "logs/ppsspp/smoke-$RUN_ID.log")"
RESULT_REPORT="$(assert_project_path "logs/ppsspp/smoke-$RUN_ID.result.md")"
pid=""
cleanup_process() {
  if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  fi
}
trap cleanup_process EXIT INT TERM

HOME="$HOME_DIR" \
XDG_CONFIG_HOME="$CONFIG_HOME" \
XDG_CACHE_HOME="$STATE_ROOT/xdg-cache" \
TMPDIR="$PROJECT_ROOT/.tmp/ppsspp" \
  "$PPSSPP" --windowed --escape-exit --pause-menu-exit "$EBOOT_DEST" >"$LOG_FILE" 2>&1 &
pid=$!

result=ECHEC
detail="Le processus s'est arrêté sans marqueur."
start_seconds=$SECONDS
while [ $((SECONDS - start_seconds)) -lt "$TIMEOUT_SECONDS" ]; do
  if [ -f "$MARKER" ] && grep -Fqx "DUSKLIGHT_PSP_SMOKE_OK" "$MARKER"; then
    result=SUCCES
    detail="Le marqueur déterministe a été écrit dans le Memory Stick isolé."
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
  detail="Timeout de ${TIMEOUT_SECONDS} secondes sans marqueur."
fi
cleanup_process
pid=""
trap - EXIT INT TERM

safe_mkdir .tmp
REPORT_TEMP="$(assert_project_path ".tmp/ppsspp-smoke-report-$RUN_ID.tmp")"
{
  printf '# Résultat du smoke test PPSSPP\n\n'
  printf -- '- Date UTC : `%s`\n' "$RUN_ID"
  printf -- '- Résultat : **%s**\n' "$result"
  printf -- '- PPSSPP : `%s`\n' "${PPSSPP#"$PROJECT_ROOT"/}"
  printf -- '- EBOOT : `%s`\n' "${EBOOT_SOURCE#"$PROJECT_ROOT"/}"
  printf -- '- Profil : `%s`\n' "${STATE_ROOT#"$PROJECT_ROOT"/}"
  printf -- '- Journal : `%s`\n\n' "${LOG_FILE#"$PROJECT_ROOT"/}"
  printf '%s\n' "$detail"
  printf '\nCe résultat est fonctionnel dans PPSSPP et ne valide ni le timing, ni les caches, ni le Media Engine sur matériel réel.\n'
} > "$REPORT_TEMP"
mv -- "$REPORT_TEMP" "$RESULT_REPORT"

printf 'Résultat : %s\n' "$result"
printf 'Journal : %s\n' "${LOG_FILE#"$PROJECT_ROOT"/}"
printf 'Rapport  : %s\n' "${RESULT_REPORT#"$PROJECT_ROOT"/}"
[ "$result" = SUCCES ] || exit 1
