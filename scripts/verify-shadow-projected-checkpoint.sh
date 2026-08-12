#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

CHECKPOINT="$PROJECT_ROOT/.test-data/ppsspp/checkpoints/shadows"
MARKER="$CHECKPOINT/PROJECTED_SHADOW.OK"
METRICS="$CHECKPOINT/PROJECTED_SHADOW.METRICS"
[ -f "$MARKER" ] && [ -f "$METRICS" ] ||
  die "checkpoint ombre projetée absent"
[ "$(cat "$MARKER")" = DUSKLIGHT_PSP_PROJECTED_SHADOW_OK ] ||
  die "marker ombre projetée invalide"
for expected in \
  shadow_profile=projected_link \
  shadow_map_width=64 \
  shadow_map_height=64 \
  shadow_map_format=GU_PSM_4444 \
  receiver_overflow=false \
  shadow_target_restored=true \
  allocations_during_playing=0 \
  stale_shadow_handles=0 \
  synchronization=true \
  non_finite_values=0 \
  error_code=0; do
  grep -qx "$expected" "$METRICS" ||
    die "métrique ombre projetée absente : $expected"
done
for key in shadow_map_updates shadow_map_reuses \
  shadow_caster_vertices receiver_triangles_selected; do
  value="$(awk -F= -v key="$key" '$1 == key {print $2}' "$METRICS")"
  [ -n "$value" ] && [ "$value" -gt 0 ] ||
    die "métrique ombre projetée nulle : $key"
done

printf '%s\n' \
  "PSP_SHADOW_PROJECTED_CHECKPOINT_OK map=64x64 receivers=true"
