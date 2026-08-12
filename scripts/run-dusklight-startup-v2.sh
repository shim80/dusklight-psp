#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

ACTION=plan
PROFILE=functional
SCENE=all
BACKEND=opengl
TIMEOUT=300
while [ "$#" -gt 0 ]; do
  case "$1" in
    --plan) ACTION=plan ;;
    --run) ACTION=run ;;
    --profile) shift; PROFILE="${1:-}" ;;
    --scene) shift; SCENE="${1:-}" ;;
    --backend) shift; BACKEND="${1:-}" ;;
    --timeout) shift; TIMEOUT="${1:-}" ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
case "$PROFILE" in functional|performance|psp_conservative) ;; *)
  die "profil invalide : $PROFILE"
esac
case "$SCENE" in boot_logos|title_flow|all) ;; *)
  die "scène invalide : $SCENE"
esac
case "$BACKEND" in opengl|vulkan) ;; *)
  die "backend invalide : $BACKEND"
esac
[[ "$TIMEOUT" =~ ^[1-9][0-9]*$ ]] && [ "$TIMEOUT" -le 1800 ] ||
  die "timeout invalide"

EBOOT="$(assert_project_path "build/psp/dusklight/EBOOT.PBP")"
DATA="$(assert_project_path "build/assets/dusklight-psp/data")"
OUTPUT="$(assert_project_path "artifacts/dusklight-startup-v2/$PROFILE")"
PROFILE_ROOT="$(assert_project_path ".tmp/dusklight-startup-v2/$PROFILE")"
PROFILE_FILE="$PROFILE_ROOT/DUSKLIGHT.BENCHMARK_PROFILE"
[ -f "$EBOOT" ] || die "EBOOT canonique absent"
[ -f "$DATA/RESOURCE.MANIFEST" ] || die "packages canoniques absents"

case "$PROFILE" in
  functional)
    CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
    RENDERER=software
    BACKEND=opengl
    ;;
  performance)
    CONFIG="$(assert_project_path "test/link-playable/ppsspp-accelerated.ini")"
    RENDERER=hardware
    ;;
  psp_conservative)
    CONFIG="$(assert_project_path "test/dusklight-psp/ppsspp-conservative.ini")"
    RENDERER=hardware
    ;;
esac
if [ "$SCENE" = all ]; then
  SCENES=(boot_logos title_flow)
else
  SCENES=("$SCENE")
fi
printf 'STARTUP_V2_PLAN profile=%s scenes=%s renderer=%s backend=%s\n' \
  "$PROFILE" "${SCENES[*]}" "$RENDERER" "$BACKEND"
[ "$ACTION" = run ] || exit 0

safe_mkdir "artifacts/dusklight-startup-v2/$PROFILE"
safe_mkdir ".tmp/dusklight-startup-v2/$PROFILE"
printf '%s' "$PROFILE" >"$PROFILE_FILE"
completed=0
pending=0
for current in "${SCENES[@]}"; do
  if [ "$current" = boot_logos ]; then
    mode=benchmark_boot_logos_v2
  else
    mode=benchmark_title_v2
  fi
  request_id="$(timestamp_utc)-startup-v2-$PROFILE-$current"
  response="$(assert_project_path \
    ".test-data/ppsspp/gui-runner/requests/$request_id/response.json")"
  args=(
    --request-id "$request_id"
    --eboot "$EBOOT"
    --game-id DUSKLIGHT_PSP
    --config "$CONFIG"
    --mode "$mode"
    --presentation game
    --backend "$BACKEND"
    --renderer "$RENDERER"
    --timeout "$TIMEOUT"
    --marker \
      "STARTUP.PARTIAL=DUSKLIGHT_PSP_ORIGINAL_STARTUP_PARTIAL"
    --package "$DATA=data"
    --package "$PROFILE_FILE=DUSKLIGHT.BENCHMARK_PROFILE"
  )
  if [ "$current" = title_flow ]; then
    args+=(
      --marker \
        "NEW_GAME_TRANSITION.OK=DUSKLIGHT_PSP_F_SP108_FIRST_PLAYABLE_OK"
    )
  fi
  action=--run
  if ! "$SCRIPT_DIR/status-ppsspp-gui-broker.sh" >/dev/null 2>&1; then
    action=--queue
  fi
  if ! "$SCRIPT_DIR/ppsspp-gui-runner-request.sh" \
      "$action" "${args[@]}"; then
    classification=PENDING_GUI_EXECUTION
    boot=false
    if [ -f "$response" ]; then
      read -r classification boot < <(
        /usr/bin/python3 - "$response" <<'PY'
import json
import pathlib
import sys
result = json.loads(pathlib.Path(sys.argv[1]).read_text())
print(result["classification"], str(result["boot_observed"]).lower())
PY
      )
    fi
    if [ "$boot" = true ]; then
      die "échec EBOOT après boot PSP : $PROFILE/$current classification=$classification"
    fi
    printf 'STARTUP_V2_PENDING profile=%s scene=%s classification=PENDING_GUI_EXECUTION host_classification=%s\n' \
      "$PROFILE" "$current" "$classification"
    pending=$((pending + 1))
    continue
  fi
  game_dir="$(assert_project_path \
    ".test-data/ppsspp/gui-runner/sessions/$request_id/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PSP")"
  [ -s "$game_dir/STARTUP_BENCHMARK.METRICS" ] ||
    die "métriques startup v2 absentes"
  grep -q '^startup_source_identified=true$' \
    "$game_dir/STARTUP_BENCHMARK.METRICS" ||
    die "métriques startup v2 invalides"
  grep -q "^benchmark_profile=$PROFILE$" \
    "$game_dir/STARTUP_BENCHMARK.METRICS" ||
    die "profil startup v2 invalide"
  cp -- "$game_dir/STARTUP_BENCHMARK.METRICS" \
    "$OUTPUT/$current.metrics"
  "$SCRIPT_DIR/append-startup-profile-evidence.sh" \
    "$PROFILE" "$CONFIG" "$BACKEND" "$RENDERER" \
    "$response" "$OUTPUT/$current.metrics"
  if [ "$current" = title_flow ]; then
    [ -s "$game_dir/NEW_GAME_TRANSITION.METRICS" ] ||
      die "métriques New Game absentes"
    grep -q '^source_stage=F_SP108$' \
      "$game_dir/NEW_GAME_TRANSITION.METRICS" ||
      die "frontière F_SP108 invalide"
    cp -- "$game_dir/NEW_GAME_TRANSITION.METRICS" \
      "$OUTPUT/new_game_transition.metrics"
  fi
  completed=$((completed + 1))
done

if [ "$SCENE" = all ] && [ "$pending" -eq 0 ]; then
  {
    printf 'format=DUSKLIGHT_PSP_STARTUP_V2_RUN_V1\n'
    printf 'profile=%s\n' "$PROFILE"
    printf 'scene_count=%s\n' "${#SCENES[@]}"
    printf 'eboot_sha256=%s\n' "$(sha256_file "$EBOOT")"
    printf 'backend=%s\nrenderer=%s\n' "$BACKEND" "$RENDERER"
    printf 'transport=persistent_gui_broker\n'
    printf 'network_used=false\n'
  } >"$OUTPUT/RUN.MANIFEST"
fi
printf 'DUSKLIGHT_STARTUP_V2_RESULT profile=%s completed=%s pending=%s transport=persistent_gui_broker\n' \
  "$PROFILE" "$completed" "$pending"
