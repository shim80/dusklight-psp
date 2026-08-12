#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

"$SCRIPT_DIR/build-ppsspp-gui-broker.sh"
"$SCRIPT_DIR/bootstrap-ppsspp-gui-broker.sh" --generate-only >/dev/null
/usr/bin/plutil -lint \
  "$PROJECT_ROOT/.test-data/ppsspp-gui-broker/com.dusklight.ppsspp-gui-broker.plist" \
  >/dev/null
if rg -n 'Library/Launch|Library/LaunchDaemons|sudo' \
  "$PROJECT_ROOT/scripts/bootstrap-ppsspp-gui-broker.sh" \
  "$PROJECT_ROOT/.test-data/ppsspp-gui-broker/com.dusklight.ppsspp-gui-broker.plist" \
  >/dev/null; then
  die "installation LaunchAgent hors dépôt"
fi
/usr/bin/python3 -m json.tool \
  "$PROJECT_ROOT/tools/macos/dusklight-ppsspp-gui-broker/request_schema.json" \
  >/dev/null
/usr/bin/python3 -m json.tool \
  "$PROJECT_ROOT/tools/macos/dusklight-ppsspp-gui-broker/response_schema.json" \
  >/dev/null
for source in supervisor.py request_worker.py artifact_collector.py \
  ppsspp_application_adapter.py dusklight_desktop_application_adapter.py; do
  PYTHONDONTWRITEBYTECODE=1 /usr/bin/python3 -c \
    "compile(open('tools/macos/dusklight-ppsspp-gui-broker/$source', encoding='utf-8').read(), '$source', 'exec')"
done
rg -q 'runner.validate_request' \
  "$PROJECT_ROOT/tools/macos/dusklight-ppsspp-gui-broker/ppsspp_application_adapter.py" ||
  die "l’adaptateur PPSSPP ne valide pas la requête runner"
rg -q 'client_mirror' \
  "$PROJECT_ROOT/tools/macos/dusklight-ppsspp-gui-broker/request_worker.py" ||
  die "les destinations du collecteur ne sont pas confinées"
rg -q 'mailbox.request.json' \
  "$PROJECT_ROOT/tools/macos/dusklight-ppsspp-gui-broker/supervisor.py" ||
  die "mailbox de soumission autonome absente"
[ -x "$PROJECT_ROOT/.tools/dusklight-ppsspp-gui-broker/DusklightPpssppGuiBroker.app/Contents/Resources/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL" ] ||
  die "PPSSPP épinglé absent du bundle autonome"
if rg -n 'importlib|artifact_collector|ppsspp-gui-runner' \
  "$PROJECT_ROOT/tools/macos/dusklight-ppsspp-gui-broker/supervisor.py" \
  >/dev/null; then
  die "le superviseur importe une couche mutable"
fi
if rg -n '/usr/bin/open|-W -n' \
  "$PROJECT_ROOT/scripts/submit-ppsspp-gui-request.sh" \
  "$PROJECT_ROOT/scripts/submit-dusklight-desktop-gui-request.sh" \
  "$PROJECT_ROOT/scripts/start-ppsspp-gui-broker.sh" >/dev/null; then
  die "transport broker contient un lancement LaunchServices"
fi
rg -Fq 'subprocess.Popen(command' \
  "$PROJECT_ROOT/tools/macos/dusklight-ppsspp-gui-broker/dusklight_desktop_application_adapter.py" ||
  die "lancement Mach-O desktop direct absent"
printf 'GUI_BROKER_HOST_OK supervisor_minimal=true workers_fresh=true applications_allowlisted=2 serialized=true\n'
