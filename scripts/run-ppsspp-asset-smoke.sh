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
        "Usage: run-ppsspp-asset-smoke.sh [--plan|--run] [--timeout secondes]"
      exit 0
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] ||
  die "timeout invalide : $TIMEOUT_SECONDS"

EBOOT_SOURCE="$(assert_project_path "build/psp/asset-smoke/EBOOT.PBP")"
PACKAGE_SOURCE="$(
  assert_project_path "build/assets/psp-static-mesh/fixture-a.dpsm"
)"
PPSSPP_CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
STATE_ROOT="$(assert_project_path ".test-data/ppsspp")"
HOME_DIR="$(assert_project_path ".test-data/ppsspp/home")"
CONFIG_HOME="$(assert_project_path ".test-data/ppsspp/home/.config")"
GAME_DIR="$CONFIG_HOME/ppsspp/PSP/GAME/DUSKLIGHT_ASSET_SMOKE"
DATA_DIR="$GAME_DIR/data"
EBOOT_DEST="$GAME_DIR/EBOOT.PBP"
PACKAGE_DEST="$DATA_DIR/fixture.dpsm"
MARKER="$GAME_DIR/ASSET.OK"
METRICS="$GAME_DIR/ASSET.METRICS"
SUCCESS_TOKEN="DUSKLIGHT_PSP_ASSET_OK"

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
  awk -F= -v key="$key" \
    '$1 == key {print substr($0, length(key) + 2); exit}' "$METRICS"
}

negative_valid() {
  [ -f "$METRICS" ] && [ ! -L "$METRICS" ] &&
    [ ! -e "$MARKER" ] &&
    [ "$(metric_value asset_embedded)" = false ] &&
    [ "$(metric_value package_valid)" = false ] &&
    [ "$(metric_value loader_error_code)" = 30 ] &&
    [ "$(metric_value loader_error_name)" = IoOpenFailed ]
}

positive_valid() {
  local key expected prefix
  [ -f "$MARKER" ] && [ ! -L "$MARKER" ] &&
    [ "$(cat "$MARKER")" = "$SUCCESS_TOKEN" ] &&
    [ -f "$METRICS" ] && [ ! -L "$METRICS" ] || return 1
  while IFS='|' read -r key expected; do
    [ "$(metric_value "$key")" = "$expected" ] || return 1
  done <<'EOF'
asset_path|data/fixture.dpsm
asset_embedded|false
package_valid|true
loader_error_code|0
loader_error_name|Ok
package_bytes|17360
vertex_count|36
index_count|60
triangle_count|20
texture_width|64
texture_height|64
texture_bytes|16384
draw_call_count|1
frames_rendered|1
matrices_finite|true
pixel_checks_valid|true
readback_guard_regions_valid|true
synchronization|complete
program_error|0
EOF
  for prefix in background marker shaft_lower tip_upper tip_lower; do
    [ "$(metric_value "pixel_${prefix}_expected")" = \
      "$(metric_value "pixel_${prefix}_actual")" ] || return 1
  done
}

PPSSPP="$(find_ppsspp)"
assert_project_path "$PPSSPP" >/dev/null

printf 'Mode            : %s\n' "$MODE"
printf 'PPSSPP          : %s\n' "${PPSSPP#"$PROJECT_ROOT"/}"
printf 'Profil isolé    : %s\n' "${STATE_ROOT#"$PROJECT_ROOT"/}"
printf 'Asset externe   : data/fixture.dpsm\n'
printf 'Marqueur        : PSP/GAME/DUSKLIGHT_ASSET_SMOKE/ASSET.OK\n'
printf 'Jeton de succès : %s\n' "$SUCCESS_TOKEN"
if [ "$MODE" = plan ]; then
  printf '%s\n' "Plan terminé : PPSSPP n'a pas été exécuté."
  exit 0
fi

[ -x "$PPSSPP" ] || die "binaire PPSSPP absent ou non exécutable"
[ -f "$EBOOT_SOURCE" ] || die "EBOOT asset smoke absent"
[ -f "$PACKAGE_SOURCE" ] || die "paquet DPSM généré absent"
safe_mkdir .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_ASSET_SMOKE/data
safe_mkdir .test-data/ppsspp/xdg-cache
safe_mkdir .tmp/ppsspp
safe_mkdir logs/ppsspp

for path in "$EBOOT_DEST" "$PACKAGE_DEST" "$MARKER" "$METRICS"; do
  if [ -e "$path" ]; then
    [ ! -L "$path" ] || die "lien symbolique refusé : $path"
    rm -f -- "$path"
  fi
done
cp -- "$EBOOT_SOURCE" "$EBOOT_DEST"

RUN_ID="$(timestamp_utc)"
pid=""
cleanup() {
  if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

run_case() {
  local name="$1" validator="$2" stdout_log stderr_log start
  stdout_log="$(assert_project_path "logs/ppsspp/asset-smoke-$RUN_ID-$name.stdout.log")"
  stderr_log="$(assert_project_path "logs/ppsspp/asset-smoke-$RUN_ID-$name.stderr.log")"
  rm -f -- "$MARKER" "$METRICS"
  HOME="$HOME_DIR" \
  XDG_CONFIG_HOME="$CONFIG_HOME" \
  XDG_CACHE_HOME="$STATE_ROOT/xdg-cache" \
  TMPDIR="$PROJECT_ROOT/.tmp/ppsspp" \
    "$PPSSPP" --graphics=software --appendconfig="$PPSSPP_CONFIG" \
    --windowed --escape-exit \
    --pause-menu-exit "$EBOOT_DEST" \
    >"$stdout_log" 2>"$stderr_log" &
  pid=$!
  start=$SECONDS
  while [ $((SECONDS - start)) -lt "$TIMEOUT_SECONDS" ]; do
    if "$validator"; then
      cleanup
      pid=""
      cp -- "$METRICS" \
        "$(assert_project_path "logs/ppsspp/asset-smoke-$RUN_ID-$name.metrics.log")"
      return 0
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
  [ -f "$METRICS" ] && cp -- "$METRICS" \
    "$(assert_project_path "logs/ppsspp/asset-smoke-$RUN_ID-$name.metrics.log")"
  return 1
}

run_case negative negative_valid ||
  die "le lancement sans DPSM n'a pas produit l'échec IoOpenFailed attendu"
cp -- "$PACKAGE_SOURCE" "$PACKAGE_DEST"
run_case positive positive_valid ||
  die "le lancement avec DPSM n'a pas produit le marqueur et les pixels attendus"

RESULT_LOG="$(assert_project_path "logs/ppsspp/asset-smoke-$RUN_ID.result.log")"
{
  printf 'date_utc=%s\n' "$RUN_ID"
  printf 'result=SUCCES\n'
  printf 'negative_case=IoOpenFailed\n'
  printf 'positive_case=%s\n' "$SUCCESS_TOKEN"
  printf 'profile=%s\n' "${STATE_ROOT#"$PROJECT_ROOT"/}"
} > "$RESULT_LOG"
trap - EXIT INT TERM

printf '%s\n' "Résultat négatif : SUCCES (asset externe obligatoire)"
printf '%s\n' "Résultat positif : SUCCES (marqueur et pixels exacts)"
printf 'Journal          : %s\n' "${RESULT_LOG#"$PROJECT_ROOT"/}"
