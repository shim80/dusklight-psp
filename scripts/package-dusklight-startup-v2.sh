#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

"$SCRIPT_DIR/package-dusklight-psp.sh"
printf '%s\n' \
  "DUSKLIGHT_STARTUP_V2_PACKAGE_ALIAS canonical=artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP"
