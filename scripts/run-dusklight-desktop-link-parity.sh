#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

ACTION=verify
MOBILE_ONLY=0
REQUIRE_POSE_CHECKPOINTS=0
EXECUTED=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --run) ACTION=run ;;
    --verify-existing) ACTION=verify ;;
    --mobile-only) MOBILE_ONLY=1 ;;
    --require-pose-checkpoints) REQUIRE_POSE_CHECKPOINTS=1 ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done

SCENARIOS=(
  link_idle_full_cycle
  link_walk
  link_run
  link_turn_90
  link_turn_180
  link_stop
  link_slope
  link_camera_follow
  link_collision_wall
  link_ground_contact
)
DURATIONS=(13 12 12 10 9 10 14 11 16 9)
ANALOG=(
  ""
  "120-300:0:60"
  "120-300:0:127"
  "120-240:127:0"
  "120-128:0:-127"
  "120-210:0:127"
  "120-360:-55:95"
  "120-260:60:60"
  "120-420:0:127"
  ""
)

TRACE_TOOL="$(assert_project_path \
  "tools/dusk_desktop_parity_trace/dusk_desktop_parity_trace.py")"
COMPARE_TOOL="$(assert_project_path \
  "tools/dusk_parity_compare/dusk_parity_compare.py")"
TOLERANCES="$(assert_project_path "reference/parity/tolerances.toml")"
RUN_STAMP="$(timestamp_utc)"

for index in "${!SCENARIOS[@]}"; do
  scenario="${SCENARIOS[$index]}"
  if [ "$MOBILE_ONLY" -eq 1 ] &&
     { [ "$scenario" = link_idle_full_cycle ] ||
       [ "$scenario" = link_ground_contact ]; }; then
    continue
  fi
  EXECUTED=$((EXECUTED + 1))
  report_dir="$(assert_project_path "build/reports/parity/$scenario")"
  safe_mkdir "build/reports/parity/$scenario"
  if [ "$ACTION" = run ]; then
    run_id="parity-v3-${scenario//_/-}-$RUN_STAMP"
    args=(
      --variant trace-relwithdebinfo
      --duration "${DURATIONS[$index]}"
      --transport direct_gui
      --run-id "$run_id"
      --trace-scenario "$scenario"
      --stage D_MN10,9,0,0
    )
    if [ -n "${ANALOG[$index]}" ]; then
      args+=(--trace-analog "${ANALOG[$index]}")
    fi
    "$SCRIPT_DIR/run-dusklight-desktop-reference.sh" "${args[@]}"
    session="$(assert_project_path \
      ".test-data/dusklight-reference/sessions/$run_id")"
    python3 -B "$TRACE_TOOL" "$session/stdout.log" \
      --trace "$report_dir/desktop.dtrc-v3.jsonl" \
      --metrics "$report_dir/desktop.metrics.json"
  fi

  trace="$report_dir/desktop.dtrc-v3.jsonl"
  metrics="$report_dir/desktop.metrics.json"
  [ -s "$trace" ] || die "trace desktop absente : $scenario"
  [ -s "$metrics" ] || die "métriques desktop absentes : $scenario"
  python3 -B "$COMPARE_TOOL" "$trace" "$trace" \
    --tolerances "$TOLERANCES" \
    --output "$report_dir/desktop.self-compare.json"
  manifest="$(assert_project_path \
    "reference/parity/scenarios/$scenario.json")"
  python3 -B - "$scenario" "$trace" "$metrics" \
    "$report_dir/desktop.self-compare.json" "$manifest" \
    "$REQUIRE_POSE_CHECKPOINTS" <<'PY'
import json
import pathlib
import sys

scenario = sys.argv[1]
events = [
    json.loads(line)
    for line in pathlib.Path(sys.argv[2]).read_text().splitlines()
]
metrics = json.loads(pathlib.Path(sys.argv[3]).read_text())
comparison = json.loads(pathlib.Path(sys.argv[4]).read_text())
manifest = json.loads(pathlib.Path(sys.argv[5]).read_text())
require_pose = sys.argv[6] == "1"
required = {
    "actor_transform", "actor_state", "animation_frame",
    "camera_state", "floor_contact", "joint_reference_point",
}
missing = sorted(required - set(metrics.get("event_types", {})))
if metrics.get("scenario_id") != scenario:
    raise SystemExit(f"scenario_id incorrect: {scenario}")
if metrics.get("event_count", 0) <= 0 or metrics.get("sampled_ticks", 0) <= 0:
    raise SystemExit(f"trace vide: {scenario}")
if any(
    event["stage"] != manifest["stage"]
    or event["room"] != manifest["room"]
    or event["layer"] != manifest["layer"]
    for event in events
):
    raise SystemExit(f"changement de scène inattendu: {scenario}")
if metrics.get("last_tick", -1) < manifest["maximum_ticks"] - 1:
    raise SystemExit(f"trace incomplète: {scenario}")
if missing:
    raise SystemExit(f"événements absents {scenario}: {missing}")
if comparison.get("status") != "MATCH_WITH_TOLERANCE":
    raise SystemExit(f"auto-comparaison invalide: {scenario}")
if comparison.get("value_divergence_events") != 0:
    raise SystemExit(f"divergence auto-comparaison: {scenario}")
if require_pose:
    event_types = set(metrics.get("event_types", {}))
    required_pose = {
        "animation_update_enter", "animation_update_exit",
        "grounding_enter", "grounding_exit",
        "pose_build_enter", "pose_build_exit",
    }
    missing_pose = sorted(required_pose - event_types)
    if missing_pose:
        raise SystemExit(f"checkpoints de pose absents {scenario}: {missing_pose}")
    by_tick = {}
    for event in events:
        event_type = event["event_type"]
        if event_type in required_pose:
            by_tick.setdefault(event["game_tick"], []).append(event_type)
    expected = [
        "animation_update_enter", "animation_update_exit",
        "grounding_enter", "grounding_exit",
        "grounding_enter", "grounding_exit",
        "pose_build_enter", "pose_build_exit",
    ]
    complete = [types for types in by_tick.values() if types == expected]
    if not complete:
        raise SystemExit(f"ordre animation/grounding/pose absent: {scenario}")
PY
  printf 'DESKTOP_LINK_PARITY_SCENARIO_OK scenario=%s events=%s ticks=%s\n' \
    "$scenario" \
    "$(python3 -B -c 'import json,sys;print(json.load(open(sys.argv[1]))["event_count"])' "$metrics")" \
    "$(python3 -B -c 'import json,sys;print(json.load(open(sys.argv[1]))["sampled_ticks"])' "$metrics")"
done

printf 'DUSKLIGHT_DESKTOP_LINK_PARITY_OK scenarios=%s action=%s\n' \
  "$EXECUTED" "$ACTION"
