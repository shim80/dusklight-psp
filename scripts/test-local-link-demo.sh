#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] ||
  die "DUSKLIGHT_GAME_IMAGE est requis"
"$SCRIPT_DIR/verify-link-loader-sources.sh"
"$SCRIPT_DIR/test-link-loader-probe.sh"
"$SCRIPT_DIR/build-link-demo-assets.sh"
psp-cmake -S "$PROJECT_ROOT/test/link-demo/psp" \
  -B "$PROJECT_ROOT/build/psp/link-demo" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDUSKLIGHT_BUILD_COMMIT="$(git rev-parse HEAD)" \
  -DDUSKLIGHT_DPMD_SHA256=b15eb6a5a8e077a888462eb7574282ce8cf730ff5baea7a747863edc76e18fd8
cmake --build "$PROJECT_ROOT/build/psp/link-demo"
"$PROJECT_ROOT/test/link-demo/verify-binary.sh"
"$SCRIPT_DIR/test-psp-bridge-host.sh"
printf '%s\n' "LINK_DEMO_LOCAL_TESTS_OK"
