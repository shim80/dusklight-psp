#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"
LABEL=com.dusklight.ppsspp-gui-broker
TARGET="gui/$(id -u)/$LABEL"
if /bin/launchctl print "$TARGET" >/dev/null 2>&1; then
  /bin/launchctl bootout "$TARGET"
fi
printf 'GUI_BROKER_UNBOOTSTRAP_OK label=%s\n' "$LABEL"
