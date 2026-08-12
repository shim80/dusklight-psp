#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

exec python3 "$PROJECT_ROOT/tools/opaque_order_compare/opaque_order_compare.py" \
  compare --root \
  "$PROJECT_ROOT/.test-data/visual-pipeline-results/opaque-order"
