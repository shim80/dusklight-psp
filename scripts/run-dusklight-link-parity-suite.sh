#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

ACTION=plan
while [ "$#" -gt 0 ]; do
  case "$1" in
    --plan) ACTION=plan ;;
    --run) ACTION=run ;;
    --run-psp) ACTION=psp ;;
    --compare-existing) ACTION=compare ;;
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
COMPARE_TOOL="$(assert_project_path \
  "tools/dusk_parity_compare/dusk_parity_compare.py")"
TOLERANCES="$(assert_project_path "reference/parity/tolerances.toml")"
SUMMARY="$(assert_project_path \
  "build/reports/parity/link-suite-summary.json")"

if [ "$ACTION" = plan ]; then
  for scenario in "${SCENARIOS[@]}"; do
    "$SCRIPT_DIR/run-ppsspp-dusklight-psp.sh" \
      --plan --mode parity_trace --scenario "$scenario" \
      --presentation game --backend opengl --transport gui \
      --timeout 300
  done
  printf 'DUSKLIGHT_LINK_PARITY_SUITE_PLAN scenarios=%s transport=persistent_gui_broker\n' \
    "${#SCENARIOS[@]}"
  exit 0
fi

if [ "$ACTION" = run ]; then
  "$SCRIPT_DIR/run-dusklight-desktop-link-parity.sh" --run
fi

for scenario in "${SCENARIOS[@]}"; do
  report_dir="$(assert_project_path "build/reports/parity/$scenario")"
  if [ "$ACTION" = run ] || [ "$ACTION" = psp ]; then
    "$SCRIPT_DIR/run-ppsspp-dusklight-psp.sh" \
      --run --mode parity_trace --scenario "$scenario" \
      --presentation game --backend opengl --transport gui \
      --timeout 300
  fi
  desktop="$report_dir/desktop.dtrc-v3.jsonl"
  psp="$report_dir/psp.dtrc-v3.jsonl"
  [ -s "$desktop" ] || die "trace desktop absente : $scenario"
  [ -s "$psp" ] || die "trace PSP absente : $scenario"
  python3 -B "$COMPARE_TOOL" "$desktop" "$psp" \
    --tolerances "$TOLERANCES" \
    --output "$report_dir/desktop-psp.compare.json"
done

python3 -B - "$SUMMARY" "${SCENARIOS[@]}" <<'PY'
import json
import pathlib
import sys

output = pathlib.Path(sys.argv[1])
root = output.parent
scenarios = []
for scenario in sys.argv[2:]:
    report = json.loads(
        (root / scenario / "desktop-psp.compare.json").read_text()
    )
    scenarios.append({
        "scenario": scenario,
        "status": report["status"],
        "desktop_events": report["desktop_events"],
        "psp_events": report["psp_events"],
        "aligned_events": report["aligned_events"],
        "desktop_only_events": report["desktop_only_events"],
        "psp_only_events": report["psp_only_events"],
        "value_divergence_events": report["value_divergence_events"],
        "first_causal_divergence": report["first_causal_divergence"],
    })
counts = {}
for scenario in scenarios:
    counts[scenario["status"]] = counts.get(scenario["status"], 0) + 1
payload = {
    "schema": "dusklight.link.parity.suite.v1",
    "alignment": "stable_source_identity",
    "scenario_count": len(scenarios),
    "status_counts": counts,
    "scenarios": scenarios,
    "user_manual_direction_validation": "pending",
    "user_manual_acceptance": "pending",
}
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
PY

printf 'DUSKLIGHT_LINK_PARITY_SUITE_COMPARED scenarios=%s summary=%s\n' \
  "${#SCENARIOS[@]}" "${SUMMARY#"$PROJECT_ROOT"/}"
