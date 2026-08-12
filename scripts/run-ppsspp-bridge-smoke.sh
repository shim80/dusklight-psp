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
        "Usage: run-ppsspp-bridge-smoke.sh [--plan|--run] [--timeout secondes]"
      exit 0
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] ||
  die "timeout invalide : $TIMEOUT_SECONDS"

EBOOT_SOURCE="$(assert_project_path "build/psp/bridge-smoke/EBOOT.PBP")"
PACKAGE_SOURCE="$(
  assert_project_path "build/assets/psp-static-mesh/fixture-a.dpsm"
)"
PPSSPP_CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
STATE_ROOT="$(assert_project_path ".test-data/ppsspp")"
HOME_DIR="$(assert_project_path ".test-data/ppsspp/home")"
CONFIG_HOME="$(assert_project_path ".test-data/ppsspp/home/.config")"
GAME_DIR="$CONFIG_HOME/ppsspp/PSP/GAME/DUSKLIGHT_BRIDGE_SMOKE"
DATA_DIR="$GAME_DIR/data"
EBOOT_DEST="$GAME_DIR/EBOOT.PBP"
PACKAGE_DEST="$DATA_DIR/fixture.dpsm"
MODE_FILE="$GAME_DIR/BRIDGE.MODE"
MARKER="$GAME_DIR/BRIDGE.OK"
METRICS="$GAME_DIR/BRIDGE.METRICS"
SUCCESS_TOKEN="DUSKLIGHT_PSP_BRIDGE_OK"

find_ppsspp() {
  local candidate
  if [ -n "${PPSSPP_BIN:-}" ]; then
    candidate="$(assert_project_path "$PPSSPP_BIN")"
    [ -f "$candidate" ] || die "PPSSPP_BIN n'est pas un fichier"
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

metric_is() {
  [ "$(metric_value "$1")" = "$2" ]
}

missing_valid() {
  [ -f "$METRICS" ] && [ ! -L "$METRICS" ] &&
    [ ! -e "$MARKER" ] &&
    metric_is run_mode valid &&
    metric_is package_valid false &&
    metric_is loader_error_code 30 &&
    metric_is loader_error_name IoOpenFailed &&
    metric_is adapter_calls 0 &&
    metric_is commands_emitted 0 &&
    metric_is backend_draw_calls 0
}

matrix_invalid_valid() {
  [ -f "$METRICS" ] && [ ! -L "$METRICS" ] &&
    [ ! -e "$MARKER" ] &&
    metric_is run_mode matrix_invalid &&
    metric_is package_valid true &&
    metric_is bridge_error_code 4 &&
    metric_is bridge_error_name MatrixNotFinite &&
    metric_is adapter_calls 1 &&
    metric_is commands_emitted 0 &&
    metric_is commands_rejected 1 &&
    metric_is backend_commands_received 0 &&
    metric_is backend_draw_calls 0
}

unsupported_valid() {
  [ -f "$METRICS" ] && [ ! -L "$METRICS" ] &&
    [ ! -e "$MARKER" ] &&
    metric_is run_mode unsupported_state &&
    metric_is package_valid true &&
    metric_is bridge_error_code 5 &&
    metric_is bridge_error_name UnsupportedState &&
    metric_is adapter_calls 1 &&
    metric_is commands_emitted 0 &&
    metric_is commands_rejected 1 &&
    metric_is backend_commands_received 0 &&
    metric_is backend_draw_calls 0 &&
    metric_is unsupported_state_count 1
}

positive_valid() {
  local key expected prefix
  [ -f "$MARKER" ] && [ ! -L "$MARKER" ] &&
    [ "$(cat "$MARKER")" = "$SUCCESS_TOKEN" ] &&
    [ "$(wc -c < "$MARKER" | tr -d ' ')" = 23 ] &&
    [ -f "$METRICS" ] && [ ! -L "$METRICS" ] || return 1
  while IFS='|' read -r key expected; do
    metric_is "$key" "$expected" || return 1
  done <<'EOF'
source_type_name|::dMdl_obj_c
source_header|d/d_model_obj.h
source_size|52
source_mtx_offset|0
source_next_offset|48
matrix_conversion_valid|true
lightweight_header_valid|true
aurora_in_header_closure|false
j3d_in_header_closure|false
gx_in_header_closure|false
resource_binding_external|true
package_path|data/fixture.dpsm
package_valid|true
vertex_count|36
index_count|60
texture_width|64
texture_height|64
adapter_calls|1
commands_emitted|1
commands_rejected|0
backend_commands_received|1
backend_draw_calls|1
direct_submit_count|0
unsupported_state_count|0
allocations_during_frame|0
run_mode|valid
transform_valid|true
orientation_valid|true
bridge_path_valid|true
guard_regions_valid|true
synchronization|complete
aurora_required_for_this_smoke|false
aurora_required_for_full_j3d_renderer|true
hardware_validation|pending
loader_error_code|0
loader_error_name|Ok
bridge_error_code|0
bridge_error_name|Ok
error_code|0
EOF
  [ -n "$(metric_value source_object_address)" ] || return 1
  [ -n "$(metric_value source_matrix_values)" ] || return 1
  [ -n "$(metric_value adapted_matrix_values)" ] || return 1
  [ "$(metric_value package_crc_expected)" = \
    "$(metric_value package_crc_actual)" ] || return 1
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
printf 'Paquet externe  : data/fixture.dpsm\n'
printf 'Marqueur        : PSP/GAME/DUSKLIGHT_BRIDGE_SMOKE/BRIDGE.OK\n'
printf 'Jeton de succès : %s\n' "$SUCCESS_TOKEN"
if [ "$MODE" = plan ]; then
  printf '%s\n' "Plan terminé : PPSSPP n'a pas été exécuté."
  exit 0
fi

[ -x "$PPSSPP" ] || die "binaire PPSSPP absent ou non exécutable"
[ -f "$EBOOT_SOURCE" ] || die "EBOOT bridge absent"
[ -f "$PACKAGE_SOURCE" ] || die "paquet DPSM généré absent"
safe_mkdir \
  .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_BRIDGE_SMOKE/data
safe_mkdir .test-data/ppsspp/xdg-cache
safe_mkdir .tmp/ppsspp
safe_mkdir logs/ppsspp

for path in \
  "$EBOOT_DEST" "$PACKAGE_DEST" "$MODE_FILE" "$MARKER" "$METRICS"; do
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
  local name="$1" mode_value="$2" validator="$3"
  local stdout_log stderr_log start
  stdout_log="$(
    assert_project_path "logs/ppsspp/bridge-smoke-$RUN_ID-$name.stdout.log"
  )"
  stderr_log="$(
    assert_project_path "logs/ppsspp/bridge-smoke-$RUN_ID-$name.stderr.log"
  )"
  rm -f -- "$MARKER" "$METRICS"
  printf '%s' "$mode_value" > "$MODE_FILE"
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
        "$(assert_project_path \
          "logs/ppsspp/bridge-smoke-$RUN_ID-$name.metrics.log")"
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
    "$(assert_project_path \
      "logs/ppsspp/bridge-smoke-$RUN_ID-$name.metrics.log")"
  return 1
}

