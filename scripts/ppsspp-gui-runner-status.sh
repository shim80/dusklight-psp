#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

APP="$(assert_project_path \
  ".tools/ppsspp-gui-runner/DusklightPPSSPPRunner.app")"
EXECUTABLE="$APP/Contents/MacOS/DusklightPPSSPPRunner"
RUNNER_RESOURCE="$APP/Contents/Resources/runner.py"
INFO="$APP/Contents/Info.plist"
SOURCE="$(assert_project_path "tools/macos/ppsspp-gui-runner/runner.py")"

status=READY
reason=validated
[ "$(uname -s)" = Darwin ] || {
  status=UNAVAILABLE
  reason=not_macos
}
if [ "$status" = READY ] && {
  [ ! -x "$EXECUTABLE" ] || [ ! -f "$RUNNER_RESOURCE" ] ||
  [ ! -f "$INFO" ];
}; then
  status=NOT_BUILT
  reason=generated_app_absent
fi
if [ "$status" = READY ] &&
   [ "$(sha256_file "$SOURCE")" != "$(sha256_file "$RUNNER_RESOURCE")" ]; then
  status=STALE
  reason=source_hash_mismatch
fi
if [ "$status" = READY ] && ! /usr/bin/plutil -lint "$INFO" >/dev/null; then
  status=INVALID
  reason=invalid_info_plist
fi
if [ "$status" = READY ] &&
   ! /usr/bin/codesign --verify --deep --strict "$APP" 2>/dev/null; then
  status=INVALID
  reason=invalid_code_signature
fi

printf 'ppsspp_gui_runner_status=%s\n' "$status"
printf 'reason=%s\n' "$reason"
printf 'app=%s\n' "${APP#"$PROJECT_ROOT"/}"
if [ -x "$EXECUTABLE" ]; then
  printf 'launcher_sha256=%s\n' "$(sha256_file "$EXECUTABLE")"
fi
if [ -f "$RUNNER_RESOURCE" ]; then
  printf 'runner_sha256=%s\n' "$(sha256_file "$RUNNER_RESOURCE")"
fi
[ "$status" = READY ]
