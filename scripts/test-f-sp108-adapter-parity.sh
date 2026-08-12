#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

BUILD="$(assert_project_path "build/host/canonical-runtime")"
DATA="$(assert_project_path "build/assets/dusklight-psp/data")"
AUDIT="$(assert_project_path \
  "tools/dusk_f_sp108_adapter_audit/dusk_f_sp108_adapter_audit.py")"
OUTPUT="$(assert_project_path "build/reports/f-sp108-adapter-parity.json")"

python3 -B "$AUDIT" --output "$OUTPUT" --self-test-negatives

python3 -B - "$OUTPUT" <<'PY'
import json
import pathlib
import sys

result = json.loads(pathlib.Path(sys.argv[1]).read_text())
expected = {
    "source_records": 599,
    "adapters_audited": 9,
    "unique_original_classes": 6,
    "transform_matches": 9,
    "lifecycle_only_adapters": 9,
    "original_sources_ported": 0,
    "missing_behavior": 9,
    "visual_placeholders": 0,
    "negative_cases": 3,
    "cross_platform_matches": 0,
    "cross_platform_partial": 9,
}
for key, value in expected.items():
    if result.get(key) != value:
        raise SystemExit(
            f"F_SP108 adapter audit mismatch: {key}={result.get(key)!r}"
        )
PY

cmake -S "$PROJECT_ROOT/test/canonical-runtime" \
  -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD" --target startup_first_playable_host_test
"$BUILD/startup_first_playable_host_test" \
  "$DATA/stages/F_SP108/R01/room.dprm" \
  "$DATA/stages/F_SP108/R01/room.dptx" \
  "$DATA/stages/F_SP108/R01/room.dpcl" \
  "$DATA/stages/F_SP108/R01/room.dpsc"

printf '%s\n' \
  "F_SP108_ADAPTER_PARITY_SUITE_OK adapters=9 transform_matches=9 lifecycle_only=9 missing_behavior=9 status=PARTIAL_PARITY"
