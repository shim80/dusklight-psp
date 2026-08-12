#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

CHECKPOINT="$PROJECT_ROOT/.test-data/ppsspp/checkpoints/environment"
MARKER="$CHECKPOINT/ENVIRONMENT_RENDER.OK"
METRICS="$CHECKPOINT/ENVIRONMENT_RENDER.METRICS"
[ -f "$MARKER" ] && [ -f "$METRICS" ] ||
  die "checkpoint environnement absent"
[ "$(cat "$MARKER")" = DUSKLIGHT_PSP_ENVIRONMENT_RENDER_OK ] ||
  die "marker environnement invalide"
grep -qx 'environment_source_derived=true' "$METRICS"
grep -qx 'fog_enabled=true' "$METRICS"
grep -qx 'allocations_during_playing=0' "$METRICS"
grep -qx 'non_finite_values=0' "$METRICS"
grep -qx 'error_code=0' "$METRICS"

printf '%s\n' \
  "PSP_ENVIRONMENT_CHECKPOINT_OK source_derived=true fog=true"
