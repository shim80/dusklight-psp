#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

usage() {
  printf '%s\n' \
    "Usage: $0 --variant <variant> [--configure-only] [--fresh]" \
    "Variants:" \
    "  vanilla-relwithdebinfo" \
    "  vanilla-debug" \
    "  vanilla-debug-asan" \
    "  trace-relwithdebinfo" \
    "  trace-debug"
}

variant=
configure_only=false
fresh=false
while [ "$#" -gt 0 ]; do
  case "$1" in
    --variant)
      [ "$#" -ge 2 ] || die "--variant exige une valeur"
      variant="$2"
      shift 2
      ;;
    --configure-only)
      configure_only=true
      shift
      ;;
    --fresh)
      fresh=true
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "argument inconnu : $1"
      ;;
  esac
done
[ -n "$variant" ] || {
  usage >&2
  exit 2
}

case "$variant" in
  vanilla-relwithdebinfo)
    source_name=source-vanilla
    preset=macos-default-relwithdebinfo
    trace=OFF
    ;;
  vanilla-debug)
    source_name=source-vanilla
    preset=macos-default-debug
    trace=OFF
    ;;
  vanilla-debug-asan)
    source_name=source-vanilla
    preset=macos-default-debug-asan
    trace=OFF
    ;;
  trace-relwithdebinfo)
    source_name=source-trace
    preset=macos-default-relwithdebinfo
    trace=ON
    ;;
  trace-debug)
    source_name=source-trace
    preset=macos-default-debug
    trace=ON
    ;;
  *)
    die "variante invalide : $variant"
    ;;
esac

REFERENCE_ROOT="$(assert_project_path ".tools/reference/dusklight-desktop")"
SOURCE="$(assert_project_path "$REFERENCE_ROOT/$source_name")"
NOD="$(assert_project_path "$REFERENCE_ROOT/worktrees/nod-dc18d2ff129f05228b8510ea092d8b24c290a49a")"
NOD_VENDOR_BASE="$(assert_project_path ".tools/sources/nod/vendor/dc18d2ff129f05228b8510ea092d8b24c290a49a")"
CARGO_HOME_DIR="$(assert_project_path "$NOD_VENDOR_BASE/.cargo")"
DAWN_ARCHIVE="$(assert_project_path ".tools/reference/cache/dawn/v20260618.032059/dawn-darwin-arm64.tar.gz")"
DAWN_DEFAULT="$(assert_project_path "$REFERENCE_ROOT/dependencies/dawn-v20260618.032059-darwin-arm64")"
SDL_DEFAULT="$(assert_project_path "$REFERENCE_ROOT/dependencies/SDL3-3.4.10")"
MINIZ_DEFAULT="$(assert_project_path "$REFERENCE_ROOT/dependencies/miniz-3.0.2-amalgamated")"
XXHASH_DEFAULT="$(assert_project_path "$REFERENCE_ROOT/dependencies/xxhash-v0.8.3")"
IMGUI_DEFAULT="$(assert_project_path "$REFERENCE_ROOT/dependencies/imgui-v1.91.9b-docking")"
RMLUI_DEFAULT="$(assert_project_path "$REFERENCE_ROOT/dependencies/rmlui-f9b8c9e")"
TRACY_DEFAULT="$(assert_project_path "$REFERENCE_ROOT/dependencies/tracy-6789e7d")"
CXXOPTS_DEFAULT="$(assert_project_path "$REFERENCE_ROOT/dependencies/cxxopts-v3.3.1")"
JSON_DEFAULT="$(assert_project_path "$REFERENCE_ROOT/dependencies/json-v3.12.0")"
CORROSION_DEFAULT="$(assert_project_path "$REFERENCE_ROOT/dependencies/corrosion-v0.6.1")"
GOOGLETEST_DEFAULT="$(assert_project_path "$REFERENCE_ROOT/dependencies/googletest-v1.17.0")"
SYMGEN_DEFAULT="$(assert_project_path ".tools/reference/cache/symgen/v1.2.3/symgen-macos-arm64")"
BUILD="$(assert_project_path "build/reference-desktop/$variant")"
LOG_ROOT="$(assert_project_path ".test-data/dusklight-reference/logs")"
LOG="$LOG_ROOT/configure-$variant.log"
EXPECTED_DUSKLIGHT=1bae8a5e6a812217ca33ba533e707ecfa64b1553
EXPECTED_NOD=dc18d2ff129f05228b8510ea092d8b24c290a49a

[ -d "$SOURCE" ] || die "source absente : $source_name"
[ -d "$NOD" ] || die "source Nod locale absente"
[ "$(git -C "$SOURCE" rev-parse HEAD)" = "$EXPECTED_DUSKLIGHT" ] ||
  die "révision Dusklight inattendue"
