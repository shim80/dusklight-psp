#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

download=false
dependencies=()
while [ "$#" -gt 0 ]; do
  case "$1" in
    --download)
      download=true
      shift
      ;;
    --dependency)
      [ "$#" -ge 2 ] || die "--dependency exige une valeur"
      dependencies+=("$2")
      shift 2
      ;;
    *)
      die "argument inconnu : $1"
      ;;
  esac
done
[ "${#dependencies[@]}" -gt 0 ] || die "au moins un --dependency est requis"

hydrate_one() {
  local id="$1" url sha archive_rel target_rel root_hint
  local required=()
  case "$id" in
    xxhash)
      url=https://github.com/Cyan4973/xxHash/archive/refs/tags/v0.8.3.tar.gz
      sha=aae608dfe8213dfd05d909a57718ef82f30722c392344583d3f39050c7f29a80
      archive_rel=.tools/reference/cache/xxhash/v0.8.3/xxHash-v0.8.3.tar.gz
      target_rel=.tools/reference/dusklight-desktop/dependencies/xxhash-v0.8.3
      root_hint=xxHash-0.8.3
      required=(cmake_unofficial/CMakeLists.txt xxhash.c xxhash.h LICENSE)
      ;;
    imgui)
      url=https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b-docking.tar.gz
      sha=466fdef9b18de15f0bb6e288e3d00ffa3d82200ec458ce5e4f724a161d9528a5
      archive_rel=.tools/reference/cache/imgui/v1.91.9b-docking/imgui-v1.91.9b-docking.tar.gz
      target_rel=.tools/reference/dusklight-desktop/dependencies/imgui-v1.91.9b-docking
      root_hint=imgui-1.91.9b-docking
      required=(imgui.cpp imgui.h LICENSE.txt backends/imgui_impl_sdl3.cpp backends/imgui_impl_wgpu.cpp)
      ;;
    rmlui)
      url=https://github.com/mikke89/RmlUi/archive/f9b8c9e2935d5df2c7dff2c190d3968e99b0c3dc.tar.gz
      sha=42b830abc2509a8c07cf02ba3313505de3a4642725654f489402022d8fefdd79
      archive_rel=.tools/reference/cache/rmlui/f9b8c9e2935d5df2c7dff2c190d3968e99b0c3dc/rmlui-f9b8c9e.tar.gz
      target_rel=.tools/reference/dusklight-desktop/dependencies/rmlui-f9b8c9e
      root_hint=RmlUi-f9b8c9e2935d5df2c7dff2c190d3968e99b0c3dc
      required=(CMakeLists.txt Include/RmlUi/Core.h LICENSE.txt Backends/RmlUi_Platform_SDL.cpp)
      ;;
    tracy)
      url=https://github.com/wolfpld/tracy/archive/6789e7d6f9a65ec98926b602097a33a9676d2606.tar.gz
      sha=ebfe4fb50d7c254901979355c80a7d4cd33624aa2ec0fe90b3b238153cd5d69b
      archive_rel=.tools/reference/cache/tracy/6789e7d6f9a65ec98926b602097a33a9676d2606/tracy-6789e7d.tar.gz
      target_rel=.tools/reference/dusklight-desktop/dependencies/tracy-6789e7d
      root_hint=tracy-6789e7d6f9a65ec98926b602097a33a9676d2606
      required=(CMakeLists.txt public/TracyClient.cpp LICENSE)
      ;;
    cxxopts)
      url=https://github.com/jarro2783/cxxopts/archive/refs/tags/v3.3.1.tar.gz
      sha=3bfc70542c521d4b55a46429d808178916a579b28d048bd8c727ee76c39e2072
      archive_rel=.tools/reference/cache/cxxopts/v3.3.1/cxxopts-v3.3.1.tar.gz
      target_rel=.tools/reference/dusklight-desktop/dependencies/cxxopts-v3.3.1
      root_hint=cxxopts-3.3.1
      required=(CMakeLists.txt include/cxxopts.hpp LICENSE)
      ;;
    json)
      url=https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
      sha=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
      archive_rel=.tools/reference/cache/json/v3.12.0/json.tar.xz
      target_rel=.tools/reference/dusklight-desktop/dependencies/json-v3.12.0
      root_hint=json
      required=(CMakeLists.txt single_include/nlohmann/json.hpp LICENSE.MIT)
      ;;
    googletest)
      url=https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
      sha=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
      archive_rel=.tools/reference/cache/googletest/v1.17.0/googletest-v1.17.0.tar.gz
      target_rel=.tools/reference/dusklight-desktop/dependencies/googletest-v1.17.0
      root_hint=googletest-1.17.0
      required=(CMakeLists.txt googletest/include/gtest/gtest.h LICENSE)
      ;;
    *)
      die "dépendance FetchContent non autorisée : $id"
      ;;
  esac

  local archive target cache_dir entries actual_sha archive_size top_roots
  archive="$(assert_project_path "$archive_rel")"
  target="$(assert_project_path "$target_rel")"
  cache_dir="$(dirname "$archive")"
  safe_mkdir "${cache_dir#"$PROJECT_ROOT"/}"
  if [ ! -f "$archive" ]; then
    [ "$download" = true ] || die "archive locale absente pour $id"
    curl -fL --proto '=https' --tlsv1.2 --retry 0 -o "$archive" "$url"
  fi
  actual_sha="$(sha256_file "$archive")"
  [ "$actual_sha" = "$sha" ] || die "SHA-256 invalide pour $id : $actual_sha"
  archive_size="$(wc -c <"$archive" | tr -d '[:space:]')"
  [ "$archive_size" -ge 10000 ] && [ "$archive_size" -le 100000000 ] ||
    die "taille hors plage pour $id : $archive_size"

  entries="$(tar -tf "$archive")"
  [ -n "$entries" ] || die "archive vide pour $id"
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
    die "chemin dangereux dans l'archive $id"
  fi
  if tar -tvf "$archive" | awk 'substr($1, 1, 1) == "l" || substr($1, 1, 1) == "h" { found = 1 } END { exit found ? 0 : 1 }'; then
    die "lien refusé dans l'archive $id"
  fi
  top_roots="$(printf '%s\n' "$entries" | awk -F/ 'NF {print $1}' | sort -u)"
  [ "$top_roots" = "$root_hint" ] ||
    die "racine d'archive inattendue pour $id : $top_roots"

  if [ -e "$target" ]; then
    [ -d "$target" ] || die "destination invalide pour $id"
  else
    mkdir -- "$target"
    tar -xf "$archive" -C "$target" --strip-components 1
  fi
  for file in "${required[@]}"; do
    [ -f "$target/$file" ] || die "fichier absent pour $id : $file"
  done

  if [ "$id" = rmlui ]; then
    cmake \
      "-DRMLUI_SOURCE_DIR=$target" \
      -P "$PROJECT_ROOT/.tools/reference/dusklight-desktop/source-vanilla/extern/aurora/cmake/patches/apply-rmlui-keyboard-showed-state.cmake"
  fi

  {
    printf 'format=DUSKLIGHT_DESKTOP_FETCHCONTENT_HYDRATION_V1\n'
    printf 'dependency=%s\n' "$id"
    printf 'source_url=%s\n' "$url"
    printf 'source_sha256=%s\n' "$actual_sha"
    printf 'source_size=%s\n' "$archive_size"
    printf 'source_root=%s\n' "$root_hint"
  } >"$target/HYDRATION.MANIFEST"
  printf 'DUSKLIGHT_FETCHCONTENT_READY dependency=%s sha256=%s size=%s root=%s\n' \
    "$id" "$actual_sha" "$archive_size" "${target#"$PROJECT_ROOT"/}"
}

for dependency in "${dependencies[@]}"; do
  hydrate_one "$dependency"
done
