#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

VERSION=v20260618.032059
EXPECTED_SHA=a9cc9903761e60cf70d7d771bd0c482be1943e273717782d71c33313afeb6080
CACHE_DIR="$(assert_project_path ".tools/reference/cache/dawn/$VERSION")"
ARCHIVE="$CACHE_DIR/dawn-darwin-arm64.tar.gz"
INSTALL_ROOT="$(assert_project_path ".tools/reference/dusklight-desktop/dependencies")"
INSTALL_DIR="$INSTALL_ROOT/dawn-$VERSION-darwin-arm64"
CONFIG_REL=lib/cmake/Dawn/DawnConfig.cmake

[ -f "$ARCHIVE" ] || die "asset Dawn autorisé absent : ${ARCHIVE#"$PROJECT_ROOT"/}"
actual_sha="$(sha256_file "$ARCHIVE")"
[ "$actual_sha" = "$EXPECTED_SHA" ] ||
  die "SHA-256 Dawn invalide : $actual_sha"

archive_size="$(wc -c <"$ARCHIVE" | tr -d '[:space:]')"
[ "$archive_size" -ge 4000000 ] && [ "$archive_size" -le 7000000 ] ||
  die "taille Dawn hors plage raisonnable : $archive_size"

entries="$(tar -tzf "$ARCHIVE")"
[ -n "$entries" ] || die "archive Dawn vide"
if printf '%s\n' "$entries" |
  awk '
    /^\// { bad = 1 }
    {
      count = split($0, parts, "/")
      for (i = 1; i <= count; i++) {
        if (parts[i] == "..") bad = 1
      }
    }
    END { exit bad ? 0 : 1 }
  '; then
  die "chemin absolu ou composant .. détecté dans Dawn"
fi
if tar -tvzf "$ARCHIVE" | awk 'substr($1, 1, 1) == "l" || substr($1, 1, 1) == "h" { found = 1 } END { exit found ? 0 : 1 }'; then
  die "lien symbolique ou lien dur refusé dans Dawn"
fi
printf '%s\n' "$entries" | grep -Fx "./$CONFIG_REL" >/dev/null ||
  die "$CONFIG_REL absent de l'archive"
printf '%s\n' "$entries" | grep -Fx './lib/libwebgpu_dawn.a' >/dev/null ||
  die "libwebgpu_dawn.a absent de l'archive"
printf '%s\n' "$entries" | grep -Fx './include/webgpu/webgpu.h' >/dev/null ||
  die "header WebGPU absent de l'archive"

safe_mkdir ".tools/reference/dusklight-desktop/dependencies"
if [ -e "$INSTALL_DIR" ]; then
  [ -d "$INSTALL_DIR" ] || die "destination Dawn invalide"
  [ -f "$INSTALL_DIR/$CONFIG_REL" ] ||
    die "destination Dawn existante incomplète"
else
  mkdir -- "$INSTALL_DIR"
  tar -xzf "$ARCHIVE" -C "$INSTALL_DIR"
fi

[ -f "$INSTALL_DIR/$CONFIG_REL" ] || die "DawnConfig.cmake absent après extraction"
[ -f "$INSTALL_DIR/lib/libwebgpu_dawn.a" ] ||
  die "bibliothèque Dawn absente après extraction"
[ -f "$INSTALL_DIR/include/dawn/native/MetalBackend.h" ] ||
  die "backend Metal Dawn absent"

arch_info="$(lipo -info "$INSTALL_DIR/lib/libwebgpu_dawn.a" 2>&1)"
printf '%s\n' "$arch_info" | grep -Eq '(architecture: arm64|are: arm64([[:space:]]|$))' ||
  die "bibliothèque Dawn non arm64 : $arch_info"
if printf '%s\n' "$arch_info" | grep -Eq 'x86_64'; then
  die "architecture x86_64 inattendue dans Dawn"
fi

MANIFEST="$INSTALL_DIR/HYDRATION.MANIFEST"
{
  printf 'format=DUSKLIGHT_DESKTOP_DAWN_HYDRATION_V1\n'
  printf 'release=%s\n' "$VERSION"
  printf 'asset=dawn-darwin-arm64.tar.gz\n'
  printf 'asset_sha256=%s\n' "$actual_sha"
  printf 'asset_size=%s\n' "$archive_size"
  printf 'config_path=%s\n' "$CONFIG_REL"
  printf 'library_architecture=arm64\n'
  printf 'network_access_performed=false\n'
} >"$MANIFEST"

printf 'DUSKLIGHT_DESKTOP_DAWN_READY root=%s sha256=%s size=%s\n' \
  "${INSTALL_DIR#"$PROJECT_ROOT"/}" "$actual_sha" "$archive_size"
