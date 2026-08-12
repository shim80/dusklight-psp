#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

TARGET=all
while [ "$#" -gt 0 ]; do
  case "$1" in
    --target)
      shift
      [ "$#" -gt 0 ] || die "--target exige une valeur"
      TARGET="$1"
      ;;
    --no-deps) ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done

case "$TARGET" in
  coordinate|pivot|forward|locomotion|all) ;;
  *) die "cible Link inconnue : $TARGET" ;;
esac

BUILD_DIR="$(assert_project_path "build/host/canonical-runtime")"
cmake -S "$PROJECT_ROOT/test/canonical-runtime" \
  -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD_DIR" --target link_fidelity_host_test

"$BUILD_DIR/link_fidelity_host_test" --target "$TARGET" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dprm" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dptx" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dpcl" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dpsc"
