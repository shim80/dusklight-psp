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
      [ "$#" -gt 0 ] || die "--timeout exige une valeur"
      TIMEOUT_SECONDS="$1"
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] ||
  die "timeout invalide"

EBOOT="$(assert_project_path "build/psp/link-demo/EBOOT.PBP")"
PACKAGE="$(assert_project_path "build/assets/link-demo/link.dpmd")"
STATE_ROOT="$(assert_project_path ".test-data/ppsspp")"
HOME_DIR="$STATE_ROOT/home"
CONFIG_HOME="$HOME_DIR/.config"
GAME_DIR="$CONFIG_HOME/ppsspp/PSP/GAME/DUSKLIGHT_LINK_DEMO"
DATA_DIR="$GAME_DIR/data"
EBOOT_DEST="$GAME_DIR/EBOOT.PBP"
PACKAGE_DEST="$DATA_DIR/link.dpmd"
MODE_FILE="$GAME_DIR/LINK_DEMO.MODE"
MARKER="$GAME_DIR/LINK_DEMO.OK"
METRICS="$GAME_DIR/LINK_DEMO.METRICS"
CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
TOKEN="DUSKLIGHT_PSP_LINK_DEMO_OK"

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

validate_common() {
  local key expected
  [ -f "$METRICS" ] && [ ! -L "$METRICS" ] || return 1
  while IFS='|' read -r key expected; do
    [ "$(metric "$key")" = "$expected" ] || return 1
  done <<'EOF'
pose_contract|J3D_PURE_WAITS_FRAME0_NEUTRAL_HANDS_V1
diagnostic_pose|true
gameplay_exact_pose|false
actor_body_callbacks_applied|false
actor_head_callbacks_applied|false
hair_dynamics_applied|false
foot_placement_applied|false
arm_ik_applied|false
item_pose_applied|false
source_inventory_triangle_count|6563
body_triangle_count|2777
head_triangle_count|540
face_triangle_count|514
hand_resource_inventory_triangle_count|2732
hand_variant_shape_count|11
selected_hand_shape_count|2
left_hand_shape_index|4
right_hand_shape_index|10
left_hand_triangle_count|248
right_hand_triangle_count|250
selected_visible_triangle_count|4329
inactive_hand_variant_triangle_count|2234
runtime_triangle_count|4329
triangles_removed_by_decimation|0
triangles_removed_from_selected_shapes|0
inactive_variants_excluded|true
selected_topology_preserved|true
runtime_chunk_count|5
runtime_draw_count|5
static_pose_baked|true
runtime_joint_count|0
runtime_weight_count|0
textures_imported|false
materials_imported|false
uv_imported|false
tev_imported|false
runtime_skinning|false
piece_layout_valid|true
pixel_checks_valid|true
guard_regions_valid|true
synchronization|complete
hardware_validation|pending
diagnostic_only|true
source_type_name|::dMdl_obj_c
resource_binding_external|true
commands_emitted|1
controller_deadzone|24
movement_speed|2.5
model_rotation_speed_deg|90
camera_orbit_speed_deg|75
auto_rotation_speed_deg|15
delta_time_clamp|0.0666667
error_code|0
EOF
  [ "$(metric package_crc_expected)" = \
    "$(metric package_crc_actual)" ] || return 1
}

validate_success() {
  validate_common &&
    [ -f "$MARKER" ] && [ ! -L "$MARKER" ] &&
    [ "$(cat "$MARKER")" = "$TOKEN" ] &&
    [ "$(wc -c <"$MARKER" | tr -d ' ')" = 26 ]
}

validate_interactive() {
  validate_common &&
    [ "$(metric mode)" = interactive ] &&
    [ ! -e "$MARKER" ]
}

printf 'Mode : %s\nProfil PPSSPP : %s\n' "$MODE" \
  "${STATE_ROOT#"$PROJECT_ROOT"/}"
if [ "$MODE" = plan ]; then
  exit 0
fi

PPSSPP="$(find_ppsspp)"
[ -x "$PPSSPP" ] || die "PPSSPP isolé non exécutable"
[ -f "$EBOOT" ] || die "EBOOT Link absent"
[ -f "$PACKAGE" ] || die "link.dpmd absent"
safe_mkdir \
  .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_LINK_DEMO/data
safe_mkdir .test-data/ppsspp/xdg-cache
safe_mkdir .tmp/ppsspp
safe_mkdir logs/link-demo
cp -- "$EBOOT" "$EBOOT_DEST"
cp -- "$PACKAGE" "$PACKAGE_DEST"

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
  local name="$1" validator="$2" start
  rm -f -- "$MARKER" "$METRICS"
  printf '%s' "$name" >"$MODE_FILE"
  HOME="$HOME_DIR" \
  XDG_CONFIG_HOME="$CONFIG_HOME" \
  XDG_CACHE_HOME="$STATE_ROOT/xdg-cache" \
  TMPDIR="$PROJECT_ROOT/.tmp/ppsspp" \
    "$PPSSPP" --graphics=software --appendconfig="$CONFIG" \
    --windowed --escape-exit \
    --pause-menu-exit "$EBOOT_DEST" \
    >"logs/link-demo/$RUN_ID-$name.stdout.log" \
    2>"logs/link-demo/$RUN_ID-$name.stderr.log" &
  pid=$!
  start=$SECONDS
  while [ $((SECONDS - start)) -lt "$TIMEOUT_SECONDS" ]; do
    if "$validator"; then
      cleanup
      pid=""
      cp -- "$METRICS" "logs/link-demo/$RUN_ID-$name.metrics.log"
      printf 'LINK_DEMO_CASE_OK mode=%s\n' "$name"
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
  [ ! -f "$METRICS" ] || sed -n '1,160p' "$METRICS" >&2
  die "échec ou timeout du mode $name"
}

run_case interactive validate_interactive
run_case smoke validate_success
[ "$(metric mode)" = smoke ] || die "mauvais mode smoke"
run_case replay validate_success
[ "$(metric mode)" = replay ] || die "mauvais mode replay"

safe_mkdir artifacts/psp-link-demo/DUSKLIGHT_LINK_DEMO/data
cp -- "$EBOOT" \
  "$PROJECT_ROOT/artifacts/psp-link-demo/DUSKLIGHT_LINK_DEMO/EBOOT.PBP"
cp -- "$PACKAGE" \
  "$PROJECT_ROOT/artifacts/psp-link-demo/DUSKLIGHT_LINK_DEMO/data/link.dpmd"
printf '%s\n' "LINK_DEMO_PPSSPP_OK smoke=true replay=true interactive=true"
