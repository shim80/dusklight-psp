#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"
TARGET="gui/$(id -u)/com.dusklight.ppsspp-gui-broker"
if ! /bin/launchctl print "$TARGET" >/dev/null 2>&1; then
  printf 'BROKER_NOT_BOOTSTRAPPED\n' >&2
  exit 69
fi
if "$SCRIPT_DIR/status-ppsspp-gui-broker.sh" --quiet; then
  "$SCRIPT_DIR/status-ppsspp-gui-broker.sh"
  exit 0
fi
classification="$($SCRIPT_DIR/status-ppsspp-gui-broker.sh 2>/dev/null | sed -n 's/^classification=//p')"
if [ "$classification" = BROKER_BUSY ]; then
  "$SCRIPT_DIR/status-ppsspp-gui-broker.sh"
  exit 0
fi
/bin/launchctl kickstart -k "$TARGET"
deadline=$((SECONDS + 15))
while [ "$SECONDS" -lt "$deadline" ]; do
  "$SCRIPT_DIR/status-ppsspp-gui-broker.sh" --quiet && {
    "$SCRIPT_DIR/status-ppsspp-gui-broker.sh"
    exit 0
  }
  sleep 0.25
done
die "BROKER_STALE après kickstart"
