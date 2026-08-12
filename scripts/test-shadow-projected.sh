#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

cmake -S "$PROJECT_ROOT/test/room-transition" \
  -B "$PROJECT_ROOT/build/host/room-transition" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/host/room-transition" \
  --target shadow_projected_budget_host_test
"$PROJECT_ROOT/build/host/room-transition/shadow_projected_budget_host_test"

printf '%s\n' \
  "HOST_SHADOW_PROJECTED_OK selected=64x64 total_edram_bytes=16384"
