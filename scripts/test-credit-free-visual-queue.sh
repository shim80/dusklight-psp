#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

require_project_root
/usr/bin/python3 -B -m unittest discover \
  -s "$PROJECT_ROOT/test/credit-free-visual-queue" -p 'test_*.py' -v
"$SCRIPT_DIR/run-credit-free-visual-queue.sh" --plan >/dev/null
"$SCRIPT_DIR/run-credit-free-visual-queue.sh" --status >/dev/null
printf '%s\n' \
  'CREDIT_FREE_VISUAL_QUEUE_HOST_OK tasks=8 negative_classes=9 release_runs=0'
