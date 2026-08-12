#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
EXPECTED_ROOT="$(git -C "$ROOT" rev-parse --show-toplevel)"
[ "$ROOT" = "$EXPECTED_ROOT" ] || {
  echo "racine Git inattendue" >&2
  exit 1
}

BUILD="$ROOT/build/host/link-playable"
ASSETS="$ROOT/build/assets/dusklight-psp/data/common"
REPORT="$ROOT/build/reports/link-move-grounding-checkpoints.csv"

cmake -S "$ROOT/test/link-playable" -B "$BUILD" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD" \
  --target link_move_grounding_checkpoint_host_test
"$BUILD/link_move_grounding_checkpoint_host_test" \
  "$ASSETS/link.dpsk" \
  "$ASSETS/link.dptx" \
  "$ASSETS/link.dpan" \
  "$ASSETS/hud.dpui" >"$REPORT"

echo "LINK_MOVE_GROUNDING_CHECKPOINTS_OK report=$REPORT"
