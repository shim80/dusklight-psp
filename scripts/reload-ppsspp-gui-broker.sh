#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"
"$SCRIPT_DIR/ensure-ppsspp-gui-broker.sh" >/dev/null
"$SCRIPT_DIR/build-ppsspp-gui-broker.sh"
STATE="$(assert_project_path ".test-data/ppsspp-gui-broker")"
old_pid="$(/usr/bin/python3 - "$STATE/heartbeat.json" <<'PY'
import json, pathlib, sys
print(json.loads(pathlib.Path(sys.argv[1]).read_text())["supervisor_pid"])
PY
)"
/bin/launchctl kickstart -k \
  "gui/$(id -u)/com.dusklight.ppsspp-gui-broker"
deadline=$((SECONDS + 20))
while [ "$SECONDS" -lt "$deadline" ]; do
  if [ -s "$STATE/heartbeat.json" ]; then
    new_pid="$(/usr/bin/python3 - "$STATE/heartbeat.json" <<'PY'
import json, pathlib, sys
print(json.loads(pathlib.Path(sys.argv[1]).read_text())["supervisor_pid"])
PY
)"
    if [ "$new_pid" != "$old_pid" ] && "$SCRIPT_DIR/status-ppsspp-gui-broker.sh" --quiet; then
      printf 'GUI_BROKER_RELOAD_OK old_pid=%s new_pid=%s\n' "$old_pid" "$new_pid"
      exit 0
    fi
  fi
  sleep 0.25
done
die "GUI_BROKER_RELOAD_TIMEOUT"
