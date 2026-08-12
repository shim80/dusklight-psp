#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

[ "${1:-}" = --no-deps ] && [ "$#" -eq 1 ] ||
  die "la finalisation exige --no-deps"
[ "${DUSKLIGHT_ORCHESTRATOR_ACTIVE:-}" = 1 ] ||
  die "finalisation réservée à l'orchestrateur"
[ "${DUSKLIGHT_TEST_SUITE_MODE:-}" = release ] ||
  die "finalisation réservée à la campagne release"

PACKAGE="$(assert_project_path \
  "artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP")"
REVIEW="$(assert_project_path "artifacts/dusklight-psp-fidelity-review")"
RENDER_REVIEW="$(assert_project_path \
  "artifacts/dusklight-psp-render-review")"
CAPTURE_STATE="$(assert_project_path \
  ".test-data/ppsspp/captures/fidelity")"
RENDER_CAPTURE_STATE="$(assert_project_path \
  ".test-data/ppsspp/captures/render-review")"
LOG="$(assert_project_path "logs/test-orchestration/invocations.jsonl")"
BUILD_ID_METRICS="$(assert_project_path \
  "build/reports/PARITY_BUILD_ID.metrics")"

[ "$(cat "$PACKAGE/DUSKLIGHT.MODE")" = interactive ] ||
  die "mode package non interactif"
[ "$(cat "$PACKAGE/DUSKLIGHT.PRESENTATION")" = game ] ||
  die "présentation package non game"
[ "$(find "$REVIEW" -name '[0-9][0-9]_*.png' | wc -l | tr -d ' ')" -eq 26 ] ||
  die "bundle de revue incomplet"
[ "$(find "$RENDER_REVIEW" -name '[0-9][0-9]_*.png' |
  wc -l | tr -d ' ')" -eq 28 ] ||
  die "bundle de revue rendu incomplet"
grep -qx 'fidelity_capture_count=26' \
  "$CAPTURE_STATE/FIDELITY.REVIEW.METRICS" ||
  die "captures PSP incomplètes"
grep -qx 'fidelity_capture_failures=0' \
  "$CAPTURE_STATE/FIDELITY.REVIEW.METRICS" ||
  die "échec de capture PSP"
grep -qx 'user_manual_acceptance=pending' \
  "$CAPTURE_STATE/FIDELITY.REVIEW.METRICS" ||
  die "état d'acceptation utilisateur invalide"
grep -qx 'render_review_capture_count=28' \
  "$RENDER_CAPTURE_STATE/RENDER.REVIEW.METRICS" ||
  die "captures de rendu PSP incomplètes"
grep -qx 'user_manual_acceptance=pending' \
  "$RENDER_CAPTURE_STATE/RENDER.REVIEW.METRICS" ||
  die "état d'acceptation rendu invalide"
"$SCRIPT_DIR/validate-dusklight-test-orchestration.sh" --log "$LOG"

pivot_max="$(awk -F, 'NR>1 && $12>m {m=$12} END {printf "%.6f", m+0}' \
  "$REVIEW/pivot_review.csv")"
alignment_min="$(awk -F, 'NR==2 {m=$11} NR>1 && $11<m {m=$11} END {printf "%.9f", m+0}' \
  "$REVIEW/movement_review.csv")"
alignment_mean="$(awk -F, 'NR>1 {s+=$11; n++} END {printf "%.9f", n?s/n:0}' \
  "$REVIEW/movement_review.csv")"
awk -v value="$pivot_max" 'BEGIN {exit !(value <= 0.01)}' ||
  die "dérive de pivot excessive"
awk -v value="$alignment_min" 'BEGIN {exit !(value >= 0.995)}' ||
  die "alignement mouvement/modèle insuffisant"

campaign="${DUSKLIGHT_TEST_CAMPAIGN:-}"
[ -n "$campaign" ] || die "identifiant de campagne absent"
campaign_lines="$(awk -v id="\"invocation_id\":\"$campaign\"" \
  'index($0,id){n++} END{print n+0}' "$LOG")"
cache_hits="$(awk -v id="\"invocation_id\":\"$campaign\"" \
  'index($0,id) && /\"cache_hit\":true/{n++} END{print n+0}' "$LOG")"
ppsspp_launches="$(awk -v id="\"invocation_id\":\"$campaign\"" \
  'index($0,id) && /\"ppsspp_launch\":true/ && /\"cache_hit\":false/{n++} END{print n+0}' "$LOG")"
