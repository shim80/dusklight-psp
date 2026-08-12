#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

CHECKPOINT="$PROJECT_ROOT/.test-data/ppsspp/checkpoints/shadows"
MARKER="$CHECKPOINT/SHADOW_SIMPLE.OK"
METRICS="$CHECKPOINT/SHADOW_SIMPLE.METRICS"
[ -f "$MARKER" ] && [ -f "$METRICS" ] ||
  die "checkpoint ombre simple absent"
[ "$(cat "$MARKER")" = DUSKLIGHT_PSP_SIMPLE_SHADOW_OK ] ||
  die "marker ombre simple invalide"
grep -qx 'shadow_profile=simple' "$METRICS"
grep -qx 'receiver_overflow=false' "$METRICS"
grep -qx 'allocations_during_playing=0' "$METRICS"
grep -qx 'stale_shadow_handles=0' "$METRICS"
grep -qx 'synchronization=true' "$METRICS"
grep -qx 'error_code=0' "$METRICS"
receiver_count="$(
  awk -F= '$1 == "receiver_triangles_selected" {print $2}' "$METRICS")"
[ -n "$receiver_count" ] &&
  [ "$receiver_count" -gt 0 ] &&
  [ "$receiver_count" -le 512 ] ||
  die "nombre de receivers invalide"

printf '%s\n' \
  "PSP_SHADOW_SIMPLE_CHECKPOINT_OK receivers=$receiver_count"
