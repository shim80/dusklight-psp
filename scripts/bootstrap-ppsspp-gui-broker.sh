#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"
GENERATE_ONLY=false
[ "${1:-}" != --generate-only ] || GENERATE_ONLY=true
LABEL=com.dusklight.ppsspp-gui-broker
DOMAIN="gui/$(id -u)"
STATE="$(assert_project_path ".test-data/ppsspp-gui-broker")"
PLIST="$STATE/$LABEL.plist"
SUPERVISOR="$(assert_project_path ".tools/dusklight-ppsspp-gui-broker/DusklightPpssppGuiBroker.app/Contents/MacOS/supervisor.py")"
"$SCRIPT_DIR/build-ppsspp-gui-broker.sh"
for directory in requests processing responses failed logs; do
  safe_mkdir ".test-data/ppsspp-gui-broker/$directory"
done
/usr/bin/python3 - "$PLIST" "$LABEL" "$SUPERVISOR" "$PROJECT_ROOT" "$STATE" <<'PY'
import pathlib, plistlib, sys
output, label, program, root, state = sys.argv[1:]
payload = {
    "Label": label, "ProgramArguments": [program], "WorkingDirectory": root,
    "RunAtLoad": True, "KeepAlive": True,
    "LimitLoadToSessionType": "Aqua",
    "StandardOutPath": str(pathlib.Path(state) / "logs/supervisor.stdout.log"),
    "StandardErrorPath": str(pathlib.Path(state) / "logs/supervisor.stderr.log"),
    "ProcessType": "Interactive",
    "EnvironmentVariables": {"PYTHONDONTWRITEBYTECODE": "1", "TMPDIR": str(pathlib.Path(root) / ".tmp")},
}
with open(output, "wb") as stream:
    plistlib.dump(payload, stream, sort_keys=True)
PY
/usr/bin/plutil -lint "$PLIST" >/dev/null
$GENERATE_ONLY && {
  printf 'GUI_BROKER_PLIST_OK path=%s\n' "${PLIST#"$PROJECT_ROOT"/}"
  exit 0
}
if /bin/launchctl print "$DOMAIN/$LABEL" >/dev/null 2>&1; then
  printf 'GUI_BROKER_ALREADY_BOOTSTRAPPED\n'
else
  rm -f -- \
    "$STATE/bootstrap.request" \
    "$STATE/stop.request" \
    "$STATE/reload.request" \
    "$STATE/heartbeat.json" \
    "$STATE/GUI_BROKER.METRICS" \
    "$STATE/GUI_BROKER.METRICS.json"
  /bin/launchctl bootstrap "$DOMAIN" "$PLIST"
fi
deadline=$((SECONDS + 15))
while [ "$SECONDS" -lt "$deadline" ]; do
  if "$SCRIPT_DIR/status-ppsspp-gui-broker.sh" --quiet; then
    printf 'GUI_BROKER_BOOTSTRAP_OK label=%s domain=%s\n' "$LABEL" "$DOMAIN"
    exit 0
  fi
  sleep 0.25
done
"$SCRIPT_DIR/status-ppsspp-gui-broker.sh" || true
die "GUI_BROKER_MACOS_PERMISSION_REQUIRED"
