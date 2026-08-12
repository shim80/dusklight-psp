#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

require_project_root
"$SCRIPT_DIR/verify-link-loader-sources.sh"

NOD_COMMIT="dc18d2ff129f05228b8510ea092d8b24c290a49a"
NOD_ROOT="$PROJECT_ROOT/.tools/sources/nod/$NOD_COMMIT"
VENDOR_ROOT="$PROJECT_ROOT/.tools/sources/nod/vendor/$NOD_COMMIT/vendor"
CARGO_HOME_DIR="$(assert_project_path ".tmp/cargo-home")"
NOD_BUILD_DIR="$(assert_project_path "build/host/link-loader/nod")"
PROBE_BUILD_DIR="$(assert_project_path "build/host/link-loader/probe")"

safe_mkdir "$CARGO_HOME_DIR"
safe_mkdir "$NOD_BUILD_DIR"
safe_mkdir "$PROBE_BUILD_DIR"

env \
  CARGO_HOME="$CARGO_HOME_DIR" \
  CARGO_TARGET_DIR="$NOD_BUILD_DIR" \
  cargo build \
    --manifest-path "$NOD_ROOT/Cargo.toml" \
    --package nod-ffi \
    --release \
    --locked \
    --offline \
    --no-default-features \
    --config "source.crates-io.replace-with='vendored-sources'" \
    --config "source.vendored-sources.directory='$VENDOR_ROOT'"

CC="${CC:-/opt/homebrew/opt/llvm/bin/clang}"
CXX="${CXX:-/opt/homebrew/opt/llvm/bin/clang++}"
[ -x "$CC" ] || die "compilateur C hôte absent : $CC"
[ -x "$CXX" ] || die "compilateur C++ hôte absent : $CXX"

cmake \
  -S "$PROJECT_ROOT/tools/dusk_link_loader_probe" \
  -B "$PROBE_BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_CXX_COMPILER="$CXX"
cmake --build "$PROBE_BUILD_DIR" --parallel
printf '%s\n' "LINK_LOADER_PROBE_BUILD_OK"