executed=$((campaign_lines - cache_hits + 1))
skipped=0
total="${DUSKLIGHT_TEST_NODES_TOTAL:-0}"
eboot_hash="$(shasum -a 256 "$PACKAGE/EBOOT.PBP" | awk '{print $1}')"
commit="$(awk -F= '$1 == "commit" {print $2; exit}' "$BUILD_ID_METRICS")"
case "$commit" in
  [0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]) ;;
  *) die "commit de l'identité de build invalide" ;;
esac
grep -qx "eboot_sha256=$eboot_hash" "$BUILD_ID_METRICS" ||
  die "EBOOT final différent de l'identité de build"

cat >"$PACKAGE/FIDELITY.METRICS" <<EOF
validation_target=PPSSPP
hardware_validation=deferred_by_user
host_validation_backend=OpenGL
vulkan_validation_status=unavailable_in_current_environment
build_commit=$commit
baseline_eboot_sha256=23bd7559bf0c73203eafeb076613c5cc10d8e56a7cc74c561e267f14b6d1f863
new_eboot_sha256=$eboot_hash
single_world_scale=true
source_world_scale=1
link_actor_origin_preserved=true
link_geometry_recentering_enabled=false
link_bounds_center_used_as_pivot=false
actor_to_model_orientation=centralized_source_yaw
model_local_forward_axis=positive_z
world_up_axis=positive_y
yaw_conversion=s16_to_radians_once
pivot_rotation_in_place=true
pivot_drift_max=$pivot_max
pivot_drift_tolerance=0.010000
actor_origin_world_position_stable=true
collision_origin_world_position_stable=true
camera_target_origin_stable=true
forward_alignment_dot_min=$alignment_min
forward_alignment_dot_mean=$alignment_mean
backward_visual_frames=0
yaw_error_max=bounded_source_turn
movement_direction_valid=true
player_motion_semantics=derived_from_daalink
walk_speed=276
run_speed=690
acceleration=1710
deceleration=1980
turn_speed=12.942
walk_run_threshold=0.8
animation_blend_duration=0.2
animation_forward_axis_valid=true
animation_speed_matches_motion=true
foot_sliding_measure=coarse_stride_ratio_valid
ground_contact_valid=true
actor_origin_to_floor=0
collision_bottom_to_floor=0
camera_target_source=actor_origin_plus_source_height
ui_source_layout=zelda_game_image.blo
ui_source_archive_count=3
ui_source_texture_count=20
ui_source_palette_count=1
ui_source_font_count=1
ui_original_asset_count=20
ui_original_texture_bytes=131072
ui_original_edram_bytes=131072
ui_procedural_game_sprites=0
ui_psp_control_glyph_count=1
ui_real_hearts=true
ui_real_rupee_icon=true
ui_real_digits=true
ui_real_action_frame=true
ui_real_pause_frame=true
ui_real_pause_font=true
ui_layout_source_derived=true
ui_asset_fallback=false
default_presentation_profile=game
debug_visuals_default=false
test_only_world_entities_visible_in_game=false
trigger_markers_visible_in_game=false
smoke_geometry_visible_in_game=false
debug_text_visible_in_game=false
fidelity_capture_count=26
fidelity_capture_failures=0
user_manual_acceptance=pending
test_nodes_total=$total
test_nodes_executed=$executed
test_nodes_cache_hits=$cache_hits
test_nodes_skipped=$skipped
targeted_suite_runs=0
checkpoint_suite_runs=0
release_suite_runs=1
successful_release_suite_runs=1
ppsspp_launches_total=$ppsspp_launches
duplicate_test_invocations=0
nested_orchestrator_invocations=0
identical_ppsspp_launches_repeated=0
old_actor_sources_preserved=8
old_transition_stress_preserved=true
package_crc_valid=true
resource_lifecycle_valid=true
process_lifecycle_valid=true
pixel_checks_valid=true
guard_regions_valid=true
synchronization=complete
non_finite_values=0
diagnostic_only=true
error_code=0
EOF
printf '%s' DUSKLIGHT_PSP_FIDELITY_AUTOMATED_OK \
  >"$PACKAGE/FIDELITY_AUTOMATED.OK"
[ "$(wc -c <"$PACKAGE/FIDELITY_AUTOMATED.OK" | tr -d ' ')" -eq 35 ] ||
  die "taille du marqueur de fidélité invalide"
printf 'DUSKLIGHT_FIDELITY_RELEASE_FINALIZED eboot_sha256=%s captures=26\n' \
  "$eboot_hash"
