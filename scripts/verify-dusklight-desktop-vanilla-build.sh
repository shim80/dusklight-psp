#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

variant=vanilla-relwithdebinfo
APP="$(assert_project_path "build/reference-desktop/$variant/Dusklight.app")"
EXE="$APP/Contents/MacOS/Dusklight"
INFO="$APP/Contents/Info.plist"
SOURCE="$(assert_project_path ".tools/reference/dusklight-desktop/source-vanilla")"
MARKER_DIR="$(assert_project_path ".test-data/dusklight-reference/markers")"
ARTIFACT_DIR="$(assert_project_path "artifacts/dusklight-desktop-reference")"
TMP_DIR="$(assert_project_path ".tmp/dusklight-desktop-reference")"
MARKER="$MARKER_DIR/VANILLA_BUILD.OK"
MANIFEST="$ARTIFACT_DIR/VANILLA_BUILD.MANIFEST"
INVENTORY="$TMP_DIR/vanilla-build-inventory.sha256"
EXPECTED_COMMIT=1bae8a5e6a812217ca33ba533e707ecfa64b1553
EXPECTED_TREE=dbd0de5c47808b5a78daa24c391166936bd6acc5

[ -d "$APP" ] || die "bundle vanilla absent"
[ -x "$EXE" ] || die "exécutable vanilla absent"
[ -f "$INFO" ] || die "Info.plist absent"
[ "$(git -C "$SOURCE" rev-parse HEAD)" = "$EXPECTED_COMMIT" ] ||
  die "commit vanilla inattendu"
[ "$(git -C "$SOURCE" rev-parse HEAD^{tree})" = "$EXPECTED_TREE" ] ||
  die "tree vanilla inattendu"
[ -z "$(git -C "$SOURCE" status --porcelain)" ] ||
  die "checkout vanilla modifié"

/usr/bin/file "$EXE" | grep -Fq 'Mach-O 64-bit executable arm64' ||
  die "exécutable vanilla non arm64"
/usr/bin/plutil -lint "$INFO" >/dev/null
/usr/bin/codesign --verify --deep --strict "$APP"

while IFS= read -r dependency; do
  case "$dependency" in
    /usr/lib/*|/System/Library/*)
      # Les bibliothèques système peuvent résider uniquement dans le dyld shared cache.
      ;;
    /*)
      [ -e "$dependency" ] || die "dépendance dynamique introuvable : $dependency"
      ;;
    @rpath/*|@executable_path/*|@loader_path/*)
      basename="${dependency##*/}"
      find "$APP" -type f -name "$basename" -print -quit | grep -q . ||
        die "dépendance bundle introuvable : $dependency"
      ;;
  esac
done < <(
  /usr/bin/otool -L "$EXE" |
    tail -n +2 |
    sed -E 's/^[[:space:]]*([^[:space:]]+).*/\1/'
)

safe_mkdir ".test-data/dusklight-reference/markers"
safe_mkdir "artifacts/dusklight-desktop-reference"
safe_mkdir ".tmp/dusklight-desktop-reference"

(
  cd "$APP"
  find . -type f -print0 |
    LC_ALL=C sort -z |
    while IFS= read -r -d '' file_path; do
      printf '%s  %s\n' \
        "$(/usr/bin/shasum -a 256 "$file_path" | awk '{print $1}')" \
        "${file_path#./}"
    done
) >"$INVENTORY"

executable_sha256="$(/usr/bin/shasum -a 256 "$EXE" | awk '{print $1}')"
executable_size="$(stat -f %z "$EXE")"
inventory_sha256="$(/usr/bin/shasum -a 256 "$INVENTORY" | awk '{print $1}')"
bundle_files="$(wc -l <"$INVENTORY" | tr -d ' ')"
bundle_identifier="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$INFO")"
bundle_version="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$INFO")"

{
  printf 'classification=READY_DUSKLIGHT_DESKTOP_VANILLA_BUILD\n'
  printf 'variant=%s\n' "$variant"
  printf 'dusklight_commit=%s\n' "$EXPECTED_COMMIT"
  printf 'dusklight_tree=%s\n' "$EXPECTED_TREE"
  printf 'bundle=build/reference-desktop/%s/Dusklight.app\n' "$variant"
  printf 'architecture=arm64\n'
  printf 'bundle_identifier=%s\n' "$bundle_identifier"
  printf 'bundle_version=%s\n' "$bundle_version"
  printf 'executable_sha256=%s\n' "$executable_sha256"
  printf 'executable_size=%s\n' "$executable_size"
  printf 'bundle_file_count=%s\n' "$bundle_files"
  printf 'bundle_inventory_sha256=%s\n' "$inventory_sha256"
  printf 'signature=adhoc_verified\n'
  printf 'vanilla_checkout=clean\n'
} >"$MANIFEST"
printf 'DUSKLIGHT_DESKTOP_VANILLA_BUILD_OK\n' >"$MARKER"

printf 'DUSKLIGHT_DESKTOP_VANILLA_BUILD_VERIFIED manifest=%s\n' \
  "${MANIFEST#"$PROJECT_ROOT"/}"
