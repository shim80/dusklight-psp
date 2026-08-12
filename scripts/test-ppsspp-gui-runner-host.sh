#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

"$SCRIPT_DIR/ppsspp-gui-runner-build.sh"
"$SCRIPT_DIR/ppsspp-gui-runner-status.sh"
/usr/bin/python3 -m json.tool \
  "$PROJECT_ROOT/tools/macos/ppsspp-gui-runner/request_schema.json" >/dev/null
PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 -c \
  'compile(open("tools/macos/ppsspp-gui-runner/runner.py", encoding="utf-8").read(), "runner.py", "exec")'
"$SCRIPT_DIR/ppsspp-gui-runner-selftest.sh" --queue --timeout 120
printf '%s\n' \
  "PPSSPP_GUI_RUNNER_HOST_OK schema=true confinement_negative_cases=10"
