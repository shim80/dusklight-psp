#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

ACTION=plan
MODE=all
TIMEOUT_SECONDS=180
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

EBOOT="$(assert_project_path "build/psp/real-actor-demo/EBOOT.PBP")"
LINK="$(assert_project_path "build/assets/link-playable")"
ROOM="$(assert_project_path "build/assets/first-real-room")"
ACTOR="$(assert_project_path "build/assets/first-real-actor")"
STATE_ROOT="$(assert_project_path ".test-data/ppsspp")"
HOME_DIR="$STATE_ROOT/home"
CONFIG_HOME="$HOME_DIR/.config"
GAME_DIR="$CONFIG_HOME/ppsspp/PSP/GAME/DUSKLIGHT_REAL_ACTOR"
DATA_DIR="$GAME_DIR/data"
MARKER="$GAME_DIR/REAL_ACTOR.OK"
METRICS="$GAME_DIR/REAL_ACTOR.METRICS"
MODE_FILE="$GAME_DIR/ACTOR.MODE"
LONG_FILE="$GAME_DIR/ACTOR.LONG"
EXIT_FILE="$GAME_DIR/ACTOR.EXIT"
SOFTWARE_CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
ACCEL_CONFIG="$(assert_project_path "test/link-playable/ppsspp-accelerated.ini")"
TOKEN="DUSKLIGHT_PSP_REAL_ACTOR_OK"

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
source_actor_record_count|3
supported_actor_record_count|2
unsupported_actor_record_count|1
actor_instances_created|2
actor_instances_active|2
geyser_process_id|0x0167
geyser_profile_symbol|g_profile_Obj_Geyser
geyser_class_name|daObjGeyser_c
geyser_instance_count|2
geyser_0_source_index|0
geyser_0_params|0x08040401
geyser_1_source_index|1
geyser_1_params|0x080404FF
source_actor_is_tag|false
switch_system_used|false
visual_original_asset_used|false
visual_fallback_used|true
particle_pool_capacity|96
particle_pool_overflow|0
actor_texture_edram_bytes|0
allocations_during_actor_update|0
allocations_during_actor_render|0
non_finite_actor_values|0
guard_regions_valid|true
pixel_checks_valid|true
region_checks_valid|true
package_crc_valid|true
synchronization|complete
diagnostic_only|true
error_code|0
EOF
  [ "$(metric actor_capacity)" -ge 2 ] &&
    [ "$(metric geyser_0_state_transitions)" -gt 0 ] &&
    [ "$(metric geyser_1_state_transitions)" -gt 0 ] &&
    [ "$(metric actor_draw_calls)" -le 8 ] &&
    [ "$(metric total_draw_calls)" -le 80 ] &&
    [ "$(metric actor_dynamic_buffer_bytes)" -le 131072 ] &&
    [ "$(metric edram_remaining)" -ge 400000 ] &&
    [ "$(metric tracked_runtime_memory)" -le 6291456 ]
}

validate_automatic() {
  validate_metrics &&
    [ "$(metric actor_reset_calls)" -gt 0 ] &&
    [ "$(metric geyser_1_player_contacts)" -gt 0 ] &&
    [ -f "$MARKER" ] && [ ! -L "$MARKER" ] &&
    [ "$(cat "$MARKER")" = "$TOKEN" ] &&
    [ "$(wc -c <"$MARKER" | tr -d ' ')" = "${#TOKEN}" ]
}

validate_invalid() {
  [ -f "$METRICS" ] && [ "$(metric mode)" = invalid ] &&
    [ ! -e "$MARKER" ]
}

EXPECTED_ERROR=0
validate_expected_failure() {
  [ -f "$METRICS" ] && [ "$(metric mode)" = smoke ] &&
    [ "$(metric error_code)" = "$EXPECTED_ERROR" ] &&
    [ ! -e "$MARKER" ]
}

validate_clean_exit() {
  validate_metrics && [ "$(metric mode)" = interactive ]
}

printf 'Action : %s\nMode : %s\nProfil PPSSPP : %s\n' \
  "$ACTION" "$MODE" "${STATE_ROOT#"$PROJECT_ROOT"/}"
[ "$ACTION" = run ] || exit 0

