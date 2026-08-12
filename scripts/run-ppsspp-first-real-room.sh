#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

ACTION=plan
MODE=all
TIMEOUT_SECONDS=120
while [ "$#" -gt 0 ]; do
  case "$1" in
    --plan) ACTION=plan ;;
    --run) ACTION=run ;;
    --mode) shift; [ "$#" -gt 0 ] || die "--mode exige une valeur"; MODE="$1" ;;
    --timeout) shift; [ "$#" -gt 0 ] || die "--timeout exige une valeur"; TIMEOUT_SECONDS="$1" ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
case "$MODE" in smoke|replay|long|interactive|negative|matrix|all) ;; *)
  die "mode invalide : $MODE"
esac
[[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] || die "timeout invalide"

EBOOT="$(assert_project_path "build/psp/real-room-demo/EBOOT.PBP")"
LINK="$(assert_project_path "build/assets/link-playable")"
ROOM="$(assert_project_path "build/assets/first-real-room")"
STATE_ROOT="$(assert_project_path ".test-data/ppsspp")"
HOME_DIR="$STATE_ROOT/home"
CONFIG_HOME="$HOME_DIR/.config"
GAME_DIR="$CONFIG_HOME/ppsspp/PSP/GAME/DUSKLIGHT_REAL_ROOM"
DATA_DIR="$GAME_DIR/data"
MARKER="$GAME_DIR/REAL_ROOM.OK"
METRICS="$GAME_DIR/REAL_ROOM.METRICS"
MODE_FILE="$GAME_DIR/ROOM.MODE"
LONG_FILE="$GAME_DIR/ROOM.LONG"
EXIT_FILE="$GAME_DIR/ROOM.EXIT"
SOFTWARE_CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
ACCEL_CONFIG="$(assert_project_path "test/link-playable/ppsspp-accelerated.ini")"
TOKEN="DUSKLIGHT_PSP_REAL_ROOM_OK"

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

validate_metrics() {
  [ -f "$METRICS" ] && [ ! -L "$METRICS" ] || return 1
  local key expected
  while IFS='|' read -r key expected; do
    [ "$(metric "$key")" = "$expected" ] || return 1
  done <<'EOF'
validation_target|PPSSPP
hardware_validation|deferred_by_user
stage_id|F_SP110
room_index|2
room_layer|0
source_room_triangle_count|1192
runtime_room_triangle_count|1192
room_triangles_removed|0
room_runtime_vertex_count|1726
room_material_count_runtime|6
room_texture_count|7
room_texture_runtime_bytes|311296
room_collision_triangle_count|413
spawn_source|PLYR
spawn_fallback|false
spawn_floor_valid|true
room_transition_supported|false
link_triangle_count|4329
allocations_during_update|0
allocations_during_render|0
package_crc_valid|true
collision_valid|true
spawn_valid|true
animation_matrices_finite|true
skinned_vertices_finite|true
pixel_checks_valid|true
guard_regions_valid|true
synchronization|complete
diagnostic_only|true
error_code|0
EOF
  [ "$(metric room_draw_calls)" -gt 0 ] &&
    [ "$(metric link_draw_calls)" -gt 0 ] &&
    [ "$(metric ui_draw_calls)" -gt 0 ] &&
    [ "$(metric room_collision_triangle_count)" -gt 0 ] &&
    [ "$(metric edram_remaining)" -ge 96000 ] &&
    [ "$(metric tracked_runtime_memory)" -le 27262976 ] &&
    [ "$(metric total_draw_calls)" -le 175 ]
}

validate_automatic() {
  validate_metrics &&
    [ -f "$MARKER" ] && [ ! -L "$MARKER" ] &&
    [ "$(cat "$MARKER")" = "$TOKEN" ] &&
    [ "$(wc -c <"$MARKER" | tr -d ' ')" = "${#TOKEN}" ]
}

validate_negative() {
  [ -f "$METRICS" ] && [ "$(metric mode)" = invalid ] &&
    [ "$(metric error_code)" = 100 ] && [ ! -e "$MARKER" ]
}

EXPECTED_ERROR=0
validate_expected_failure() {
  [ -f "$METRICS" ] && [ "$(metric mode)" = smoke ] &&
    [ "$(metric error_code)" = "$EXPECTED_ERROR" ] &&
    [ ! -e "$MARKER" ]
}

validate_clean_exit() {
  validate_metrics && [ "$(metric mode)" = interactive ] &&
    [ "$(metric pause_state)" = playing ]
}

printf 'Action : %s\nMode : %s\nProfil PPSSPP : %s\n' \
  "$ACTION" "$MODE" "${STATE_ROOT#"$PROJECT_ROOT"/}"
[ "$ACTION" = run ] || exit 0

PPSSPP="$(find_ppsspp)"
[ -x "$PPSSPP" ] || die "PPSSPP isolé non exécutable"
[ -f "$EBOOT" ] || die "EBOOT real room absent"
safe_mkdir .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_REAL_ROOM/data
safe_mkdir .test-data/ppsspp/xdg-cache
safe_mkdir .tmp/ppsspp
safe_mkdir logs/real-room
cp -- "$EBOOT" "$GAME_DIR/EBOOT.PBP"
for package in link.dpsk link.dptx link.dpan hud.dpui; do
  cp -- "$LINK/$package" "$DATA_DIR/$package"
done
for package in room.dprm room.dptx room.dpcl room.dpsc ROOM.MANIFEST; do
  cp -- "$ROOM/$package" "$DATA_DIR/$package"
done

RUN_ID="$(timestamp_utc)"
pid=""
cleanup() {
  if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

launch() {
  local name="$1" config="$2"
  local backend=software
  [ "$config" = "$ACCEL_CONFIG" ] && backend=opengl3.1
  rm -f -- "$MARKER" "$METRICS" "$EXIT_FILE"
  printf '%s' "$name" >"$MODE_FILE"
  HOME="$HOME_DIR" XDG_CONFIG_HOME="$CONFIG_HOME" \
  XDG_CACHE_HOME="$STATE_ROOT/xdg-cache" \
  TMPDIR="$PROJECT_ROOT/.tmp/ppsspp" \
    "$PPSSPP" --graphics="$backend" --appendconfig="$config" \
    --windowed --escape-exit \
    --pause-menu-exit "$GAME_DIR/EBOOT.PBP" \
    >"$PROJECT_ROOT/logs/real-room/$RUN_ID-$name.stdout.log" \
    2>"$PROJECT_ROOT/logs/real-room/$RUN_ID-$name.stderr.log" &
  pid=$!
}

wait_for() {
  local validator="$1" start=$SECONDS
  while [ $((SECONDS - start)) -lt "$TIMEOUT_SECONDS" ]; do
    "$validator" && return 0
    if ! kill -0 "$pid" 2>/dev/null; then
      wait "$pid" 2>/dev/null || true
      pid=""
      return 1
    fi
    sleep 1
  done
  return 1
}

run_automatic() {
  local name="$1"
  rm -f -- "$LONG_FILE"
  [ "$name" != long ] || printf '%s' 3600 >"$LONG_FILE"
  launch "$([ "$name" = long ] && printf replay || printf '%s' "$name")" \
    "$SOFTWARE_CONFIG"
  if ! wait_for validate_automatic; then
    [ ! -f "$METRICS" ] || sed -n '1,220p' "$METRICS" >&2
    die "échec PPSSPP : $name"
  fi
  cleanup
  pid=""
  cp -- "$METRICS" "$PROJECT_ROOT/logs/real-room/$RUN_ID-$name.metrics.log"
  rm -f -- "$LONG_FILE"
  printf 'FIRST_REAL_ROOM_CASE_OK mode=%s\n' "$name"
}

run_interactive() {
  rm -f -- "$LONG_FILE"
  launch interactive "$ACCEL_CONFIG"
  wait_for validate_metrics || die "le mode interactif n'a pas démarré"
  printf '%s' exit >"$EXIT_FILE"
  wait_for validate_clean_exit || die "la sortie interactive propre a expiré"
  cleanup
  pid=""
  [ ! -e "$MARKER" ] ||
    die "interactive ne doit pas produire le marqueur automatique"
  printf '%s\n' "FIRST_REAL_ROOM_CASE_OK mode=interactive clean_exit=true"
}

run_negative() {
  rm -f -- "$LONG_FILE"
  launch unknown "$SOFTWARE_CONFIG"
  wait_for validate_negative || die "le mode inconnu n'a pas été refusé"
  cleanup
  pid=""
  printf '%s\n' "FIRST_REAL_ROOM_NEGATIVE_OK mode=unknown marker=false"
}

restore_packages() {
  local package
  for package in link.dpsk link.dptx link.dpan hud.dpui; do
    cp -- "$LINK/$package" "$DATA_DIR/$package"
  done
  for package in room.dprm room.dptx room.dpcl room.dpsc ROOM.MANIFEST; do
    cp -- "$ROOM/$package" "$DATA_DIR/$package"
  done
}

run_package_negative() {
  local package="$1" mutation="$2" expected="$3" label="$4"
  restore_packages
  if [ "$mutation" = absent ]; then
    rm -f -- "$DATA_DIR/$package"
  else
    printf '\0' >>"$DATA_DIR/$package"
  fi
  EXPECTED_ERROR="$expected"
  launch smoke "$SOFTWARE_CONFIG"
  if ! wait_for validate_expected_failure; then
    [ ! -f "$METRICS" ] || sed -n '1,120p' "$METRICS" >&2
    die "cas négatif PPSSPP non refusé : $label"
  fi
  cleanup
  pid=""
  [ ! -e "$MARKER" ] ||
    die "un cas négatif a produit REAL_ROOM.OK : $label"
  printf 'FIRST_REAL_ROOM_NEGATIVE_OK case=%s marker=false\n' "$label"
}

run_matrix() {
  run_package_negative room.dprm absent 101 dprm_absent
  run_package_negative room.dptx absent 101 dptx_absent
  run_package_negative room.dpcl absent 101 dpcl_absent
  run_package_negative room.dpsc absent 101 dpsc_absent
  run_package_negative room.dprm corrupt 120 dprm_crc
  run_package_negative room.dptx corrupt 120 dptx_crc
  run_package_negative room.dpcl corrupt 120 dpcl_crc
  run_package_negative room.dpsc corrupt 120 dpsc_crc
  restore_packages
  run_negative
}

case "$MODE" in
  smoke) run_automatic smoke ;;
  replay) run_automatic replay ;;
  long) run_automatic long ;;
  interactive) run_interactive ;;
  negative) run_negative ;;
  matrix) run_matrix ;;
  all)
    run_matrix
    run_automatic smoke
    run_automatic replay
    run_automatic long
    run_interactive
    ;;
esac
printf '%s\n' "FIRST_REAL_ROOM_PPSSPP_OK mode=$MODE"
