#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

[ "$(uname -s)" = Darwin ] || die "le broker GUI est réservé à macOS"
SOURCE="$(assert_project_path \
  "tools/macos/dusklight-ppsspp-gui-broker")"
APP="$(assert_project_path \
  ".tools/dusklight-ppsspp-gui-broker/DusklightPpssppGuiBroker.app")"
MACOS="$APP/Contents/MacOS"
RESOURCES="$APP/Contents/Resources"
BUNDLED_PPSSPP="$RESOURCES/PPSSPPSDL.app"
PINNED_PPSSPP="$(assert_project_path ".tools/ppsspp/PPSSPPSDL.app")"

safe_mkdir .tools/dusklight-ppsspp-gui-broker
safe_mkdir \
  .tools/dusklight-ppsspp-gui-broker/DusklightPpssppGuiBroker.app/Contents/MacOS
safe_mkdir \
  .tools/dusklight-ppsspp-gui-broker/DusklightPpssppGuiBroker.app/Contents/Resources
cp -- "$SOURCE/Info.plist.in" "$APP/Contents/Info.plist"
for source in supervisor.py request_worker.py artifact_collector.py \
  ppsspp_application_adapter.py dusklight_desktop_application_adapter.py; do
  cp -- "$SOURCE/$source" "$MACOS/$source"
  chmod 0755 "$MACOS/$source"
done
cp -- "$PROJECT_ROOT/tools/macos/ppsspp-gui-runner/runner.py" "$MACOS/runner.py"
git -C "$PROJECT_ROOT" rev-parse HEAD >"$MACOS/GIT_COMMIT"
chmod 0755 "$MACOS/runner.py"
[ -f "$RESOURCES/worker-version.fixture" ] || printf '2\n' >"$RESOURCES/worker-version.fixture"
source_ppsspp="$PINNED_PPSSPP/Contents/MacOS/PPSSPPSDL"
bundled_ppsspp="$BUNDLED_PPSSPP/Contents/MacOS/PPSSPPSDL"
if [ ! -x "$bundled_ppsspp" ] || \
   [ "$(sha256_file "$source_ppsspp")" != "$(sha256_file "$bundled_ppsspp")" ]; then
  rm -rf -- "$BUNDLED_PPSSPP"
  /usr/bin/ditto "$PINNED_PPSSPP" "$BUNDLED_PPSSPP"
fi
/usr/bin/plutil -lint "$APP/Contents/Info.plist" >/dev/null
/usr/bin/codesign --force --deep --sign - "$APP"
/usr/bin/codesign --verify --deep --strict "$APP"

{
  printf 'supervisor_sha256=%s\n' "$(sha256_file "$SOURCE/supervisor.py")"
  printf 'worker_sha256=%s\n' "$(sha256_file "$SOURCE/request_worker.py")"
  printf 'collector_sha256=%s\n' "$(sha256_file "$SOURCE/artifact_collector.py")"
  printf 'ppsspp_adapter_sha256=%s\n' \
    "$(sha256_file "$SOURCE/ppsspp_application_adapter.py")"
  printf 'desktop_adapter_sha256=%s\n' \
    "$(sha256_file "$SOURCE/dusklight_desktop_application_adapter.py")"
  printf 'runner_sha256=%s\n' \
    "$(sha256_file "$PROJECT_ROOT/tools/macos/ppsspp-gui-runner/runner.py")"
  printf 'ppsspp_sha256=%s\n' "$(sha256_file "$source_ppsspp")"
  printf 'request_schema_sha256=%s\n' \
    "$(sha256_file "$SOURCE/request_schema.json")"
  printf 'response_schema_sha256=%s\n' \
    "$(sha256_file "$SOURCE/response_schema.json")"
  printf 'codesign=adhoc-verified\n'
} >"$PROJECT_ROOT/.tools/dusklight-ppsspp-gui-broker/BUILD.MANIFEST"

printf 'PPSSPP_GUI_BROKER_BUILD_OK app=%s\n' "${APP#"$PROJECT_ROOT"/}"
