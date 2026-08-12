#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

TIMEOUT_SECONDS=30
while [ "$#" -gt 0 ]; do
  case "$1" in
    --timeout)
      shift
      [ "$#" -gt 0 ] || die "--timeout exige un nombre de secondes"
      TIMEOUT_SECONDS="$1"
      ;;
    -h|--help)
      printf '%s\n' "Usage: test-psp-smokes.sh [--timeout secondes]"
      exit 0
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]] ||
  die "timeout invalide : $TIMEOUT_SECONDS"

safe_mkdir logs/ppsspp
RUN_ID="$(timestamp_utc)"
AUTOMATION_LOG="$(assert_project_path "logs/ppsspp/automated-smokes-$RUN_ID.log")"
exec > >(tee "$AUTOMATION_LOG") 2>&1

printf '%s\n' "=== Test hôte des conteneurs Dusklight ==="
HOST_CXX="${CXX:-c++}"
command -v "$HOST_CXX" >/dev/null 2>&1 ||
  die "compilateur C++ hôte absent : $HOST_CXX"
safe_mkdir build/host/core-smoke
"$HOST_CXX" \
  -std=c++20 -Wall -Wextra -Werror \
  -I"$PROJECT_ROOT/dusklight-main/include" \
  -I"$PROJECT_ROOT/dusklight-main/libs/dolphin/include/dolphin" \
  "$PROJECT_ROOT/test/core-smoke/host_containers_test.cpp" \
  "$PROJECT_ROOT/dusklight-main/src/SSystem/SComponent/c_node.cpp" \
  "$PROJECT_ROOT/dusklight-main/src/SSystem/SComponent/c_list.cpp" \
  "$PROJECT_ROOT/dusklight-main/src/SSystem/SComponent/c_tree.cpp" \
  -o "$PROJECT_ROOT/build/host/core-smoke/containers-test"
"$PROJECT_ROOT/build/host/core-smoke/containers-test"
printf '%s\n' "[OK] Listes et arbres Dusklight validés sur l'hôte."

printf '%s\n' "=== Test hôte de la disposition GU ==="
safe_mkdir build/host/gu-smoke
"$HOST_CXX" \
  -std=c++20 -Wall -Wextra -Werror \
  -I"$PROJECT_ROOT/dusklight-main/platforms/psp/include" \
  "$PROJECT_ROOT/test/gu-smoke/host_layout_test.cpp" \
  -o "$PROJECT_ROOT/build/host/gu-smoke/layout-test"
"$PROJECT_ROOT/build/host/gu-smoke/layout-test"
printf '%s\n' "[OK] Disposition VRAM GU validée sur l'hôte."

printf '%s\n' "=== Test hôte de la géométrie 3D ==="
safe_mkdir build/host/3d-smoke
"$HOST_CXX" \
  -std=c++20 -Wall -Wextra -Werror \
  -I"$PROJECT_ROOT/dusklight-main/platforms/psp/include" \
  -I"$PROJECT_ROOT/test/3d-smoke" \
  "$PROJECT_ROOT/test/3d-smoke/host_geometry_test.cpp" \
  "$PROJECT_ROOT/test/3d-smoke/scene.cpp" \
  -o "$PROJECT_ROOT/build/host/3d-smoke/geometry-test"
"$PROJECT_ROOT/build/host/3d-smoke/geometry-test"
printf '%s\n' \
  "[OK] Maillage, matrices, projections et marges 3D validés sur l'hôte."

printf '%s\n' "=== Pipeline DPSM et convertisseur ==="
"$PROJECT_ROOT/scripts/test-psp-asset-pipeline.sh"

printf '%s\n' "=== Couture légère dMdl_obj_c ==="
"$PROJECT_ROOT/scripts/test-dmdl-object-seam.sh"

printf '%s\n' "=== Tests hôte du bridge dMdl_obj_c ==="
"$PROJECT_ROOT/scripts/test-psp-bridge-host.sh"

printf '%s\n' "=== Smoke PSP historique ==="
psp-cmake -S "$PROJECT_ROOT/test/smoke" \
  -B "$PROJECT_ROOT/build/psp/smoke" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/psp/smoke"
"$PROJECT_ROOT/test/smoke/verify-binary.sh"
"$PROJECT_ROOT/scripts/run-ppsspp-smoke.sh" \
  --run --timeout "$TIMEOUT_SECONDS"

printf '%s\n' "=== Core smoke Dusklight PSP ==="
psp-cmake -S "$PROJECT_ROOT/test/core-smoke" \
  -B "$PROJECT_ROOT/build/psp/core-smoke" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/psp/core-smoke"
"$PROJECT_ROOT/test/core-smoke/verify-binary.sh"
"$PROJECT_ROOT/scripts/run-ppsspp-core-smoke.sh" \
  --run --timeout "$TIMEOUT_SECONDS"

printf '%s\n' "=== GU smoke Dusklight PSP ==="
psp-cmake -S "$PROJECT_ROOT/test/gu-smoke" \
  -B "$PROJECT_ROOT/build/psp/gu-smoke" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/psp/gu-smoke"
"$PROJECT_ROOT/test/gu-smoke/verify-binary.sh"
"$PROJECT_ROOT/scripts/run-ppsspp-gu-smoke.sh" \
  --run --timeout "$TIMEOUT_SECONDS"

printf '%s\n' "=== Smoke 3D Dusklight PSP ==="
psp-cmake -S "$PROJECT_ROOT/test/3d-smoke" \
  -B "$PROJECT_ROOT/build/psp/3d-smoke" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/psp/3d-smoke"
"$PROJECT_ROOT/test/3d-smoke/verify-binary.sh"
"$PROJECT_ROOT/scripts/run-ppsspp-3d-smoke.sh" \
  --run --timeout "$TIMEOUT_SECONDS"

printf '%s\n' "=== Asset smoke Dusklight PSP ==="
psp-cmake -S "$PROJECT_ROOT/test/asset-smoke" \
  -B "$PROJECT_ROOT/build/psp/asset-smoke" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/psp/asset-smoke"
"$PROJECT_ROOT/test/asset-smoke/verify-binary.sh"
"$PROJECT_ROOT/scripts/run-ppsspp-asset-smoke.sh" \
  --run --timeout "$TIMEOUT_SECONDS"

printf '%s\n' "=== Bridge smoke Dusklight PSP ==="
psp-cmake -S "$PROJECT_ROOT/test/bridge-smoke" \
  -B "$PROJECT_ROOT/build/psp/bridge-smoke" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/psp/bridge-smoke"
"$PROJECT_ROOT/test/bridge-smoke/verify-binary.sh"
"$PROJECT_ROOT/scripts/run-ppsspp-bridge-smoke.sh" \
  --run --timeout "$TIMEOUT_SECONDS"

printf '%s\n' "=== Résultat global : SUCCES ==="
printf 'Journal automatisé : %s\n' "${AUTOMATION_LOG#"$PROJECT_ROOT"/}"
