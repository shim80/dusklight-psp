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
    --mode)
      shift
      [ "$#" -gt 0 ] || die "--mode exige une valeur"
      MODE="$1"
      ;;
    --timeout)
      shift
      [ "$#" -gt 0 ] || die "--timeout exige une valeur"
      TIMEOUT_SECONDS="$1"
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
case "$MODE" in smoke|replay|interactive|negative|all) ;; *)
  die "mode invalide : $MODE"
esac
[[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] ||
  die "timeout invalide"

EBOOT="$(assert_project_path "build/psp/link-playable-demo/EBOOT.PBP")"
ASSETS="$(assert_project_path "build/assets/link-playable")"
STATE_ROOT="$(assert_project_path ".test-data/ppsspp")"
HOME_DIR="$STATE_ROOT/home"
CONFIG_HOME="$HOME_DIR/.config"
GAME_DIR="$CONFIG_HOME/ppsspp/PSP/GAME/DUSKLIGHT_LINK_PLAYABLE"
DATA_DIR="$GAME_DIR/data"
MARKER="$GAME_DIR/PLAYABLE.OK"
METRICS="$GAME_DIR/PLAYABLE.METRICS"
MODE_FILE="$GAME_DIR/PLAYABLE.MODE"
EXIT_FILE="$GAME_DIR/PLAYABLE.EXIT"
SOFTWARE_CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
ACCEL_CONFIG="$(assert_project_path "test/link-playable/ppsspp-accelerated.ini")"
TOKEN="DUSKLIGHT_PSP_PLAYABLE_DEMO_OK"

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
source_disc_id|GZ2P01
source_disc_revision|0
source_type_name|::dMdl_obj_c
model_triangle_count|4329
joint_count|35
max_weights_per_vertex|5
animation_count|3
run_animation_fallback|false
animation_sample_rate|30
animation_blend_frames|6
runtime_skinning|true
skinning_path|CPU_REFERENCE
allocations_during_update|0
allocations_during_render|0
texture_count|29
texture_formats|GU_PSM_4444_SWIZZLED
textures_downscaled|0
materials_runtime|27
framebuffer_format|GU_PSM_5650
hearts_max|3
package_crc_valid|true
animation_matrices_finite|true
skinned_vertices_finite|true
guard_regions_valid|true
pixel_checks_valid|true
synchronization|complete
diagnostic_only|true
compatible_psp_1000_budget|true
hardware_performance_claim|false
error_code|0
EOF
  [ "$(metric texture_runtime_bytes)" -gt 0 ] &&
    [ "$(metric link_draw_calls)" -gt 0 ] &&
    [ "$(metric ui_draw_calls)" -gt 0 ] &&
    [ "$(metric edram_remaining)" -gt 80000 ]
}

validate_automatic() {
  validate_metrics &&
    [ -f "$MARKER" ] && [ ! -L "$MARKER" ] &&
    [ "$(cat "$MARKER")" = "$TOKEN" ] &&
    [ "$(wc -c <"$MARKER" | tr -d ' ')" = 30 ]
}

validate_clean_exit() {
  validate_metrics &&
    [ "$(metric mode)" = interactive ] &&
    [ "$(metric pause_state)" = exiting ]
}

validate_negative() {
  [ -f "$METRICS" ] && [ ! -L "$METRICS" ] &&
    [ "$(metric mode)" = invalid ] &&
    [ "$(metric error_code)" = 100 ] &&
    [ ! -e "$MARKER" ]
}

printf 'Action : %s\nMode : %s\nProfil PPSSPP : %s\n' \
  "$ACTION" "$MODE" "${STATE_ROOT#"$PROJECT_ROOT"/}"
if [ "$ACTION" = plan ]; then
  exit 0
fi

[ -f "$EBOOT" ] || die "EBOOT jouable absent"
for package in link.dpsk link.dptx link.dpan hud.dpui PLAYABLE.MANIFEST; do
  [ -f "$ASSETS/$package" ] || die "package absent : $package"
done
safe_mkdir logs/link-playable
RUN_ID="$(timestamp_utc)"
run_case() {
  local name="$1" config="$2" renderer="$3"
  local request_id="$RUN_ID-link-playable-$name"
  local control="$PROJECT_ROOT/.tmp/link-playable-gui/$request_id"
  local request_mode="$name"
  local -a args
  [ "$name" != negative ] || request_mode=smoke
  safe_mkdir ".tmp/link-playable-gui/$request_id"
  printf '%s' "$([ "$name" = negative ] && echo unknown || echo "$name")" \
    >"$control/PLAYABLE.MODE"
  args=(
    --run
    --request-id "$request_id"
    --eboot "$EBOOT"
    --game-id DUSKLIGHT_LINK_PLAYABLE
    --config "$config"
    --mode "$request_mode"
    --presentation game
    --backend opengl
    --renderer "$renderer"
    --timeout "$TIMEOUT_SECONDS"
    --package "$ASSETS=data"
    --package "$control/PLAYABLE.MODE=PLAYABLE.MODE"
  )
  if [ "$name" = interactive ]; then
    printf '%s' exit >"$control/PLAYABLE.EXIT"
    args+=(--package "$control/PLAYABLE.EXIT=PLAYABLE.EXIT")
  elif [ "$name" != negative ]; then
    args+=(--marker "PLAYABLE.OK=$TOKEN")
  fi
  "$SCRIPT_DIR/ppsspp-gui-runner-request.sh" "${args[@]}"
  GAME_DIR="$PROJECT_ROOT/.test-data/ppsspp/gui-runner/sessions/$request_id/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_LINK_PLAYABLE"
  DATA_DIR="$GAME_DIR/data"
  MARKER="$GAME_DIR/PLAYABLE.OK"
  METRICS="$GAME_DIR/PLAYABLE.METRICS"
  MODE_FILE="$GAME_DIR/PLAYABLE.MODE"
  EXIT_FILE="$GAME_DIR/PLAYABLE.EXIT"
  case "$name" in
    smoke|replay)
      validate_automatic || die "échec PPSSPP GUI : $name"
      printf 'LINK_PLAYABLE_CASE_OK mode=%s transport=persistent_gui_broker\n' "$name"
      ;;
    interactive)
      validate_clean_exit || die "sortie interactive GUI invalide"
      [ ! -e "$MARKER" ] || die "marqueur automatique interactif inattendu"
      printf '%s\n' \
        "LINK_PLAYABLE_CASE_OK mode=interactive clean_exit=true transport=persistent_gui_broker"
      ;;
    negative)
      validate_negative || die "mode inconnu GUI non refusé"
      printf '%s\n' \
        "LINK_PLAYABLE_NEGATIVE_OK mode=unknown marker=false transport=persistent_gui_broker"
      ;;
  esac
  cp -- "$METRICS" \
    "$PROJECT_ROOT/logs/link-playable/$RUN_ID-$name.metrics.log"
}

case "$MODE" in
  smoke) run_case smoke "$SOFTWARE_CONFIG" software ;;
  replay) run_case replay "$SOFTWARE_CONFIG" software ;;
  interactive) run_case interactive "$ACCEL_CONFIG" hardware ;;
  negative) run_case negative "$SOFTWARE_CONFIG" software ;;
  all)
    run_case negative "$SOFTWARE_CONFIG" software
    run_case smoke "$SOFTWARE_CONFIG" software
    run_case replay "$SOFTWARE_CONFIG" software
    run_case interactive "$ACCEL_CONFIG" hardware
    ;;
esac

printf '%s\n' "LINK_PLAYABLE_PPSSPP_OK mode=$MODE"
