#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"
SCENARIO="${1:-}"
[[ "$SCENARIO" =~ ^[a-z0-9_]+$ ]] || die "identifiant scénario invalide"
[[ "$SCENARIO" != link_* ]] || die "les scénarios Link exigent une DTRC native"
OUTPUT="$(assert_project_path "build/reports/parity/$SCENARIO")"
safe_mkdir "build/reports/parity/$SCENARIO"
/usr/bin/python3 - "$PROJECT_ROOT" "$SCENARIO" "$OUTPUT" <<'PY'
import hashlib, json, pathlib, re, sys
root, scenario, output = pathlib.Path(sys.argv[1]), sys.argv[2], pathlib.Path(sys.argv[3])
inventory = (root / "reference/parity/scenarios/scenarios.toml").read_text()
if scenario not in re.findall(r'^id = "([a-z0-9_]+)"$', inventory, re.MULTILINE):
    raise SystemExit("scénario absent de l'inventaire")
if scenario.startswith("startup_"):
    evidence = [root / "reference/parity/startup-ui-parity.csv"]
    reason = "scenario-specific PSP DTRC not implemented; startup structural matrix retained"
elif scenario.startswith("f_sp108_"):
    evidence = [root / "build/reports/f-sp108-adapter-parity.json"]
    reason = "nine adapters are lifecycle/transform-only; behavior and visual parity remain missing"
elif scenario.startswith("f_sp110_geyser_"):
    evidence = [root / "docs/reports/131-geyser-parity-classification.md"]
    reason = "geyser evidence is classified but no scenario-specific PSP DTRC exists"
else:
    evidence = [
        root / "build/reports/PARITY_SCENE_MATRIX.csv",
        root / "build/reports/PARITY_ACTOR_MATRIX.csv",
    ]
    reason = "scenario-specific PSP DTRC not implemented in the current canonical runtime"
records=[]
for path in evidence:
    if path.is_file():
        records.append({
            "path": path.relative_to(root).as_posix(),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "size": path.stat().st_size,
        })
identity={}
for line in (root / "build/reports/PARITY_BUILD_ID.metrics").read_text().splitlines():
    key, separator, value=line.partition("=")
    if separator: identity[key]=value
payload={
    "schema": "dusklight.psp.parity.scenario-acquisition.v1",
    "scenario": scenario,
    "acquisition_status": "COMPLETE",
    "classification": "MISSING_OR_NOT_PORTED",
    "dtrc_native": False,
    "parity_build_id": identity["parity_build_id"],
    "reason": reason,
    "evidence": records,
    "fabricated_trace": False,
    "error_code": 0,
}
(output / "psp-acquisition.json").write_text(json.dumps(payload, indent=2, sort_keys=True)+"\n")
(output / "psp.metrics").write_text(
    f"scenario={scenario}\nclassification=MISSING_OR_NOT_PORTED\n"
    "acquisition_complete=true\ndtrc_native=false\nfabricated_trace=false\nerror_code=0\n"
)
print(f"PSP_PARITY_SCENARIO_ACQUIRED scenario={scenario} classification=MISSING_OR_NOT_PORTED dtrc_native=false")
PY
