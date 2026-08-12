#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

VERSION=1.2.3
EXPECTED_SHA=662d257a66ebd750e93d68b6400be1bf8e04158571ccc808baa53abc53a7ac45
EXPECTED_SIZE=2248864
CACHE_DIR="$(assert_project_path ".tools/reference/cache/symgen/v$VERSION")"
BINARY="$CACHE_DIR/symgen-macos-arm64"

safe_mkdir ".tools/reference/cache/symgen/v$VERSION"
if [ ! -f "$BINARY" ]; then
  [ "${1:-}" = --download ] ||
    die "binaire symgen local absent ; utiliser --download après autorisation"
  curl -fL --proto '=https' --tlsv1.2 --retry 0 \
    -o "$BINARY" \
    "https://github.com/encounter/symgen/releases/download/v$VERSION/symgen-macos-arm64"
fi

actual_sha="$(sha256_file "$BINARY")"
[ "$actual_sha" = "$EXPECTED_SHA" ] ||
  die "SHA-256 symgen invalide : $actual_sha"
actual_size="$(wc -c <"$BINARY" | tr -d '[:space:]')"
[ "$actual_size" = "$EXPECTED_SIZE" ] ||
  die "taille symgen invalide : $actual_size"
file "$BINARY" | grep -Eq 'Mach-O 64-bit executable arm64' ||
  die "symgen n'est pas un exécutable Mach-O arm64"
lipo -info "$BINARY" | grep -Eq '(architecture: arm64|are: arm64([[:space:]]|$))' ||
  die "architecture symgen inattendue"
chmod 0755 "$BINARY"

printf 'DUSKLIGHT_DESKTOP_SYMGEN_READY path=%s sha256=%s size=%s\n' \
  "${BINARY#"$PROJECT_ROOT"/}" "$actual_sha" "$actual_size"
