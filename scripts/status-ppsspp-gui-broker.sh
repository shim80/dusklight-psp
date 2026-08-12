#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"
QUIET=false
[ "${1:-}" != --quiet ] || QUIET=true
LABEL=com.dusklight.ppsspp-gui-broker
TARGET="gui/$(id -u)/$LABEL"
HEARTBEAT="$(assert_project_path ".test-data/ppsspp-gui-broker/heartbeat.json")"
MAILBOX="$(assert_project_path ".test-data/ppsspp-gui-broker/mailbox.request.json")"
classification=BROKER_NOT_BOOTSTRAPPED
reason=launchd_job_absent
if /bin/launchctl print "$TARGET" >/dev/null 2>&1; then
  classification=BROKER_STALE
  reason=heartbeat_absent
  if [ -s "$HEARTBEAT" ]; then
    read -r age protocol state < <(/usr/bin/python3 - "$HEARTBEAT" <<'PY'
import datetime, json, pathlib, sys
value = json.loads(pathlib.Path(sys.argv[1]).read_text())
updated = datetime.datetime.fromisoformat(value["updated_at"].replace("Z", "+00:00"))
age = (datetime.datetime.now(datetime.timezone.utc) - updated).total_seconds()
print(round(age), value.get("protocol_version"), value.get("status"))
PY
    )
    if [ "$protocol" != 3 ]; then
      classification=BROKER_PROTOCOL_MISMATCH
      reason="protocol_$protocol"
    elif [ ! -f "$MAILBOX" ]; then
      classification=BROKER_PROTOCOL_MISMATCH
      reason=request_mailbox_absent
    elif [ "$age" -le 3 ] && [ "$state" = BUSY ]; then
      classification=BROKER_BUSY
      reason=heartbeat_current
    elif [ "$age" -le 3 ]; then
      classification=BROKER_READY
      reason=heartbeat_current
    else
      reason="heartbeat_age_${age}s"
    fi
  fi
fi
$QUIET || {
  printf 'classification=%s\nreason=%s\n' "$classification" "$reason"
  [ ! -s "$HEARTBEAT" ] || cat "$HEARTBEAT"
}
[ "$classification" = BROKER_READY ] || [ "$classification" = BROKER_BUSY ]
