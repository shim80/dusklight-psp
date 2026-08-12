#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

require_project_root
[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] ||
  die "DUSKLIGHT_GAME_IMAGE est requis"
[ -f "$DUSKLIGHT_GAME_IMAGE" ] || die "image locale absente"

OUTPUT_DIR="$(assert_project_path "build/assets/link-demo")"
FIRST="$OUTPUT_DIR/link.dpmd"
SECOND="$OUTPUT_DIR/link-repeat.dpmd"
MANIFEST="$OUTPUT_DIR/link-manifest.json"
PREVIEW="$OUTPUT_DIR/link-preview.obj"
HAND_PREVIEW="$OUTPUT_DIR/link-hands-all.obj"
PROBE="$(assert_project_path "build/host/link-loader/probe/dusk_link_loader_probe")"
HOST_BUILD="$(assert_project_path "build/host/link-demo")"

safe_mkdir build/assets/link-demo
"$SCRIPT_DIR/build-link-loader-probe.sh"

generate() {
  local package="$1"
  env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_DPMD_OUTPUT="$package" \
    DUSKLIGHT_DPMD_MANIFEST="$MANIFEST" \
    DUSKLIGHT_DPMD_PREVIEW="$PREVIEW" \
    DUSKLIGHT_HAND_PREVIEW="$HAND_PREVIEW" \
    "$PROBE"
}

generate "$FIRST" >"$OUTPUT_DIR/conversion-first.log"
generate "$SECOND" >"$OUTPUT_DIR/conversion-second.log"
cmp -s "$FIRST" "$SECOND" || die "les deux conversions DPMD diffèrent"

cmake -S "$PROJECT_ROOT/test/link-demo" -B "$HOST_BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$HOST_BUILD"
"$HOST_BUILD/link_dpmd_host_test" "$FIRST" "$OUTPUT_DIR"
"$HOST_BUILD/link_benchmark_stats_host_test"
"$HOST_BUILD/link_benchmark_metrics_host_test"

printf '%s\n' \
  "LINK_DEMO_ASSETS_OK package=build/assets/link-demo/link.dpmd deterministic=true"
