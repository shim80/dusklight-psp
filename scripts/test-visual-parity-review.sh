#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

TOOL="$(assert_project_path \
  "tools/dusk_visual_parity_review/dusk_visual_parity_review.py")"
OUTPUT="$(assert_project_path "build/reports/visual-parity-selftest")"

python3 -B "$TOOL" \
  --self-test \
  --output "build/reports/visual-parity-selftest"

for file in \
  desktop.png psp.png side_by_side.png overlay_50.png \
  difference_heatmap.png landmarks.csv screen_bounds.csv; do
  [ -s "$OUTPUT/$file" ] || die "dérivé visuel absent : $file"
done

printf '%s\n' \
  "VISUAL_PARITY_PIPELINE_HOST_OK derivatives=7 real_capture_status=PENDING_GUI_EXECUTION"
