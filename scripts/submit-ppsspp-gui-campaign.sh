#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"
CAMPAIGN=""
ACTION=resume
while [ "$#" -gt 0 ]; do
  case "$1" in
    --campaign) shift; CAMPAIGN="${1:-}" ;;
    --resume) ACTION=resume ;;
    --only-failed) ACTION=failed ;;
    --status) ACTION=status ;;
    *) die "option campagne inconnue : $1" ;;
  esac
  shift
done
[ "$CAMPAIGN" = global-parity ] || die "campagne inconnue : $CAMPAIGN"
STATE="$(assert_project_path ".test-data/ppsspp-gui-broker/campaigns/$CAMPAIGN")"
MANIFEST="$STATE/campaign.json"
RESULTS="$STATE/results.json"
safe_mkdir ".test-data/ppsspp-gui-broker/campaigns/$CAMPAIGN"

/usr/bin/python3 - "$PROJECT_ROOT" "$MANIFEST" "$RESULTS" <<'PY'
import json, pathlib, re, sys
root, manifest_path, results_path = map(pathlib.Path, sys.argv[1:])
source = (root / "reference/parity/scenarios/scenarios.toml").read_text()
scenario_ids = re.findall(r'^id = "([A-Za-z0-9_]+)"$', source, re.MULTILINE)
if len(scenario_ids) != 40 or len(set(scenario_ids)) != 40:
    raise SystemExit("inventaire scénarios différent de 40")
items = [{"id": value, "kind": "parity_scenario"} for value in scenario_ids]
items.extend([
    {"id": "benchmark_performance", "kind": "benchmark_performance"},
    {"id": "benchmark_psp_conservative", "kind": "benchmark_psp_conservative"},
    {"id": "release", "kind": "release"},
])
manifest = {"format": "DUSKLIGHT_PPSSPP_CAMPAIGN_V1", "campaign_id": "global-parity", "items": items}
manifest_path.write_text(json.dumps(manifest, sort_keys=True, indent=2) + "\n")
if not results_path.exists():
    results_path.write_text(json.dumps({item["id"]: "pending" for item in items}, sort_keys=True, indent=2) + "\n")
PY

if [ "$ACTION" = status ]; then
  /usr/bin/python3 - "$MANIFEST" "$RESULTS" <<'PY'
import collections, json, pathlib, sys
manifest, results = [json.loads(pathlib.Path(value).read_text()) for value in sys.argv[1:]]
counts = collections.Counter(results.values())
print(f"CAMPAIGN_STATUS id={manifest['campaign_id']} total={len(manifest['items'])} " + " ".join(f"{key}={counts[key]}" for key in ("pending", "succeeded", "failed")))
PY
  exit 0
fi

"$SCRIPT_DIR/ensure-ppsspp-gui-broker.sh" >/dev/null
failed=0
while IFS='|' read -r item kind state; do
  [ -n "$item" ] || continue
  if [ "$ACTION" = failed ]; then
    [ "$state" = failed ] || continue
  else
    [ "$state" != succeeded ] || continue
  fi
  result=failed
  case "$kind" in
    parity_scenario)
      if [[ "$item" == link_* ]]; then
        "$SCRIPT_DIR/run-ppsspp-dusklight-psp.sh" --run \
          --mode parity_trace --scenario "$item" --presentation game \
          --backend opengl --transport gui --timeout 300 && result=succeeded
      else
        "$SCRIPT_DIR/acquire-psp-parity-scenario.sh" "$item" && result=succeeded
      fi ;;
    benchmark_performance)
      "$SCRIPT_DIR/run-dusklight-psp-benchmarks.sh" --run \
        --profile performance --scene all --backend auto && result=succeeded ;;
    benchmark_psp_conservative)
      "$SCRIPT_DIR/run-dusklight-psp-benchmarks.sh" --run \
        --profile psp_conservative --scene all --backend opengl && result=succeeded ;;
    release)
      "$SCRIPT_DIR/test-dusklight-psp-runtime.sh" --release && result=succeeded ;;
    *) die "type de campagne interdit : $kind" ;;
  esac
  /usr/bin/python3 - "$RESULTS" "$item" "$result" <<'PY'
import json, os, pathlib, sys
path, item, result = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3]
value = json.loads(path.read_text()); value[item] = result
temporary = path.with_suffix(".json.tmp")
temporary.write_text(json.dumps(value, sort_keys=True, indent=2) + "\n")
os.replace(temporary, path)
PY
  [ "$result" = succeeded ] || failed=$((failed + 1))
done < <(/usr/bin/python3 - "$MANIFEST" "$RESULTS" <<'PY'
import json, pathlib, sys
manifest, results = [json.loads(pathlib.Path(value).read_text()) for value in sys.argv[1:]]
for item in manifest["items"]:
    print(item["id"], item["kind"], results.get(item["id"], "pending"), sep="|")
PY
)
"$0" --campaign "$CAMPAIGN" --status
[ "$failed" -eq 0 ]
