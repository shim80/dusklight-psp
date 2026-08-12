#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

ACTION=plan
MODE=all
TIMEOUT_SECONDS=240
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
case "$MODE" in smoke|replay|long|interactive|all) ;; *)
  die "mode invalide : $MODE"
esac

EBOOT="$(assert_project_path "build/psp/room-transition-demo/EBOOT.PBP")"
COMMON="$(assert_project_path "build/assets/link-playable")"
ROOMS="$(assert_project_path "build/assets/room-transition")"
STATE_ROOT="$(assert_project_path ".test-data/ppsspp")"
HOME_DIR="$STATE_ROOT/home"
CONFIG_HOME="$HOME_DIR/.config"
GAME_DIR="$CONFIG_HOME/ppsspp/PSP/GAME/DUSKLIGHT_ROOM_TRANSITION"
DATA_DIR="$GAME_DIR/data"
MARKER="$GAME_DIR/ROOM_TRANSITION.OK"
METRICS="$GAME_DIR/ROOM_TRANSITION.METRICS"
MODE_FILE="$GAME_DIR/TRANSITION.MODE"
SOFTWARE_CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
ACCEL_CONFIG="$(assert_project_path "test/link-playable/ppsspp-accelerated.ini")"
TOKEN="DUSKLIGHT_PSP_ROOM_TRANSITION_OK"
PPSSPP_INI="$CONFIG_HOME/ppsspp/PSP/SYSTEM/ppsspp.ini"

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
  [ -f "$METRICS" ] || return 1
  local key expected
  while IFS='|' read -r key expected; do
    [ "$(metric "$key")" = "$expected" ] || return 1
  done <<'EOF'
validation_target|PPSSPP
hardware_validation|deferred_by_user
selection_classification|REAL_BIDIRECTIONAL_ROOM_PAIR_SELECTED
pair_bidirectional|true
stage_a|D_MN10
room_a|9
exit_a|1
stage_b|D_MN10
room_b|2
exit_b|2
destination_a_to_b_start|1
destination_b_to_a_start|2
spawn_a_floor_valid|true
spawn_b_floor_valid|true
transition_failures|0
double_room_edram_residency|false
allocations_during_playing|0
bytes_leaked|0
actor_duplicates|0
stale_room_handles|0
stale_actor_handles|0
stale_texture_references|0
package_crc_valid|true
spawn_mapping_valid|true
trigger_mapping_valid|true
collision_valid|true
camera_valid|true
actor_lifecycle_valid|true
resource_generations_valid|true
pixel_checks_valid|true
guard_regions_valid|true
synchronization|complete
non_finite_values|0
diagnostic_only|true
error_code|0
EOF
  [ "$(metric transition_count)" -ge 2 ] &&
    [ "$(metric transition_a_to_b_count)" -ge 1 ] &&
    [ "$(metric transition_b_to_a_count)" -ge 1 ] &&
    [ "$(metric edram_remaining_min)" -ge 96000 ] &&
    [ "$(metric tracked_runtime_memory)" -le 27262976 ] &&
    [ "$(metric transition_working_memory)" -le 1048576 ]
}

validate_automatic() {
  validate_metrics &&
    [ -f "$MARKER" ] &&
    [ "$(cat "$MARKER")" = "$TOKEN" ] &&
    [ "$(wc -c <"$MARKER" | tr -d ' ')" = "${#TOKEN}" ]
}

printf 'Action : %s\nMode : %s\nProfil PPSSPP : %s\n' \
  "$ACTION" "$MODE" "${STATE_ROOT#"$PROJECT_ROOT"/}"
[ "$ACTION" = run ] || exit 0

PPSSPP="$(find_ppsspp)"
[ -x "$PPSSPP" ] || die "PPSSPP isolé non exécutable"
[ -f "$EBOOT" ] || die "EBOOT transition absent"
if [ -f "$PPSSPP_INI" ]; then
  sed -i '' \
    's/^GraphicsBackend = .*/GraphicsBackend = 0 (OPENGL)/' \
    "$PPSSPP_INI"
fi
safe_mkdir .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_ROOM_TRANSITION/data/common
safe_mkdir .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_ROOM_TRANSITION/data/stages/D_MN10/R09
safe_mkdir .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_ROOM_TRANSITION/data/stages/D_MN10/R02
safe_mkdir .test-data/ppsspp/xdg-cache
safe_mkdir .tmp/ppsspp
safe_mkdir logs/room-transition

