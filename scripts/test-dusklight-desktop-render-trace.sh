#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

PATCH="$(assert_project_path "reference/desktop/patches/0009-dusklight-reference-render-state-trace.patch")"
TRACE="test/fixtures/desktop-render-trace-v1.jsonl"

grep -Fq 'kFrameLimit = 4' "$PATCH" || die "borne de frames absente"
grep -Fq 'kShapeCapacity = 8192' "$PATCH" || die "borne de soumissions absente"
for event_type in render_model_matrix render_view_matrix render_projection_matrix render_screen_bounds; do
  grep -Fq "\\\"type\\\":\\\"$event_type\\\"" "$PATCH" ||
    die "preuve géométrique absente : $event_type"
done
if grep -Eq 'GX(Set|Call|Draw|Flush)|drawDone|waitFor' "$PATCH"; then
  die "la trace neutre ne doit ajouter aucun appel de rendu ou flush"
fi
"$SCRIPT_DIR/validate-dusklight-desktop-render-trace.sh" --trace "$TRACE"
/usr/bin/python3 - "$PROJECT_ROOT" <<'PY'
import importlib.util
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
source = root / "tools/macos/dusklight-ppsspp-gui-broker/dusklight_desktop_application_adapter.py"
spec = importlib.util.spec_from_file_location("desktop_adapter_geometry_test", source)
adapter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(adapter)
events = [
    {"type": "render_submission", "frame": 1, "submission_id": 2},
    {"type": "render_model_matrix", "frame": 1, "submission_id": 2,
     "value": [0.0] * 12},
    {"type": "render_view_matrix", "frame": 1, "submission_id": 2,
     "value": [0.0] * 12},
    {"type": "render_projection_matrix", "frame": 1, "submission_id": 2,
     "value": [0.0] * 16},
    {"type": "render_screen_bounds", "frame": 1, "submission_id": 2,
     "camera_space_depth": [1.0, 2.0], "screen_bounds": [-1.0, -1.0, 1.0, 1.0]},
]
if not adapter._geometry_valid(events):
    raise SystemExit("preuve géométrique valide rejetée")
if adapter._geometry_valid(events[:-1]):
    raise SystemExit("bounds absents acceptés")
PY
printf 'DUSKLIGHT_DESKTOP_RENDER_TRACE_TESTS_OK\n'
