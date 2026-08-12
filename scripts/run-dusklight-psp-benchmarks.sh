#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"
. "$SCRIPT_DIR/env.sh" >/dev/null

ACTION=plan
PROFILE=performance
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
case "$SCENE" in boot_intro|title|link_idle|room_stress|transition|all) ;; *)
  die "scène invalide : $SCENE"
esac
case "$BACKEND" in auto|opengl|vulkan) ;; *)
  die "backend invalide : $BACKEND"
esac
[[ "$TIMEOUT" =~ ^[1-9][0-9]*$ ]] && [ "$TIMEOUT" -le 1800 ] ||
  die "timeout invalide"

EBOOT="$(assert_project_path "build/psp/dusklight/EBOOT.PBP")"
DATA="$(assert_project_path "build/assets/dusklight-psp/data")"
PROFILE_ROOT="$(assert_project_path ".tmp/dusklight-benchmark/$PROFILE")"
PROFILE_FILE="$PROFILE_ROOT/DUSKLIGHT.BENCHMARK_PROFILE"
OUTPUT="$(assert_project_path "artifacts/dusklight-psp-benchmarks/$PROFILE")"
[ -f "$EBOOT" ] || die "EBOOT canonique absent"
[ -f "$DATA/RESOURCE.MANIFEST" ] || die "packages canoniques absents"
if [ "$ACTION" = run ]; then
  "$SCRIPT_DIR/status-ppsspp-gui-broker.sh" >/dev/null ||
    die "broker PPSSPP GUI absent ou périmé"
fi

case "$PROFILE" in
  functional)
    CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
    RENDERER=software
    PRESENTATION=debug
    BACKEND=opengl
    ;;
  performance)
    CONFIG="$(assert_project_path "test/link-playable/ppsspp-accelerated.ini")"
    RENDERER=hardware
    PRESENTATION=game
    ;;
  psp_conservative)
    CONFIG="$(assert_project_path "test/dusklight-psp/ppsspp-conservative.ini")"
    RENDERER=hardware
    PRESENTATION=game
    ;;
esac

if [ "$SCENE" = all ]; then
  SCENES=(boot_intro title link_idle room_stress transition)
else
  SCENES=("$SCENE")
fi

printf 'BENCHMARK_PLAN profile=%s scenes=%s renderer=%s backend=%s\n' \
  "$PROFILE" "${SCENES[*]}" "$RENDERER" "$BACKEND"
[ "$ACTION" = run ] || exit 0

safe_mkdir ".tmp/dusklight-benchmark/$PROFILE"
safe_mkdir "artifacts/dusklight-psp-benchmarks/$PROFILE"
printf '%s' "$PROFILE" >"$PROFILE_FILE"
PENDING=0
COMPLETED=0

for current in "${SCENES[@]}"; do
  mode="benchmark_$current"
  request_id="$(timestamp_utc)-benchmark-$PROFILE-$current"
  response="$(assert_project_path \
    ".test-data/ppsspp/gui-runner/requests/$request_id/response.json")"
  if ! "$SCRIPT_DIR/ppsspp-gui-runner-request.sh" --run \
    --request-id "$request_id" \
    --eboot "$EBOOT" \
    --game-id DUSKLIGHT_PSP \
    --config "$CONFIG" \
    --mode "$mode" \
    --presentation "$PRESENTATION" \
    --backend "$BACKEND" \
    --renderer "$RENDERER" \
    --timeout "$TIMEOUT" \
    --marker BENCHMARK.OK=DUSKLIGHT_PSP_BENCHMARK_OK \
    --package "$DATA=data" \
    --package "$PROFILE_FILE=DUSKLIGHT.BENCHMARK_PROFILE"; then
    boot=false
    classification=PENDING_GUI_EXECUTION
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
    printf 'BENCHMARK_PENDING profile=%s scene=%s classification=PENDING_GUI_EXECUTION host_classification=%s\n' \
      "$PROFILE" "$current" "$classification"
    PENDING=$((PENDING + 1))
    continue
  fi
  [ -f "$response" ] || die "réponse runner PPSSPP absente"
  metrics="$(assert_project_path \
    ".test-data/ppsspp/gui-runner/sessions/$request_id/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PSP/BENCHMARK.METRICS")"
  "$SCRIPT_DIR/validate-dusklight-benchmark-metrics.sh" "$metrics"
  cp -- "$metrics" "$OUTPUT/$current.metrics"
  COMPLETED=$((COMPLETED + 1))
done

if [ "$SCENE" = all ] && [ "$PENDING" -eq 0 ]; then
  {
    printf 'benchmark_contract=DUSKLIGHT_PSP_RENDERING_CONTRACT_V1\n'
    printf 'profile=%s\n' "$PROFILE"
    printf 'scene_count=%s\n' "${#SCENES[@]}"
    printf 'eboot_sha256=%s\n' "$(sha256_file "$EBOOT")"
    printf 'backend=%s\nrenderer=%s\n' "$BACKEND" "$RENDERER"
    printf 'transport=persistent_gui_broker\n'
    printf 'network_used=false\n'
  } >"$OUTPUT/RUN.MANIFEST"
fi
if [ "$PENDING" -ne 0 ]; then
  {
    printf 'classification=PENDING_GUI_EXECUTION\n'
    printf 'profile=%s\ncompleted=%s\npending=%s\n' \
      "$PROFILE" "$COMPLETED" "$PENDING"
    printf 'transport=persistent_gui_broker\n'
  } >"$PROFILE_ROOT/RUN.PENDING.MANIFEST"
fi

printf 'DUSKLIGHT_PSP_BENCHMARK_SUITE_RESULT profile=%s completed=%s pending=%s transport=persistent_gui_broker\n' \
  "$PROFILE" "$COMPLETED" "$PENDING"
