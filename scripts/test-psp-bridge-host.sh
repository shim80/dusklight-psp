#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

HOST_CXX="${CXX:-c++}"
command -v "$HOST_CXX" >/dev/null 2>&1 ||
  die "compilateur C++ hôte absent : $HOST_CXX"
safe_mkdir build/host/bridge-smoke

COMMON_FLAGS=(
  -std=c++20
  -Wall
  -Wextra
  -Werror
  "-I$PROJECT_ROOT/dusklight-main/include"
  "-I$PROJECT_ROOT/dusklight-main/libs/dolphin/include"
  "-I$PROJECT_ROOT/dusklight-main/platforms/psp/include"
)

"$HOST_CXX" \
  "${COMMON_FLAGS[@]}" \
  "$PROJECT_ROOT/test/bridge-smoke/host_bridge_test.cpp" \
  "$PROJECT_ROOT/dusklight-main/platforms/psp/src/static_render_bridge.cpp" \
  -o "$PROJECT_ROOT/build/host/bridge-smoke/bridge-host-test"
"$PROJECT_ROOT/build/host/bridge-smoke/bridge-host-test"

"$HOST_CXX" \
  "${COMMON_FLAGS[@]}" \
  "$PROJECT_ROOT/test/bridge-smoke/host_pixel_reference_test.cpp" \
  -o "$PROJECT_ROOT/build/host/bridge-smoke/pixel-reference-test"
"$PROJECT_ROOT/build/host/bridge-smoke/pixel-reference-test"

if rg -n '\b(new|malloc|calloc|realloc)\b' \
  "$PROJECT_ROOT/dusklight-main/platforms/psp/src/static_render_bridge.cpp" \
  "$PROJECT_ROOT/dusklight-main/platforms/psp/src/static_render_backend.cpp" \
  >/dev/null; then
  die "allocation détectée dans le chemin bridge"
fi

printf '%s\n' \
  "[OK] Adaptateur, matrices, refus, vues sans copie et pixels bridge validés sur l'hôte."
