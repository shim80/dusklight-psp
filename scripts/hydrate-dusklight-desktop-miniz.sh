#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

VERSION=3.0.2
COMMIT=293d4db1b7d0ffee9756d035b9ac6f7431ef8492
VCPKG_COMMIT=89fe1e9a672ffe1c543e6956b92e69ab1a62b936
EXPECTED_SHA512=426054403121f84a2ac365f7545b35fb217b41061aebaffce483568d3d374d453ab87987c599a85f1f745e0ec7144a3181ed9b100f354e2823f165ba286b0611
EXPECTED_SHA256=c4b4c25a4eb81883448ff8924e6dba95c800094a198dc9ce66a292ac2ef8e018
CACHE_DIR="$(assert_project_path ".tools/reference/cache/miniz/$VERSION")"
ARCHIVE="$CACHE_DIR/miniz-$VERSION-source.tar.gz"
INSTALL_ROOT="$(assert_project_path ".tools/reference/dusklight-desktop/dependencies")"
INSTALL_DIR="$INSTALL_ROOT/miniz-$VERSION"
AMALGAMATED_DIR="$INSTALL_ROOT/miniz-$VERSION-amalgamated"
AMALGAMATION_BUILD="$(assert_project_path ".tmp/miniz-$VERSION-amalgamation-build")"
AMALGAMATED_ARCHIVE="$CACHE_DIR/miniz-$VERSION-amalgamated-local.zip"

[ -f "$ARCHIVE" ] || die "archive source miniz absente"
actual_sha512="$(shasum -a 512 "$ARCHIVE" | awk '{print $1}')"
[ "$actual_sha512" = "$EXPECTED_SHA512" ] ||
  die "SHA-512 miniz différent de l'autorité vcpkg : $actual_sha512"
actual_sha256="$(sha256_file "$ARCHIVE")"
[ "$actual_sha256" = "$EXPECTED_SHA256" ] ||
  die "SHA-256 miniz local inattendu : $actual_sha256"

archive_size="$(wc -c <"$ARCHIVE" | tr -d '[:space:]')"
[ "$archive_size" = 115766 ] || die "taille miniz inattendue : $archive_size"

entries="$(tar -tzf "$ARCHIVE")"
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
  die "chemin absolu ou composant .. détecté dans miniz"
fi
if tar -tvzf "$ARCHIVE" | awk 'substr($1, 1, 1) == "l" || substr($1, 1, 1) == "h" { found = 1 } END { exit found ? 0 : 1 }'; then
  die "lien symbolique ou lien dur refusé dans miniz"
fi

required_files=(
  CMakeLists.txt
  miniz.c
  miniz.h
  miniz_common.h
  miniz_tdef.c
  miniz_tdef.h
  miniz_tinfl.c
  miniz_tinfl.h
  miniz_zip.c
  miniz_zip.h
  LICENSE
  readme.md
  ChangeLog.md
)
for file in "${required_files[@]}"; do
  printf '%s\n' "$entries" |
    grep -Fx "miniz-$VERSION/$file" >/dev/null ||
    die "fichier miniz absent de l'archive : $file"
done

safe_mkdir ".tools/reference/dusklight-desktop/dependencies"
if [ -e "$INSTALL_DIR" ]; then
  [ -d "$INSTALL_DIR" ] || die "destination miniz invalide"
  [ -f "$INSTALL_DIR/CMakeLists.txt" ] ||
    die "destination miniz existante incomplète"
else
  tar -xzf "$ARCHIVE" -C "$INSTALL_ROOT"
fi
for file in "${required_files[@]}"; do
  [ -f "$INSTALL_DIR/$file" ] || die "fichier miniz absent après extraction : $file"
done

