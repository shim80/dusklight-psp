#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

home_dir=
while [ "$#" -gt 0 ]; do
  case "$1" in
    --home)
      home_dir="$2"
      shift 2
      ;;
    *)
      die "argument inconnu : $1"
      ;;
  esac
done
[ -n "$home_dir" ] || die "--home est requis"
home_dir="$(assert_project_path "$home_dir")"

SOURCE="$(assert_project_path "tools/reference/dusklight-desktop-input-profile.cpp")"
BIN_DIR="$(assert_project_path ".tools/reference/bin")"
GENERATOR="$BIN_DIR/dusklight-desktop-input-profile"
PREF_DIR="$home_dir/Library/Application Support/TwilitRealm/Dusklight"
OUTPUT="$PREF_DIR/keyboard_bindings.dat"
CONFIG="$PREF_DIR/config.json"
CXX="$SCRIPT_DIR/reference-clangxx-macos.sh"
SDK="$(xcrun --show-sdk-path)"

mkdir -p "$BIN_DIR" "$PREF_DIR"
if [ ! -x "$GENERATOR" ] || [ "$SOURCE" -nt "$GENERATOR" ]; then
  "$CXX" -isysroot "$SDK" -std=c++17 -O2 "$SOURCE" -o "$GENERATOR"
fi
"$GENERATOR" "$OUTPUT" "$CONFIG"
[ "$(stat -f %z "$OUTPUT")" = 740 ] ||
  die "profil clavier généré de taille inattendue"
[ -s "$CONFIG" ] || die "configuration Classic isolée absente"

printf 'DUSKLIGHT_DESKTOP_INPUT_PROFILE_READY keyboard=%s config=%s\n' \
  "${OUTPUT#"$PROJECT_ROOT"/}" "${CONFIG#"$PROJECT_ROOT"/}"
