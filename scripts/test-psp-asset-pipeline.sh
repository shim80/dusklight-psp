#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

HOST_CXX="${CXX:-c++}"
command -v "$HOST_CXX" >/dev/null 2>&1 ||
  die "compilateur C++ hôte absent : $HOST_CXX"

safe_mkdir build/host/fixture-generator
safe_mkdir build/host/psp-asset-converter
safe_mkdir build/host/asset-smoke
safe_mkdir build/assets/psp-static-mesh

"$HOST_CXX" -std=c++20 -Wall -Wextra -Werror \
  "$PROJECT_ROOT/test/assets/psp-static-mesh/generate_fixture_texture.cpp" \
  -o "$PROJECT_ROOT/build/host/fixture-generator/generate-fixture-texture"
"$PROJECT_ROOT/build/host/fixture-generator/generate-fixture-texture" \
  "$PROJECT_ROOT/build/assets/psp-static-mesh/generated-fixture.ppm"
cmp -- \
  "$PROJECT_ROOT/test/assets/psp-static-mesh/fixture.ppm" \
  "$PROJECT_ROOT/build/assets/psp-static-mesh/generated-fixture.ppm"

cmake -S "$PROJECT_ROOT/tools/psp-asset-converter" \
  -B "$PROJECT_ROOT/build/host/psp-asset-converter" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$PROJECT_ROOT/build/host/psp-asset-converter"

CONVERTER="$PROJECT_ROOT/build/host/psp-asset-converter/dusk_psp_assetc"
OBJ="$PROJECT_ROOT/test/assets/psp-static-mesh/fixture.obj"
PPM="$PROJECT_ROOT/test/assets/psp-static-mesh/fixture.ppm"
OUTPUT_A="$PROJECT_ROOT/build/assets/psp-static-mesh/fixture-a.dpsm"
OUTPUT_B="$PROJECT_ROOT/build/assets/psp-static-mesh/fixture-b.dpsm"
"$CONVERTER" --obj "$OBJ" --texture "$PPM" \
  --output "$OUTPUT_A" \
  --report "$PROJECT_ROOT/build/assets/psp-static-mesh/fixture-a.txt"
"$CONVERTER" --obj "$OBJ" --texture "$PPM" \
  --output "$OUTPUT_B" \
  --report "$PROJECT_ROOT/build/assets/psp-static-mesh/fixture-b.txt"
cmp -- "$OUTPUT_A" "$OUTPUT_B"

"$HOST_CXX" \
  -std=c++20 -Wall -Wextra -Werror \
  -I"$PROJECT_ROOT/dusklight-main/platforms/psp/include" \
  "$PROJECT_ROOT/test/asset-smoke/host_package_test.cpp" \
  "$PROJECT_ROOT/dusklight-main/platforms/psp/src/dpsm.cpp" \
  -o "$PROJECT_ROOT/build/host/asset-smoke/package-test"
"$PROJECT_ROOT/build/host/asset-smoke/package-test" "$OUTPUT_A"

"$HOST_CXX" \
  -std=c++20 -Wall -Wextra -Werror \
  -I"$PROJECT_ROOT/dusklight-main/platforms/psp/include" \
  "$PROJECT_ROOT/test/asset-smoke/host_loader_test.cpp" \
  "$PROJECT_ROOT/dusklight-main/platforms/psp/src/dpsm.cpp" \
  "$PROJECT_ROOT/dusklight-main/platforms/psp/src/dpsm_loader.cpp" \
  -o "$PROJECT_ROOT/build/host/asset-smoke/loader-test"
"$PROJECT_ROOT/build/host/asset-smoke/loader-test" \
  "$OUTPUT_A" \
  "$PROJECT_ROOT/build/assets/psp-static-mesh/absent.dpsm"

"$HOST_CXX" \
  -std=c++20 -Wall -Wextra -Werror \
  "$PROJECT_ROOT/test/asset-smoke/host_pixel_reference_test.cpp" \
  -o "$PROJECT_ROOT/build/host/asset-smoke/pixel-reference-test"
"$PROJECT_ROOT/build/host/asset-smoke/pixel-reference-test"

printf '%s\n' \
  "[OK] Fixture, conversion déterministe, mutations et chargeur DPSM validés."
