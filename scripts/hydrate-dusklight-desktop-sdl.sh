#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

RELEASE=release-3.4.10
EXPECTED_SHA=12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785
CACHE_DIR="$(assert_project_path ".tools/reference/cache/sdl/$RELEASE")"
ARCHIVE="$CACHE_DIR/SDL3-3.4.10.tar.gz"
INSTALL_ROOT="$(assert_project_path ".tools/reference/dusklight-desktop/dependencies")"
INSTALL_DIR="$INSTALL_ROOT/SDL3-3.4.10"

[ -f "$ARCHIVE" ] || die "asset SDL autorisé absent : ${ARCHIVE#"$PROJECT_ROOT"/}"
actual_sha="$(sha256_file "$ARCHIVE")"
[ "$actual_sha" = "$EXPECTED_SHA" ] ||
  die "SHA-256 SDL invalide : $actual_sha"

archive_size="$(wc -c <"$ARCHIVE" | tr -d '[:space:]')"
[ "$archive_size" -ge 14000000 ] && [ "$archive_size" -le 18000000 ] ||
  die "taille SDL hors plage raisonnable : $archive_size"

entries="$(tar -tzf "$ARCHIVE")"
[ -n "$entries" ] || die "archive SDL vide"
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
  die "chemin absolu ou composant .. détecté dans SDL"
fi
if tar -tvzf "$ARCHIVE" | awk 'substr($1, 1, 1) == "l" || substr($1, 1, 1) == "h" { found = 1 } END { exit found ? 0 : 1 }'; then
  die "lien symbolique ou lien dur refusé dans SDL"
fi
printf '%s\n' "$entries" | grep -Fx 'SDL3-3.4.10/CMakeLists.txt' >/dev/null ||
  die "CMakeLists SDL absent"
printf '%s\n' "$entries" | grep -Fx 'SDL3-3.4.10/include/SDL3/SDL.h' >/dev/null ||
  die "header SDL3 absent"

safe_mkdir ".tools/reference/dusklight-desktop/dependencies"
if [ -e "$INSTALL_DIR" ]; then
  [ -d "$INSTALL_DIR" ] || die "destination SDL invalide"
  [ -f "$INSTALL_DIR/CMakeLists.txt" ] ||
    die "destination SDL existante incomplète"
else
  tar -xzf "$ARCHIVE" -C "$INSTALL_ROOT"
fi

[ -f "$INSTALL_DIR/CMakeLists.txt" ] || die "SDL absent après extraction"
[ -f "$INSTALL_DIR/include/SDL3/SDL.h" ] ||
  die "headers SDL absents après extraction"

MANIFEST="$INSTALL_DIR/HYDRATION.MANIFEST"
{
  printf 'format=DUSKLIGHT_DESKTOP_SDL_HYDRATION_V1\n'
  printf 'release=%s\n' "$RELEASE"
  printf 'asset=SDL3-3.4.10.tar.gz\n'
  printf 'asset_sha256=%s\n' "$actual_sha"
  printf 'asset_size=%s\n' "$archive_size"
  printf 'source_root=SDL3-3.4.10\n'
  printf 'network_access_performed=true\n'
} >"$MANIFEST"

printf 'DUSKLIGHT_DESKTOP_SDL_READY root=%s sha256=%s size=%s\n' \
  "${INSTALL_DIR#"$PROJECT_ROOT"/}" "$actual_sha" "$archive_size"