[ "$(git -C "$NOD" rev-parse HEAD)" = "$EXPECTED_NOD" ] ||
  die "révision Nod inattendue"
"$SCRIPT_DIR/verify-link-loader-sources.sh" >/dev/null
[ -f "$CARGO_HOME_DIR/config.toml" ] ||
  die "configuration Cargo vendor locale absente"

safe_mkdir "build/reference-desktop/$variant"
safe_mkdir ".test-data/dusklight-reference/logs"

prefix_path="${DUSKLIGHT_REFERENCE_PREFIX_PATH:-}"
if [ -z "$prefix_path" ] && command -v brew >/dev/null 2>&1; then
  prefix_path="$(brew --prefix 2>/dev/null || true)"
fi
compiler_root="${DUSKLIGHT_REFERENCE_LLVM_ROOT:-}"
if [ -z "$compiler_root" ] && command -v brew >/dev/null 2>&1; then
  compiler_root="$(brew --prefix llvm 2>/dev/null || true)"
fi
[ -n "$compiler_root" ] ||
  die "LLVM C++20 compatible absent (DUSKLIGHT_REFERENCE_LLVM_ROOT)"
[ -x "$compiler_root/bin/clang" ] && [ -x "$compiler_root/bin/clang++" ] ||
  die "compilateurs LLVM locaux absents sous : $compiler_root"
c_compiler="$SCRIPT_DIR/reference-clang-macos.sh"
cxx_compiler="$SCRIPT_DIR/reference-clangxx-macos.sh"
[ -x "$c_compiler" ] && [ -x "$cxx_compiler" ] ||
  die "wrappers LLVM macOS du dépôt absents ou non exécutables"
dawn_dir="${DUSKLIGHT_REFERENCE_DAWN_DIR:-$DAWN_DEFAULT}"
if [ -n "$dawn_dir" ]; then
  dawn_dir="$(assert_project_path "$dawn_dir")"
  [ -d "$dawn_dir" ] || die "arbre Dawn local absent : $dawn_dir"
  find "$dawn_dir" -type f -name DawnConfig.cmake -print -quit |
    grep -q . || die "DawnConfig.cmake absent de l'arbre Dawn local"
fi
sdl_dir="${DUSKLIGHT_REFERENCE_SDL_DIR:-$SDL_DEFAULT}"
sdl_dir="$(assert_project_path "$sdl_dir")"
[ -f "$sdl_dir/CMakeLists.txt" ] || die "source SDL locale absente"
[ -f "$sdl_dir/include/SDL3/SDL.h" ] || die "headers SDL locaux absents"
miniz_dir="${DUSKLIGHT_REFERENCE_MINIZ_DIR:-$MINIZ_DEFAULT}"
miniz_dir="$(assert_project_path "$miniz_dir")"
[ -f "$miniz_dir/miniz.c" ] && [ -f "$miniz_dir/miniz.h" ] ||
  die "sources miniz amalgamées locales incomplètes"
for local_source in \
  "$XXHASH_DEFAULT/cmake_unofficial/CMakeLists.txt" \
  "$IMGUI_DEFAULT/imgui.cpp" \
  "$RMLUI_DEFAULT/CMakeLists.txt" \
  "$TRACY_DEFAULT/CMakeLists.txt" \
  "$CXXOPTS_DEFAULT/CMakeLists.txt" \
  "$JSON_DEFAULT/CMakeLists.txt" \
  "$CORROSION_DEFAULT/CMakeLists.txt" \
  "$GOOGLETEST_DEFAULT/CMakeLists.txt"; do
  [ -f "$local_source" ] || die "source FetchContent locale absente : $local_source"
done
[ -x "$SYMGEN_DEFAULT" ] || die "symgen local vérifié absent ou non exécutable"