PPSSPP="$(find_ppsspp)"
[ -x "$PPSSPP" ] || die "PPSSPP isolé non exécutable"
[ -f "$EBOOT" ] || die "EBOOT real actor absent"
safe_mkdir .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_REAL_ACTOR/data
safe_mkdir .test-data/ppsspp/xdg-cache
safe_mkdir .tmp/ppsspp
safe_mkdir logs/real-actor
cp -- "$EBOOT" "$GAME_DIR/EBOOT.PBP"
for package in link.dpsk link.dptx link.dpan hud.dpui; do
  cp -- "$LINK/$package" "$DATA_DIR/$package"
done
for package in room.dprm room.dptx room.dpcl; do
  cp -- "$ROOM/$package" "$DATA_DIR/$package"
done
cp -- "$ACTOR/room.dpsc" "$DATA_DIR/room.dpsc"
cp -- "$ACTOR/ACTOR.MANIFEST" "$DATA_DIR/ACTOR.MANIFEST"
printf '%s' "dual_geyser_source_records_v1" >"$GAME_DIR/ACTOR.SCENARIO"

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
    >"$PROJECT_ROOT/logs/real-actor/$RUN_ID-$name.stdout.log" \
    2>"$PROJECT_ROOT/logs/real-actor/$RUN_ID-$name.stderr.log" &
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
  local name="$1" config="$SOFTWARE_CONFIG"
  rm -f -- "$LONG_FILE"
  if [ "$name" = long ]; then
    printf '%s' 7200 >"$LONG_FILE"
    config="$ACCEL_CONFIG"
    if [ "$TIMEOUT_SECONDS" -lt 360 ]; then
      TIMEOUT_SECONDS=360
    fi
  fi
  launch "$([ "$name" = long ] && printf replay || printf '%s' "$name")" "$config"
  if ! wait_for validate_automatic; then
    [ ! -f "$METRICS" ] || sed -n '1,260p' "$METRICS" >&2
    die "échec PPSSPP acteur : $name"
  fi
  cleanup
  pid=""
  cp -- "$METRICS" "$PROJECT_ROOT/logs/real-actor/$RUN_ID-$name.metrics.log"
  rm -f -- "$LONG_FILE"
  printf 'FIRST_REAL_ACTOR_CASE_OK mode=%s\n' "$name"
}

run_interactive() {
  rm -f -- "$LONG_FILE"
  launch interactive "$ACCEL_CONFIG"
  wait_for validate_metrics || die "le mode interactif acteur n'a pas démarré"
  printf '%s' exit >"$EXIT_FILE"
  wait_for validate_clean_exit || die "la sortie interactive acteur a expiré"
  cleanup
  pid=""
  [ ! -e "$MARKER" ] ||
    die "interactive ne doit pas produire REAL_ACTOR.OK"
  printf '%s\n' "FIRST_REAL_ACTOR_CASE_OK mode=interactive clean_exit=true"
}

run_negative() {
  launch unknown "$SOFTWARE_CONFIG"
  wait_for validate_invalid || die "mode acteur inconnu non refusé"
  cleanup
  pid=""
  [ ! -e "$MARKER" ] || die "mode invalide a produit REAL_ACTOR.OK"
  printf '%s\n' "FIRST_REAL_ACTOR_NEGATIVE_OK mode=unknown marker=false"
}

restore_scene() {
  cp -- "$ACTOR/room.dpsc" "$DATA_DIR/room.dpsc"
}

run_scene_negative() {
  local mutation="$1" expected="$2" label="$3"
  restore_scene
  if [ "$mutation" = absent ]; then
    rm -f -- "$DATA_DIR/room.dpsc"
  else
    printf '\0' >>"$DATA_DIR/room.dpsc"
  fi
  EXPECTED_ERROR="$expected"
  launch smoke "$SOFTWARE_CONFIG"
  wait_for validate_expected_failure ||
    die "cas DPSC acteur non refusé : $label"
  cleanup
  pid=""
  [ ! -e "$MARKER" ] ||
    die "un DPSC invalide a produit REAL_ACTOR.OK : $label"
  printf 'FIRST_REAL_ACTOR_NEGATIVE_OK case=%s marker=false\n' "$label"
}

run_matrix() {
  run_scene_negative absent 101 dpsc_absent
  run_scene_negative corrupt 120 dpsc_crc
  restore_scene
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
printf '%s\n' "FIRST_REAL_ACTOR_PPSSPP_OK mode=$MODE"