rm -f -- "$PACKAGE_DEST"
run_case missing valid missing_valid ||
  die "le cas sans DPSM n'a pas produit IoOpenFailed"

cp -- "$PACKAGE_SOURCE" "$PACKAGE_DEST"
run_case matrix-invalid matrix_invalid matrix_invalid_valid ||
  die "la matrice non finie n'a pas produit MatrixNotFinite"
run_case unsupported unsupported_state unsupported_valid ||
  die "l'état non pris en charge n'a pas produit UnsupportedState"
run_case valid valid positive_valid ||
  die "le bridge valide n'a pas produit le marqueur, les métriques et pixels attendus"

RESULT_LOG="$(
  assert_project_path "logs/ppsspp/bridge-smoke-$RUN_ID.result.log"
)"
{
  printf 'date_utc=%s\n' "$RUN_ID"
  printf 'result=SUCCES\n'
  printf 'missing_case=IoOpenFailed\n'
  printf 'matrix_case=MatrixNotFinite\n'
  printf 'unsupported_case=UnsupportedState\n'
  printf 'valid_case=%s\n' "$SUCCESS_TOKEN"
  printf 'profile=%s\n' "${STATE_ROOT#"$PROJECT_ROOT"/}"
} > "$RESULT_LOG"
trap - EXIT INT TERM

printf '%s\n' "Sans DPSM       : SUCCES (IoOpenFailed)"
printf '%s\n' "Matrice invalide: SUCCES (MatrixNotFinite)"
printf '%s\n' "État refusé     : SUCCES (UnsupportedState)"
printf '%s\n' "Bridge valide   : SUCCES (marqueur, compteurs et pixels)"
printf 'Journal         : %s\n' "${RESULT_LOG#"$PROJECT_ROOT"/}"