cmake_args=(
  --preset "$preset"
  -S "$SOURCE"
  -B "$BUILD"
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
  -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
  -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON
  -DCMAKE_DISABLE_SOURCE_CHANGES=ON
  "-DCMAKE_C_COMPILER=$c_compiler"
  "-DCMAKE_CXX_COMPILER=$cxx_compiler"
  "-DCMAKE_OBJC_COMPILER=$c_compiler"
  "-DCMAKE_OBJCXX_COMPILER=$cxx_compiler"
  "-DCMAKE_CXX_FLAGS=-include TargetConditionals.h"
  '-DCMAKE_EXE_LINKER_FLAGS=-Wl,-U,_OBJC_CLASS_\$_MTLLogStateDescriptor'
  -DDUSK_ENABLE_UPDATE_CHECKER=OFF
  -DDUSK_ENABLE_SENTRY_NATIVE=OFF
  -DDUSK_ENABLE_CODE_MODS=OFF
  -DDUSKLIGHT_REFERENCE_TRACE="$trace"
  -DAURORA_DAWN_PROVIDER=package
  "-DAURORA_DAWN_PACKAGE_URL=file://$DAWN_ARCHIVE"
  -DAURORA_SDL3_PROVIDER=vendor
  "-DFETCHCONTENT_SOURCE_DIR_SDL=$sdl_dir"
  -DAURORA_NOD_PROVIDER=vendor
  "-DFETCHCONTENT_SOURCE_DIR_AURORA_NOD=$NOD"
  "-DFETCHCONTENT_SOURCE_DIR_MINIZ=$miniz_dir"
  "-DFETCHCONTENT_SOURCE_DIR_XXHASH=$XXHASH_DEFAULT"
  "-DFETCHCONTENT_SOURCE_DIR_IMGUI=$IMGUI_DEFAULT"
  "-DFETCHCONTENT_SOURCE_DIR_RMLUI=$RMLUI_DEFAULT"
  "-DFETCHCONTENT_SOURCE_DIR_TRACY=$TRACY_DEFAULT"
  "-DFETCHCONTENT_SOURCE_DIR_CXXOPTS=$CXXOPTS_DEFAULT"
  "-DFETCHCONTENT_SOURCE_DIR_JSON=$JSON_DEFAULT"
  "-DFETCHCONTENT_SOURCE_DIR_CORROSION=$CORROSION_DEFAULT"
  "-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$GOOGLETEST_DEFAULT"
  "-DSYMGEN_PATH=$SYMGEN_DEFAULT"
)
if [ "$fresh" = true ]; then
  cmake_args=(--fresh "${cmake_args[@]}")
fi
if [ -n "$prefix_path" ]; then
  cmake_args+=("-DCMAKE_PREFIX_PATH=$prefix_path")
fi
if [ -n "$dawn_dir" ]; then
  cmake_args+=("-DFETCHCONTENT_SOURCE_DIR_DAWN_PREBUILT=$dawn_dir")
fi

set +e
{
  printf 'variant=%s\n' "$variant"
  printf 'source=%s\n' "${SOURCE#"$PROJECT_ROOT"/}"
  printf 'preset=%s\n' "$preset"
  printf 'offline=true\n'
  printf 'prefix_path=%s\n' "$prefix_path"
  printf 'c_compiler=%s\n' "$("$c_compiler" --version | head -n 1)"
  printf 'cxx_compiler=%s\n' "$("$cxx_compiler" --version | head -n 1)"
  env \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    HTTP_PROXY=http://127.0.0.1:9 \
    HTTPS_PROXY=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    CARGO_HOME="$CARGO_HOME_DIR" \
    CARGO_NET_OFFLINE=true \
    cmake "${cmake_args[@]}"
} 2>&1 | tee "$LOG"
configure_status="${PIPESTATUS[0]}"
set -e
[ "$configure_status" -eq 0 ] ||
  die "configuration hors ligne échouée (voir ${LOG#"$PROJECT_ROOT"/})"

if grep -Eiq \
  '(Performing download step|Downloading[[:space:]]|Cloning into|git clone|Connecting to|Could not resolve host)' \
  "$LOG"; then
  die "tentative réseau détectée dans le journal de configuration"
fi

if [ "$configure_only" = true ]; then
  printf 'DUSKLIGHT_DESKTOP_REFERENCE_CONFIGURE_OK variant=%s\n' "$variant"
  exit 0
fi

BUILD_LOG="$LOG_ROOT/build-$variant.log"
set +e
env \
  http_proxy=http://127.0.0.1:9 \
  https_proxy=http://127.0.0.1:9 \
  HTTP_PROXY=http://127.0.0.1:9 \
  HTTPS_PROXY=http://127.0.0.1:9 \
  ALL_PROXY=http://127.0.0.1:9 \
  NO_PROXY= \
  CARGO_HOME="$CARGO_HOME_DIR" \
  CARGO_NET_OFFLINE=true \
  cmake --build "$BUILD" --config \
    "$([ "$variant" = vanilla-relwithdebinfo ] || [ "$variant" = trace-relwithdebinfo ] && echo RelWithDebInfo || echo Debug)" \
  2>&1 | tee "$BUILD_LOG"
build_status="${PIPESTATUS[0]}"
set -e
[ "$build_status" -eq 0 ] ||
  die "build échoué (voir ${BUILD_LOG#"$PROJECT_ROOT"/})"
if grep -Eiq \
  '(Updating crates.io index|spurious network error|Could not resolve host|Connecting to)' \
  "$BUILD_LOG"; then
  die "tentative réseau détectée pendant le build"
fi

printf 'DUSKLIGHT_DESKTOP_REFERENCE_BUILD_OK variant=%s\n' "$variant"
