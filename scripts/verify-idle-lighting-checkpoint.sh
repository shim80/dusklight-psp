#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"

CAPTURES="$(assert_project_path \
  ".test-data/ppsspp/captures/idle-lighting-review")"
CHECKPOINT="$(assert_project_path \
  ".test-data/ppsspp/checkpoints/idle-lighting")"
EBOOT="$(assert_project_path "build/psp/dusklight/EBOOT.PBP")"
METRICS="$CAPTURES/IDLE_LIGHTING.METRICS"

[ -f "$METRICS" ] && [ -f "$CAPTURES/IDLE_LIGHTING_REVIEW.OK" ] &&
  [ -f "$CAPTURES/SHADOW_STATE_ISOLATION.OK" ] ||
  die "preuves idle/lighting incomplètes"
grep -qx 'error_code=0' "$METRICS"
grep -qx 'idle_visual_glide_detected=false' "$METRICS"
grep -qx 'idle_feet_contact_valid=true' "$METRICS"
grep -qx 'default_render_profile=known_good_unlit' "$METRICS"
grep -qx 'default_lighting=off' "$METRICS"
grep -qx 'lighting_status=not_accepted' "$METRICS"
"$SCRIPT_DIR/package-dusklight-idle-lighting-review.sh"
safe_mkdir .test-data/ppsspp/checkpoints/idle-lighting
{
  printf 'checkpoint=idle-lighting\n'
  printf 'classification=READY_DUSKLIGHT_PSP_IDLE_FIDELITY_REVIEW\n'
  printf 'user_manual_acceptance=pending\n'
  printf 'eboot_sha256=%s\n' \
    "$(shasum -a 256 "$EBOOT" | awk '{print $1}')"
  printf 'metrics_sha256=%s\n' \
    "$(shasum -a 256 "$METRICS" | awk '{print $1}')"
  printf 'lighting_marker_present=%s\n' \
    "$([ -f "$CAPTURES/LIGHTING_PIPELINE.OK" ] && echo true || echo false)"
} >"$CHECKPOINT/CHECKPOINT.MANIFEST"
printf '%s\n' \
  'IDLE_LIGHTING_CHECKPOINT_OK classification=READY_DUSKLIGHT_PSP_IDLE_FIDELITY_REVIEW'