cp -- "$EBOOT" "$GAME_DIR/EBOOT.PBP"
for package in link.dpsk link.dptx link.dpan hud.dpui; do
  cp -- "$COMMON/$package" "$DATA_DIR/common/$package"
done
for room in R09 R02; do
  for package in room.dprm room.dptx room.dpcl room.dpsc ROOM.MANIFEST; do
    cp -- "$ROOMS/stages/D_MN10/$room/$package" \
      "$DATA_DIR/stages/D_MN10/$room/$package"
  done
done
printf '%s' \
  "seed=0x4455534B route=source_triggers checkpoints=A,B,A acceleration=trigger_entry_only" \
  >"$GAME_DIR/TRANSITION.SCENARIO"

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
  local mode_name="$1" config="$2" backend="$3"
  printf '%s' "$mode_name" >"$MODE_FILE"
  HOME="$HOME_DIR" XDG_CONFIG_HOME="$CONFIG_HOME" \
  XDG_CACHE_HOME="$STATE_ROOT/xdg-cache" \
  TMPDIR="$PROJECT_ROOT/.tmp/ppsspp" \
    "$PPSSPP" --graphics="$backend" --appendconfig="$config" \
    --windowed --escape-exit \
    --pause-menu-exit "$GAME_DIR/EBOOT.PBP" \
    >"$PROJECT_ROOT/logs/room-transition/$RUN_ID-$mode_name.stdout.log" \
    2>"$PROJECT_ROOT/logs/room-transition/$RUN_ID-$mode_name.stderr.log" &
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
  local requested="$1" runtime_mode="$1" config="$SOFTWARE_CONFIG" backend=software
  rm -f -- \
    "$MARKER" "$METRICS" "$GAME_DIR/TRANSITION.EXIT" \
    "$GAME_DIR/TRANSITION.AUTO" "$GAME_DIR/TRANSITION.LONG"
  [ "$requested" != long ] || {
    runtime_mode=replay
    config="$ACCEL_CONFIG"
    backend=opengl3.1
    printf '%s' 40 >"$GAME_DIR/TRANSITION.LONG"
  }
  launch "$runtime_mode" "$config" "$backend"
  if ! wait_for validate_automatic; then
    [ ! -f "$METRICS" ] || sed -n '1,260p' "$METRICS" >&2
    die "échec PPSSPP transition : $requested"
  fi
  cleanup
  pid=""
  cp -- "$METRICS" \
    "$PROJECT_ROOT/logs/room-transition/$RUN_ID-$requested.metrics.log"
  if [ "$requested" = long ]; then
    [ "$(metric transition_count)" -ge 40 ] ||
      die "session longue incomplète"
  fi
  if [ "$requested" = replay ] || [ "$requested" = long ]; then
    [ "$(metric player_rupees_after)" -ge 1 ] ||
      die "collecte rejouable absente"
    [ "$(metric room_resets)" -ge 1 ] ||
      die "reset de salle non exercé"
    [ "$(metric pause_entries)" -ge 1 ] ||
      die "pause rejouable non exercée"
  fi
  printf 'ROOM_TRANSITION_CASE_OK mode=%s transitions=%s\n' \
    "$requested" "$(metric transition_count)"
}

run_interactive() {
  rm -f -- \
    "$MARKER" "$METRICS" "$GAME_DIR/TRANSITION.EXIT" \
    "$GAME_DIR/TRANSITION.AUTO" "$GAME_DIR/TRANSITION.LONG"
  printf '%s' enabled >"$GAME_DIR/TRANSITION.AUTO"
  printf '%s' exit_after_round_trip >"$GAME_DIR/TRANSITION.EXIT"
  launch interactive "$ACCEL_CONFIG" opengl3.1
  if ! wait_for validate_metrics; then
    [ ! -f "$METRICS" ] || sed -n '1,260p' "$METRICS" >&2
    die "interactive transition n'a pas effectué l'aller-retour"
  fi
  cleanup
  pid=""
  [ ! -e "$MARKER" ] ||
    die "interactive ne doit pas produire ROOM_TRANSITION.OK"
  printf '%s\n' \
    "ROOM_TRANSITION_CASE_OK mode=interactive a_to_b=true b_to_a=true clean_exit=true"
}

case "$MODE" in
  smoke|replay|long) run_automatic "$MODE" ;;
  interactive) run_interactive ;;
  all)
    run_automatic smoke
    run_automatic replay
    run_automatic long
    run_interactive
    ;;
esac