if [ ! -f "$AMALGAMATED_DIR/miniz.c" ] ||
   [ ! -f "$AMALGAMATED_DIR/miniz.h" ]; then
  rm -rf "$AMALGAMATION_BUILD" "$AMALGAMATED_DIR"
  mkdir -p "$AMALGAMATION_BUILD" "$AMALGAMATED_DIR"
  cmake \
    -S "$INSTALL_DIR" \
    -B "$AMALGAMATION_BUILD" \
    -DAMALGAMATE_SOURCES=ON \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_FUZZERS=OFF \
    -DINSTALL_PROJECT=OFF \
    >/dev/null
  cp "$AMALGAMATION_BUILD/amalgamation/miniz.c" "$AMALGAMATED_DIR/miniz.c"
  cp "$AMALGAMATION_BUILD/amalgamation/miniz.h" "$AMALGAMATED_DIR/miniz.h"
  cp "$INSTALL_DIR/ChangeLog.md" "$AMALGAMATED_DIR/ChangeLog.md"
  cp "$INSTALL_DIR/readme.md" "$AMALGAMATED_DIR/readme.md"
  cp "$INSTALL_DIR/LICENSE" "$AMALGAMATED_DIR/LICENSE"
  cp -R "$INSTALL_DIR/examples" "$AMALGAMATED_DIR/examples"
fi

for file in miniz.c miniz.h ChangeLog.md readme.md LICENSE; do
  [ -f "$AMALGAMATED_DIR/$file" ] ||
    die "fichier amalgamé miniz absent : $file"
done
[ -d "$AMALGAMATED_DIR/examples" ] ||
  die "exemples miniz absents de l'amalgame"
if grep -Eq '#include "miniz_(common|tdef|tinfl|zip|export)\.h"' \
  "$AMALGAMATED_DIR/miniz.c" "$AMALGAMATED_DIR/miniz.h"; then
  die "l'amalgame miniz conserve une dépendance vers un header modulaire"
fi

# L'archive n'est pas une preuve amont : elle est une reconstruction locale
# déterministe à partir de la source déjà authentifiée par le SHA-512 vcpkg.
find "$AMALGAMATED_DIR" -type d -exec chmod 0755 {} +
find "$AMALGAMATED_DIR" -type f -exec chmod 0644 {} +
find "$AMALGAMATED_DIR" -exec touch -t 198001010000 {} +
rm -f "$AMALGAMATED_ARCHIVE"
(
  cd "$AMALGAMATED_DIR"
  LC_ALL=C find . -type f -print |
    LC_ALL=C sort |
    zip -X -q "$AMALGAMATED_ARCHIVE" -@
)
amalgamated_sha256="$(sha256_file "$AMALGAMATED_ARCHIVE")"
amalgamated_size="$(wc -c <"$AMALGAMATED_ARCHIVE" | tr -d '[:space:]')"
amalgamated_inventory_sha256="$(
  (
    cd "$AMALGAMATED_DIR"
    LC_ALL=C find . -type f -print |
      LC_ALL=C sort |
      while IFS= read -r file; do
        shasum -a 256 "$file"
      done
  ) | shasum -a 256 | awk '{print $1}'
)"

MANIFEST="$INSTALL_DIR/HYDRATION.MANIFEST"
{
  printf 'format=DUSKLIGHT_DESKTOP_MINIZ_HYDRATION_V1\n'
  printf 'version=%s\n' "$VERSION"
  printf 'commit=%s\n' "$COMMIT"
  printf 'vcpkg_commit=%s\n' "$VCPKG_COMMIT"
  printf 'source_archive_sha512=%s\n' "$actual_sha512"
  printf 'source_archive_sha256=%s\n' "$actual_sha256"
  printf 'source_archive_size=%s\n' "$archive_size"
  printf 'source_root=miniz-%s\n' "$VERSION"
  printf 'amalgamated_archive_sha256=%s\n' "$amalgamated_sha256"
  printf 'amalgamated_archive_size=%s\n' "$amalgamated_size"
  printf 'amalgamated_inventory_sha256=%s\n' "$amalgamated_inventory_sha256"
  printf 'network_access_performed=true\n'
} >"$MANIFEST"

printf 'DUSKLIGHT_DESKTOP_MINIZ_READY root=%s sha512=%s sha256=%s size=%s amalgamated_sha256=%s\n' \
  "${INSTALL_DIR#"$PROJECT_ROOT"/}" "$actual_sha512" "$actual_sha256" \
  "$archive_size" "$amalgamated_sha256"
