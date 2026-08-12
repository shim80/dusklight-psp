#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

require_project_root
exec /usr/bin/python3 -B \
  "$PROJECT_ROOT/tools/credit_free_visual_queue/runner.py" "$@"
