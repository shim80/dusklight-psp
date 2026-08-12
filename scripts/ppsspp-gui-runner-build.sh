#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

[ "$(uname -s)" = Darwin ] || die "le runner GUI est réservé à macOS"
SOURCE_DIR="$(assert_project_path "tools/macos/ppsspp-gui-runner")"
OUTPUT_ROOT="$(assert_project_path ".tools/ppsspp-gui-runner")"
APP="$(assert_project_path \
  ".tools/ppsspp-gui-runner/DusklightPPSSPPRunner.app")"
CONTENTS="$APP/Contents"
MACOS="$CONTENTS/MacOS"
RESOURCES="$CONTENTS/Resources"
EXECUTABLE="$MACOS/DusklightPPSSPPRunner"
RUNNER_RESOURCE="$RESOURCES/runner.py"
INFO="$CONTENTS/Info.plist"

safe_mkdir .tools/ppsspp-gui-runner
safe_mkdir .tools/ppsspp-gui-runner/DusklightPPSSPPRunner.app/Contents/MacOS
safe_mkdir .tools/ppsspp-gui-runner/DusklightPPSSPPRunner.app/Contents/Resources

RUNNER_HASH="$(sha256_file "$SOURCE_DIR/runner.py")"
BUILD="${RUNNER_HASH:0:12}"
sed \
  -e 's/@RUNNER_VERSION@/1.0.0/g' \
  -e "s/@RUNNER_BUILD@/$BUILD/g" \
  "$SOURCE_DIR/Info.plist.in" >"$INFO"
cp -- "$SOURCE_DIR/runner.py" "$RUNNER_RESOURCE"
chmod 0644 "$RUNNER_RESOURCE"
/usr/bin/clang -std=c11 -Wall -Wextra -Werror -Os \
  "$SOURCE_DIR/launcher.c" -o "$EXECUTABLE"
/usr/bin/plutil -lint "$INFO" >/dev/null
/usr/bin/codesign --force --deep --sign - "$APP"
/usr/bin/codesign --verify --deep --strict "$APP"

{
  printf 'runner_sha256=%s\n' "$RUNNER_HASH"
  printf 'schema_sha256=%s\n' \
    "$(sha256_file "$SOURCE_DIR/request_schema.json")"
  printf 'info_template_sha256=%s\n' \
    "$(sha256_file "$SOURCE_DIR/Info.plist.in")"
  printf 'generated_executable_sha256=%s\n' \
    "$(sha256_file "$EXECUTABLE")"
  printf 'launcher_source_sha256=%s\n' \
    "$(sha256_file "$SOURCE_DIR/launcher.c")"
  printf 'codesign=adhoc-verified\n'
} >"$OUTPUT_ROOT/BUILD.MANIFEST"

printf 'PPSSPP_GUI_RUNNER_BUILD_OK app=%s runner_sha256=%s\n' \
  "${APP#"$PROJECT_ROOT"/}" "$RUNNER_HASH"
