#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

require_project_root
BUILD="$(assert_project_path build/host/canonical-runtime)"
cmake -S "$PROJECT_ROOT/test/canonical-runtime" -B "$BUILD" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD" --target depth_behavior_contract_host_test
"$BUILD/depth_behavior_contract_host_test"
