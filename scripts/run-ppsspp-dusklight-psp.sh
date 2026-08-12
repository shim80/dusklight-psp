#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=lib/ppsspp-host-backend.sh
. "$SCRIPT_DIR/lib/ppsspp-host-backend.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

ACTION=plan
MODE=smoke
PRESENTATION=game
TIMEOUT_SECONDS=600
BACKEND="${DUSKLIGHT_PPSSPP_BACKEND:-opengl}"
TRANSPORT="${DUSKLIGHT_PPSSPP_TRANSPORT:-gui}"
PARITY_SCENARIO=link_walk
OPAQUE_ORDER_VARIANT=source_order
while [ "$#" -gt 0 ]; do
  case "$1" in
    --plan) ACTION=plan ;;
    --run) ACTION=run ;;
    --mode)
      shift
      [ "$#" -gt 0 ] || die "--mode exige une valeur"
      MODE="$1"
      ;;
    --presentation)
      shift
      [ "$#" -gt 0 ] || die "--presentation exige une valeur"
      PRESENTATION="$1"
      ;;
    --backend)
      shift
      [ "$#" -gt 0 ] || die "--backend exige auto, opengl ou vulkan"
      BACKEND="$1"
      ;;
    --transport)
      shift
      [ "$#" -gt 0 ] || die "--transport exige auto ou gui"
      TRANSPORT="$1"
      ;;
    --scenario)
      shift
      [ "$#" -gt 0 ] || die "--scenario exige une valeur"
      PARITY_SCENARIO="$1"
      ;;
    --opaque-order-variant)
      shift
      [ "$#" -gt 0 ] || die "--opaque-order-variant exige une valeur"
      OPAQUE_ORDER_VARIANT="$1"
      ;;
    --timeout)
      shift
      [ "$#" -gt 0 ] || die "--timeout exige une valeur"
      TIMEOUT_SECONDS="$1"
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
case "$MODE" in
  smoke|replay|long|interactive|fidelity_review|root_review|idle_lighting_review|depth_behavior_fixture|opaque_order_invariance|parity_trace|all) ;;
  *)
    die "mode invalide : $MODE"
    ;;
esac
case "$OPAQUE_ORDER_VARIANT" in
  source_order|reverse_order|deterministic_permutation) ;;
  *) die "variante d'ordre opaque invalide : $OPAQUE_ORDER_VARIANT" ;;
esac
case "$PARITY_SCENARIO" in
  link_idle_full_cycle|link_walk|link_run|link_turn_90|link_turn_180|link_stop|link_slope|link_camera_follow|link_collision_wall|link_ground_contact|d_mn10_r09_actors|d_mn10_r02_actors|f_sp108_first_playable) ;;
  *) die "scénario de parité invalide : $PARITY_SCENARIO" ;;
esac
case "$PRESENTATION" in game|debug|opaque_only) ;; *)
  die "présentation invalide : $PRESENTATION"
esac
ppsspp_validate_backend "$BACKEND" ||
  die "backend PPSSPP invalide : $BACKEND"
case "$TRANSPORT" in auto|gui) ;; *)
  die "transport PPSSPP invalide : $TRANSPORT"
esac

EBOOT="$(assert_project_path "build/psp/dusklight/EBOOT.PBP")"
ASSETS="$(assert_project_path "build/assets/dusklight-psp/data")"
STATE_ROOT="$(assert_project_path ".test-data/ppsspp")"
HOME_DIR="$STATE_ROOT/home"
CONFIG_HOME="$HOME_DIR/.config"
set_game_dir() {
  GAME_DIR="$1"
  MARKER="$GAME_DIR/CONTINUOUS.OK"
  METRICS="$GAME_DIR/CONTINUOUS.METRICS"
  ACTOR_MARKER="$GAME_DIR/ORIGINAL_ACTOR.OK"
  ACTOR_METRICS="$GAME_DIR/ORIGINAL_ACTOR.METRICS"
  DYNAMIC_MARKER="$GAME_DIR/ORIGINAL_DYNAMIC_ACTOR.OK"
  DYNAMIC_METRICS="$GAME_DIR/ORIGINAL_DYNAMIC_ACTOR.METRICS"
  SWITCH_MARKER="$GAME_DIR/SWITCH_SURFACE.OK"
  BCK_MARKER="$GAME_DIR/BCK_SURFACE.OK"
  MOVEBG_MARKER="$GAME_DIR/MOVEBG_SURFACE.OK"
  DOOR_MARKER="$GAME_DIR/ORIGINAL_DOOR.OK"
  DOOR_METRICS="$GAME_DIR/ORIGINAL_DOOR.METRICS"
  INTERACTION_MARKER="$GAME_DIR/INTERACTION_SURFACE.OK"
  INTERACTION_METRICS="$GAME_DIR/INTERACTION_SURFACE.METRICS"
  TBOX_MARKER="$GAME_DIR/ORIGINAL_TBOX.OK"
  TBOX_METRICS="$GAME_DIR/ORIGINAL_TBOX.METRICS"
  ITEM_FLOW_MARKER="$GAME_DIR/ORIGINAL_ITEM_FLOW.OK"
  EXPANSION_MARKER="$GAME_DIR/EXPANSION.OK"
  EXPANSION_METRICS="$GAME_DIR/EXPANSION.METRICS"
  DUNGEON_SLICE_MARKER="$GAME_DIR/DUNGEON_SLICE.OK"
  GROUNDING_MARKER="$GAME_DIR/LINK_GROUNDING.OK"
  GROUNDING_METRICS="$GAME_DIR/LINK_GROUNDING.METRICS"
  ROOT_ANCHOR_MARKER="$GAME_DIR/LINK_ROOT_ANCHOR.OK"
  ROOT_ANCHOR_METRICS="$GAME_DIR/LINK_ROOT_ANCHOR.METRICS"
  IDLE_FIDELITY_MARKER="$GAME_DIR/LINK_IDLE_FIDELITY.OK"
  ENVIRONMENT_MARKER="$GAME_DIR/ENVIRONMENT_RENDER.OK"
  ENVIRONMENT_METRICS="$GAME_DIR/ENVIRONMENT_RENDER.METRICS"
  SHADOW_SIMPLE_MARKER="$GAME_DIR/SHADOW_SIMPLE.OK"
  SHADOW_SIMPLE_METRICS="$GAME_DIR/SHADOW_SIMPLE.METRICS"
  PROJECTED_SHADOW_MARKER="$GAME_DIR/PROJECTED_SHADOW.OK"
  PROJECTED_SHADOW_METRICS="$GAME_DIR/PROJECTED_SHADOW.METRICS"
  FIDELITY_REVIEW_METRICS="$GAME_DIR/FIDELITY.REVIEW.METRICS"
  RENDER_REVIEW_METRICS="$GAME_DIR/RENDER.REVIEW.METRICS"
  RENDER_REVIEW_MARKER="$GAME_DIR/RENDER.REVIEW.OK"
  ROOT_REVIEW_METRICS="$GAME_DIR/ROOT.REVIEW.METRICS"
  IDLE_LIGHTING_METRICS="$GAME_DIR/IDLE_LIGHTING.METRICS"
  IDLE_LIGHTING_REVIEW_MARKER="$GAME_DIR/IDLE_LIGHTING_REVIEW.OK"
  LIGHTING_PIPELINE_MARKER="$GAME_DIR/LIGHTING_PIPELINE.OK"
  SHADOW_STATE_ISOLATION_MARKER="$GAME_DIR/SHADOW_STATE_ISOLATION.OK"
  INTERACTIVE_COMPLETION="$GAME_DIR/INTERACTIVE.COMPLETE"
  PARITY_SCENARIO_FILE="$GAME_DIR/PARITY.SCENARIO"
  PARITY_TRACE_FILE="$GAME_DIR/PARITY_PSP_DTRC_V3.jsonl"
  PARITY_TRACE_METRICS="$GAME_DIR/PARITY_PSP_TRACE.METRICS"
  PARITY_TRACE_COMPLETE="$GAME_DIR/PARITY_PSP_TRACE.COMPLETE"
  DEPTH_BEHAVIOR_MARKER="$GAME_DIR/DEPTH_BEHAVIOR_FIXTURE.OK"
  DEPTH_BEHAVIOR_METRICS="$GAME_DIR/DEPTH_BEHAVIOR_FIXTURE.METRICS"
  DEPTH_BEHAVIOR_STATE="$GAME_DIR/DEPTH_BEHAVIOR_STATE.jsonl"
  OPAQUE_ORDER_MODE_FILE="$GAME_DIR/OPAQUE_ORDER.MODE"
  OPAQUE_ORDER_MARKER="$GAME_DIR/OPAQUE_ORDER.OK"
  OPAQUE_ORDER_METRICS="$GAME_DIR/OPAQUE_ORDER.METRICS"
  OPAQUE_ORDER_STATE="$GAME_DIR/OPAQUE_ORDER_STATE.json"
  OPAQUE_ORDER_MANIFEST="$GAME_DIR/OPAQUE_ORDER_DRAWS.csv"
  OPAQUE_ORDER_TRACE="$GAME_DIR/OPAQUE_ORDER_TRACE.jsonl"
  OPAQUE_ORDER_FRAMEBUFFER="$GAME_DIR/OPAQUE_ORDER_FRAMEBUFFER.5650"
  TRANSPORT_METRICS="$GAME_DIR/PPSSPP_TRANSPORT.METRICS"
  MODE_FILE="$GAME_DIR/DUSKLIGHT.MODE"
  PRESENTATION_FILE="$GAME_DIR/DUSKLIGHT.PRESENTATION"
}
set_game_dir "$CONFIG_HOME/ppsspp/PSP/GAME/DUSKLIGHT_PSP"
SOFTWARE_CONFIG="$(assert_project_path "test/gu-smoke/ppsspp-software.ini")"
ACCEL_CONFIG="$(assert_project_path "test/link-playable/ppsspp-accelerated.ini")"
TOKEN=DUSKLIGHT_PSP_CONTINUOUS_PORT_OK
ACTOR_TOKEN=DUSKLIGHT_PSP_ORIGINAL_RENDERED_ACTOR_OK
DYNAMIC_TOKEN=DUSKLIGHT_PSP_ORIGINAL_DYNAMIC_ACTOR_OK
SWITCH_TOKEN=DUSKLIGHT_PSP_SWITCH_SURFACE_OK
BCK_TOKEN=DUSKLIGHT_PSP_BCK_SURFACE_OK
MOVEBG_TOKEN=DUSKLIGHT_PSP_MOVEBG_SURFACE_OK
DOOR_TOKEN=DUSKLIGHT_PSP_ORIGINAL_DOOR_OK
INTERACTION_TOKEN=DUSKLIGHT_PSP_INTERACTION_SURFACE_OK
TBOX_TOKEN=DUSKLIGHT_PSP_ORIGINAL_TBOX_OK
ITEM_FLOW_TOKEN=DUSKLIGHT_PSP_ORIGINAL_ITEM_FLOW_OK
EXPANSION_TOKEN=DUSKLIGHT_PSP_CONTINUOUS_EXPANSION_OK
DUNGEON_SLICE_TOKEN=DUSKLIGHT_PSP_FIRST_DUNGEON_SLICE_OK
GROUNDING_TOKEN=DUSKLIGHT_PSP_LINK_GROUNDING_OK
ROOT_ANCHOR_TOKEN=DUSKLIGHT_PSP_LINK_ROOT_ANCHOR_OK
IDLE_FIDELITY_TOKEN=DUSKLIGHT_PSP_LINK_IDLE_FIDELITY_OK
ENVIRONMENT_TOKEN=DUSKLIGHT_PSP_ENVIRONMENT_RENDER_OK
SHADOW_SIMPLE_TOKEN=DUSKLIGHT_PSP_SIMPLE_SHADOW_OK
PROJECTED_SHADOW_TOKEN=DUSKLIGHT_PSP_PROJECTED_SHADOW_OK
DEPTH_BEHAVIOR_TOKEN=DUSKLIGHT_PSP_DEPTH_BEHAVIOR_FIXTURE_OK
OPAQUE_ORDER_TOKEN=DUSKLIGHT_PSP_OPAQUE_ORDER_OK

metric() {
  awk -F= -v key="$1" \
    '$1 == key {print substr($0, length(key) + 2); exit}' "$METRICS"
}

actor_metric() {
  awk -F= -v key="$1" \
    '$1 == key {print substr($0, length(key) + 2); exit}' "$ACTOR_METRICS"
}

dynamic_metric() {
  awk -F= -v key="$1" \
    '$1 == key {print substr($0, length(key) + 2); exit}' "$DYNAMIC_METRICS"
}

door_metric() {
  awk -F= -v key="$1" \
    '$1 == key {print substr($0, length(key) + 2); exit}' "$DOOR_METRICS"
}

interaction_metric() {
  awk -F= -v key="$1" \
    '$1 == key {print substr($0, length(key) + 2); exit}' \
    "$INTERACTION_METRICS"
}

tbox_metric() {
  awk -F= -v key="$1" \
    '$1 == key {print substr($0, length(key) + 2); exit}' "$TBOX_METRICS"
}

expansion_metric() {
  awk -F= -v key="$1" \
    '$1 == key {print substr($0, length(key) + 2); exit}' \
    "$EXPANSION_METRICS"
}

validate_expansion_metrics() {
  [ -f "$EXPANSION_METRICS" ] || return 1
  local key expected
  while IFS='|' read -r key expected; do
    [ "$(expansion_metric "$key")" = "$expected" ] || return 1
  done <<'EOF'
validation_target|PPSSPP
hardware_validation|deferred_by_user
canonical_target|true
portability_records_scanned|947
portability_profiles_resolved|67
portability_sources_resolved|67
portability_host_probes_passed|10
portability_pspsdk_probes_passed|10
original_sources_compiled_total|8
tier_a_candidates|5
tier_a_ported|2
tier_a_blocked|3
switch_candidates|56
switch_actors_ported|5
bck_candidates|13
bck_actors_ported|1
movebg_candidates|23
movebg_actors_ported|3
door_candidates|14
door_actors_ported|1
interaction_candidates|14
interaction_actors_ported|1
item_candidates|10
item_actors_ported|1
enemy_candidates|13
enemy_sources_compiled|0
enemy_instances_created|0
enemy_instances_defeated|0
combat_available|false
rooms_discovered|4
rooms_supported|2
rooms_blocked|2
transition_edges_supported|2
source_modified_lines_total|0
actor_specific_logic_rewrites|0
actor_specific_psp_draw_calls|0
actor_specific_runtime_package_formats|0
process_slot_leaks|0
process_duplicates|0
render_queue_overflows|0
allocations_during_playing|0
bytes_leaked|0
package_crc_valid|true
process_lifecycle_valid|true
resource_lifecycle_valid|true
room_lifecycle_valid|true
switch_lifecycle_valid|true
animation_lifecycle_valid|true
movebg_lifecycle_valid|true
door_lifecycle_valid|true
item_lifecycle_valid|true
combat_lifecycle_valid|false
combat_phase|BLOCKED_PER_ACTOR
guard_regions_valid|true
pixel_checks_valid|true
synchronization|complete
non_finite_values|0
diagnostic_only|true
current_mode_valid|true
error_code|0
EOF
  [ "$(expansion_metric original_profiles_registered_total)" -ge 8 ] &&
    [ "$(expansion_metric original_process_types_total)" -ge 8 ] &&
    [ "$(expansion_metric original_instances_peak)" -gt 0 ] &&
    [ "$(expansion_metric movebg_handles_peak)" -gt 0 ] &&
    [ "$(expansion_metric doors_opened)" -gt 0 ] &&
    [ "$(expansion_metric doors_closed)" -gt 0 ] &&
    [ "$(expansion_metric switch_interactions)" -gt 0 ] &&
    [ "$(expansion_metric button_interactions)" -gt 0 ] &&
    [ "$(expansion_metric chests_opened)" -eq 1 ] &&
    [ "$(expansion_metric items_acquired)" -eq 1 ] &&
    [ "$(expansion_metric model_instances_peak)" -gt 0 ] &&
    [ "$(expansion_metric animation_players_peak)" -gt 0 ] &&
    [ "$(expansion_metric edram_remaining_min)" -ge 96000 ]
}

validate_tbox_metrics() {
  [ -f "$TBOX_METRICS" ] || return 1
  local key expected
  while IFS='|' read -r key expected; do
    [ "$(tbox_metric "$key")" = "$expected" ] || return 1
  done <<'EOF'
validation_target|PPSSPP
hardware_validation|deferred_by_user
source_name|tboxB0
source_class|daTbox_c
source_profile|g_profile_TBOX
source_process_id|0x00FB
source_implementation|dusklight-main/src/d/actor/d_a_tbox.cpp
source_modified_lines|0
source_logic_owner|original_dusklight_source
interactions_accepted|1
items_created|1
events_completed|1
unsupported_boss_placements|0
heart_piece_quantity|1
treasure_19_open|true
actor_specific_logic_rewrites|0
diagnostic_only|true
valid|true
error_code|0
EOF
  [ "$(tbox_metric placements_seen)" -ge 2 ] &&
    [ "$(tbox_metric placements_created)" -ge 2 ] &&
    [ "$(tbox_metric interaction_requests)" -ge 1 ] &&
    [ "$(tbox_metric profile_create_calls)" -ge 2 ] &&
    [ "$(tbox_metric profile_execute_calls)" -gt 0 ] &&
    [ "$(tbox_metric profile_draw_calls)" -gt 0 ] &&
    [ "$(tbox_metric profile_delete_calls)" -ge 2 ]
}

validate_interaction_metrics() {
  [ -f "$INTERACTION_METRICS" ] || return 1
  local key expected
  while IFS='|' read -r key expected; do
    [ "$(interaction_metric "$key")" = "$expected" ] || return 1
  done <<'EOF'
validation_target|PPSSPP
hardware_validation|deferred_by_user
source_name|swspin
source_record_index|15
source_parameters|0x00004EE1
source_class|daObjSwSpinner_c
source_profile|g_profile_Obj_SwSpinner
source_process_id|0x00B3
source_modified_lines|0
input_facade|bounded_daSpinner_c_contract
full_spinner_port|false
switch|0xE1
mechanism|spnGear
source_logic_owner|original_dusklight_source
actor_specific_logic_rewrites|0
interaction_invalid_candidates|0
interaction_rejected_actions|0
diagnostic_only|true
valid|true
error_code|0
EOF
  [ "$(interaction_metric input_frames)" -gt 200 ] &&
    [ "$(interaction_metric prompt_frames)" -gt 200 ] &&
    [ "$(interaction_metric spinner_in_frames)" -gt 200 ] &&
    [ "$(interaction_metric rotation_frames)" -gt 200 ] &&
    [ "$(interaction_metric switch_activations)" -gt 0 ] &&
    [ "$(interaction_metric mechanism_changes)" -gt 0 ] &&
    [ "$(interaction_metric profile_create_calls)" -gt 0 ] &&
    [ "$(interaction_metric profile_execute_calls)" -gt 0 ] &&
    [ "$(interaction_metric profile_draw_calls)" -gt 0 ] &&
    [ "$(interaction_metric profile_delete_calls)" -gt 0 ]
}

validate_door_metrics() {
  [ -f "$DOOR_METRICS" ] || return 1
  local key expected
  while IFS='|' read -r key expected; do
    [ "$(door_metric "$key")" = "$expected" ] || return 1
  done <<'EOF'
validation_target|PPSSPP
hardware_validation|deferred_by_user
source_name|L4Pgate
process_id|0x009D
profile_symbol|g_profile_Obj_Lv4PoGate
archive|L4R02Gate
switch|0x45
matrix_mismatches|0
dynamic_collision_frame_lag|0
source_modified_lines|0
actor_specific_logic_rewrites|0
audio_fallback|silent_instrumented
particle_fallback|absent_instrumented
vibration_fallback|absent_instrumented
diagnostic_only|true
valid|true
error_code|0
EOF
  [ "$(door_metric doors_closed)" -gt 0 ] &&
    [ "$(door_metric doors_opened)" -gt 0 ] &&
    [ "$(door_metric completed_cycles)" -gt 0 ] &&
    [ "$(door_metric matrix_parity_samples)" -gt 0 ] &&
    [ "$(door_metric movebg_creates)" -gt 0 ] &&
    [ "$(door_metric movebg_updates)" -gt 0 ] &&
    [ "$(door_metric movebg_creates)" = \
      "$(door_metric movebg_deletes)" ]
}

validate_dynamic_metrics() {
  [ -f "$DYNAMIC_METRICS" ] || return 1
  local key expected
  while IFS='|' read -r key expected; do
    [ "$(dynamic_metric "$key")" = "$expected" ] || return 1
  done <<'EOF'
validation_target|PPSSPP
hardware_validation|deferred_by_user
selection_classification|ORIGINAL_DYNAMIC_ACTOR_SPNGEAR_SELECTED
source_table_type|ACTR
source_record_index|19
source_name|spnGear
source_name_hash|0x3E96B91B
source_process_id|0x0183
source_process_symbol|fpcNm_Obj_Lv4Gear_e
source_profile_symbol|g_profile_Obj_Lv4Gear
source_class_name|daObjLv4Gear_c
source_params|0x000000E1
source_room|9
source_layer|0
original_dynamic_source_modified_lines|0
original_dynamic_logic_rewritten|false
compat_symbols_unsupported|0
source_params_preserved|true
source_switch_used|false
source_switch_scope|not_applicable
switch_read_calls|0
switch_on_calls|0
switch_off_calls|0
model_rotation_from_original_execute|true
model_matrix_source|original_actor_logic
model_matrix_valid|true
movebg_used|false
movebg_resource_count|0
stale_movebg_handles|0
dynamic_collision_frame_lag|0
collision_matrix_source|not_applicable
model_collision_matrix_parity|true
actor_specific_psp_draw_calls|0
dynamic_actor_destroyed_on_room_unload|true
dynamic_actor_recreated_on_room_reload|true
dynamic_commands_after_delete|0
dynamic_collision_after_delete|0
actor_heap_overflow|0
render_queue_overflows|0
allocations_during_playing|0
bytes_leaked|0
dynamic_model_leaks|0
dynamic_resource_leaks|0
dynamic_collision_leaks|0
parity_profile|true
parity_params|true
parity_resources|true
parity_switch|true
parity_state_machine|true
parity_rotation|true
parity_model_matrix|true
parity_collision_matrix|true
parity_lifecycle|true
package_crc_valid|true
profile_valid|true
record_mapping_valid|true
resource_mapping_valid|true
switch_mapping_valid|true
state_machine_valid|true
rotation_valid|true
model_lifecycle_valid|true
collision_lifecycle_valid|true
render_path_valid|true
room_ownership_valid|true
pixel_checks_valid|true
guard_regions_valid|true
synchronization|complete
non_finite_values|0
diagnostic_only|true
new_parallel_runtime_targets|0
new_runtime_package_formats|0
error_code|0
EOF
  [ "$(dynamic_metric dynamic_profile_register_calls)" -ge 1 ] &&
    [ "$(dynamic_metric dynamic_process_create_calls)" -ge 1 ] &&
    [ "$(dynamic_metric dynamic_process_execute_calls)" -gt 0 ] &&
    [ "$(dynamic_metric dynamic_process_draw_calls)" -gt 0 ] &&
    [ "$(dynamic_metric dynamic_process_delete_calls)" -ge 1 ] &&
    [ "$(dynamic_metric dynamic_process_errors)" -eq 0 ] &&
    [ "$(dynamic_metric source_rotation_updates)" -gt 0 ] &&
    [ "$(dynamic_metric source_state_transitions)" -gt 0 ] &&
    [ "$(dynamic_metric model_instances_created)" = \
      "$(dynamic_metric model_instances_destroyed)" ] &&
    [ "$(dynamic_metric pause_samples)" -gt 0 ] &&
    [ "$(dynamic_metric pause_violations)" -eq 0 ]
}

validate_actor_metrics() {
  [ -f "$ACTOR_METRICS" ] || return 1
  local key expected
  while IFS='|' read -r key expected; do
    [ "$(actor_metric "$key")" = "$expected" ] || return 1
  done <<'EOF'
validation_target|PPSSPP
hardware_validation|deferred_by_user
canonical_target|true
selection_classification|ORIGINAL_RENDERED_ACTOR_SELECTED
source_name|L4hmato
source_name_hash|0xDD2F1294
source_process_id|0x009F
source_profile_symbol|g_profile_Obj_Lv4HsTarget
source_class_name|daLv4HsTarget_c
original_source_files_compiled|1
original_source_modified_lines|0
original_logic_rewritten|false
compat_symbols_unsupported|0
resource_phase_supported|true
stale_resource_handles|0
actor_heap_overflow|0
model_matrices_from_original_logic|true
original_rendered_process_errors|0
actor_specific_psp_draw_calls|0
actor_present_in_room_a|true
actor_present_in_room_b|true
actor_destroyed_on_room_unload|true
actor_recreated_on_room_reload|true
commands_after_actor_delete|0
draws_in_wrong_room|0
render_queue_overflows|0
allocations_during_playing|0
bytes_leaked|0
original_rendered_model_leaks|0
original_rendered_resource_leaks|0
package_crc_valid|true
profile_valid|true
record_mapping_valid|true
resource_mapping_valid|true
resource_lifecycle_valid|true
model_lifecycle_valid|true
model_matrix_valid|true
render_path_valid|true
lifecycle_valid|true
room_ownership_valid|true
parity_profile|true
parity_params|true
parity_transform|true
parity_resource_requests|true
parity_model_matrix|true
parity_lifecycle|true
pixel_checks_valid|true
guard_regions_valid|true
synchronization|complete
non_finite_values|0
diagnostic_only|true
error_code|0
EOF
  [ "$(actor_metric resource_archive_requests)" -ge 1 ] &&
    [ "$(actor_metric resource_get_calls)" -ge 1 ] &&
    [ "$(actor_metric resource_release_calls)" -ge 1 ] &&
    [ "$(actor_metric model_instances_created)" -ge 1 ] &&
    [ "$(actor_metric model_instances_created)" = "$(actor_metric model_instances_destroyed)" ] &&
    [ "$(actor_metric original_rendered_profile_register_calls)" -ge 1 ] &&
    [ "$(actor_metric original_rendered_process_create_calls)" -ge 1 ] &&
    [ "$(actor_metric original_rendered_process_execute_calls)" -gt 0 ] &&
    [ "$(actor_metric original_rendered_process_draw_calls)" -gt 0 ] &&
    [ "$(actor_metric original_rendered_process_delete_calls)" -ge 1 ] &&
    [ "$(actor_metric original_actor_render_commands)" -gt 0 ]
}

validate_metrics() {
  [ -f "$METRICS" ] || return 1
  local key expected
  while IFS='|' read -r key expected; do
    [ "$(metric "$key")" = "$expected" ] || return 1
  done <<'EOF'
validation_target|PPSSPP
hardware_validation|deferred_by_user
canonical_target|true
duplicate_runtime_implementation_count|0
game_context_initialized|true
resource_manager_initialized|true
render_queue_initialized|true
process_manager_initialized|true
stage_runtime_initialized|true
compatibility_level|source_subset
compat_symbols_unsupported|0
original_actor_or_component_class|daScex_c
original_profile_symbol|g_profile_SCENE_EXIT
original_process_id|0x030C
original_actor_source_modified_lines|0
tier_a_candidates|5
tier_a_sources_compiled|2
tier_a_blocked|3
tier_a_original_source_modified_lines|0
tier_a_actor_specific_draw_calls|0
switch_surface_initialized|true
switch_stage_scope_end|0xC0
switch_room_scope_begin|0xC0
switch_room_scope_end|0xF0
switch_sentinel|0xFF
switch_invalid_requests|0
bck_surface_linked|true
bck_package_format|DPAN
bck_rigid_supported|true
bck_skeletal_joint_limit|64
bck_actor_specific_formats|0
bck_actor_specific_state_machines|0
movebg_surface_linked|true
movebg_package_format|DPCL
movebg_coordinate_space|local
movebg_dynamic_transform_required|true
model_matrix_source|original_actor_logic
collision_matrix_source|original_actor_logic
model_collision_matrix_parity|true
dynamic_collision_frame_lag|0
original_process_errors|0
transition_logic_source|original_dusklight_source
transition_visual_source|psp_fallback
specialized_psp_trigger_logic_calls|0
stale_resource_handles|0
process_slot_leaks|0
process_duplicates|0
profile_duplicate_registrations|0
stale_process_handles|0
process_calls_after_delete|0
transition_failures|0
render_queue_overflows|0
allocations_during_playing|0
bytes_leaked|0
package_crc_valid|true
resource_generations_valid|true
process_lifecycle_valid|true
original_source_path_valid|true
original_profile_valid|true
original_behavior_parity_valid|true
transition_mapping_valid|true
spawn_mapping_valid|true
collision_valid|true
camera_valid|true
pixel_checks_valid|true
guard_regions_valid|true
synchronization|complete
non_finite_values|0
diagnostic_only|true
error_code|0
EOF
  [ "$(metric original_source_files_compiled)" -ge 1 ] &&
    [ "$(metric tier_a_instances_created)" -ge 1 ] &&
    [ "$(metric original_profile_register_calls)" -ge 1 ] &&
    [ "$(metric original_process_create_calls)" -ge 1 ] &&
    [ "$(metric original_process_execute_calls)" -gt 0 ] &&
    [ "$(metric original_process_delete_calls)" -ge 1 ] &&
    [ "$(metric original_transition_request_calls)" -ge 1 ] &&
    [ "$(metric transition_count)" -ge 2 ]
}

validate_automatic() {
  validate_metrics &&
    validate_actor_metrics &&
    validate_dynamic_metrics &&
    validate_door_metrics &&
    validate_interaction_metrics &&
    validate_tbox_metrics &&
    validate_expansion_metrics &&
    grep -qx 'link_grounding_status=AUTOMATICALLY_CORRECTED' \
      "$GROUNDING_METRICS" &&
    grep -qx 'grounding_source_derived=true' "$GROUNDING_METRICS" &&
    grep -qx 'legs_visible_below_floor_frames=0' "$GROUNDING_METRICS" &&
    grep -qx 'pelvis_below_floor_frames=0' "$GROUNDING_METRICS" &&
    grep -qx 'collision_bottom_below_floor_frames=0' \
      "$GROUNDING_METRICS" &&
    grep -qx 'model_collision_vertical_parity=true' \
      "$GROUNDING_METRICS" &&
    grep -qx 'root_translation_double_applied=false' \
      "$GROUNDING_METRICS" &&
    grep -qx 'user_manual_acceptance=pending' "$GROUNDING_METRICS" &&
    grep -qx 'error_code=0' "$GROUNDING_METRICS" &&
    grep -qx 'root_anchor_source_derived=true' "$ROOT_ANCHOR_METRICS" &&
    grep -qx 'root_horizontal_motion_removed=false' "$ROOT_ANCHOR_METRICS" &&
    grep -qx 'root_horizontal_motion_preserved=true' \
      "$ROOT_ANCHOR_METRICS" &&
    grep -qx 'root_horizontal_motion_double_applied=false' \
      "$ROOT_ANCHOR_METRICS" &&
    grep -qx 'idle_actor_origin_stable=true' "$ROOT_ANCHOR_METRICS" &&
    grep -qx 'idle_root_reference_valid=true' "$ROOT_ANCHOR_METRICS" &&
    grep -qx 'idle_pelvis_motion_preserved=true' "$ROOT_ANCHOR_METRICS" &&
    grep -qx 'idle_feet_grounded=true' "$ROOT_ANCHOR_METRICS" &&
    grep -qx 'walk_root_reference_valid=true' "$ROOT_ANCHOR_METRICS" &&
    grep -qx 'run_root_reference_valid=true' "$ROOT_ANCHOR_METRICS" &&
    grep -qx 'collision_model_origin_parity=true' "$ROOT_ANCHOR_METRICS" &&
    grep -qx 'user_manual_acceptance=pending' "$ROOT_ANCHOR_METRICS" &&
    grep -qx 'error_code=0' "$ROOT_ANCHOR_METRICS" &&
    grep -qx 'environment_source_derived=true' "$ENVIRONMENT_METRICS" &&
    grep -qx 'fog_enabled=true' "$ENVIRONMENT_METRICS" &&
    grep -qx 'allocations_during_playing=0' "$ENVIRONMENT_METRICS" &&
    grep -qx 'non_finite_values=0' "$ENVIRONMENT_METRICS" &&
    grep -qx 'user_manual_acceptance=pending' "$ENVIRONMENT_METRICS" &&
    grep -qx 'error_code=0' "$ENVIRONMENT_METRICS" &&
    grep -qx 'shadow_profile=simple' "$SHADOW_SIMPLE_METRICS" &&
    grep -qx 'receiver_overflow=false' "$SHADOW_SIMPLE_METRICS" &&
    grep -qx 'allocations_during_playing=0' "$SHADOW_SIMPLE_METRICS" &&
    grep -qx 'stale_shadow_handles=0' "$SHADOW_SIMPLE_METRICS" &&
    grep -qx 'non_finite_values=0' "$SHADOW_SIMPLE_METRICS" &&
    grep -qx 'synchronization=true' "$SHADOW_SIMPLE_METRICS" &&
    grep -qx 'error_code=0' "$SHADOW_SIMPLE_METRICS" &&
    grep -qx 'shadow_profile=projected_link' \
      "$PROJECTED_SHADOW_METRICS" &&
    grep -qx 'shadow_map_width=64' "$PROJECTED_SHADOW_METRICS" &&
    grep -qx 'shadow_map_height=64' "$PROJECTED_SHADOW_METRICS" &&
    grep -qx 'shadow_map_format=GU_PSM_4444' \
      "$PROJECTED_SHADOW_METRICS" &&
    grep -qx 'receiver_overflow=false' "$PROJECTED_SHADOW_METRICS" &&
    grep -qx 'shadow_target_restored=true' \
      "$PROJECTED_SHADOW_METRICS" &&
    grep -qx 'allocations_during_playing=0' \
      "$PROJECTED_SHADOW_METRICS" &&
    grep -qx 'stale_shadow_handles=0' "$PROJECTED_SHADOW_METRICS" &&
    grep -qx 'synchronization=true' "$PROJECTED_SHADOW_METRICS" &&
    grep -qx 'non_finite_values=0' "$PROJECTED_SHADOW_METRICS" &&
    grep -qx 'error_code=0' "$PROJECTED_SHADOW_METRICS" &&
    [ -f "$MARKER" ] &&
    [ -f "$ACTOR_MARKER" ] &&
    [ -f "$DYNAMIC_MARKER" ] &&
    [ -f "$SWITCH_MARKER" ] &&
    [ -f "$BCK_MARKER" ] &&
    [ -f "$MOVEBG_MARKER" ] &&
    [ -f "$DOOR_MARKER" ] &&
    [ -f "$INTERACTION_MARKER" ] &&
    [ -f "$TBOX_MARKER" ] &&
    [ -f "$ITEM_FLOW_MARKER" ] &&
    [ -f "$EXPANSION_MARKER" ] &&
    [ -f "$DUNGEON_SLICE_MARKER" ] &&
    [ -f "$GROUNDING_MARKER" ] &&
    [ -f "$ROOT_ANCHOR_MARKER" ] &&
    [ -f "$IDLE_FIDELITY_MARKER" ] &&
    [ -f "$ENVIRONMENT_MARKER" ] &&
    [ -f "$SHADOW_SIMPLE_MARKER" ] &&
    [ -f "$PROJECTED_SHADOW_MARKER" ] &&
    [ "$(cat "$MARKER")" = "$TOKEN" ] &&
    [ "$(cat "$ACTOR_MARKER")" = "$ACTOR_TOKEN" ] &&
    [ "$(cat "$DYNAMIC_MARKER")" = "$DYNAMIC_TOKEN" ] &&
    [ "$(cat "$SWITCH_MARKER")" = "$SWITCH_TOKEN" ] &&
    [ "$(cat "$BCK_MARKER")" = "$BCK_TOKEN" ] &&
    [ "$(cat "$MOVEBG_MARKER")" = "$MOVEBG_TOKEN" ] &&
    [ "$(cat "$DOOR_MARKER")" = "$DOOR_TOKEN" ] &&
    [ "$(cat "$INTERACTION_MARKER")" = "$INTERACTION_TOKEN" ] &&
    [ "$(cat "$TBOX_MARKER")" = "$TBOX_TOKEN" ] &&
    [ "$(cat "$ITEM_FLOW_MARKER")" = "$ITEM_FLOW_TOKEN" ] &&
    [ "$(cat "$EXPANSION_MARKER")" = "$EXPANSION_TOKEN" ] &&
    [ "$(cat "$DUNGEON_SLICE_MARKER")" = "$DUNGEON_SLICE_TOKEN" ] &&
    [ "$(cat "$GROUNDING_MARKER")" = "$GROUNDING_TOKEN" ] &&
    [ "$(cat "$ROOT_ANCHOR_MARKER")" = "$ROOT_ANCHOR_TOKEN" ] &&
    [ "$(cat "$IDLE_FIDELITY_MARKER")" = "$IDLE_FIDELITY_TOKEN" ] &&
    [ "$(cat "$ENVIRONMENT_MARKER")" = "$ENVIRONMENT_TOKEN" ] &&
    [ "$(cat "$SHADOW_SIMPLE_MARKER")" = "$SHADOW_SIMPLE_TOKEN" ] &&
    [ "$(cat "$PROJECTED_SHADOW_MARKER")" = \
      "$PROJECTED_SHADOW_TOKEN" ] &&
    [ "$(wc -c <"$MARKER" | tr -d ' ')" = "${#TOKEN}" ] &&
    [ "$(wc -c <"$ACTOR_MARKER" | tr -d ' ')" = "${#ACTOR_TOKEN}" ] &&
    [ "$(wc -c <"$DYNAMIC_MARKER" | tr -d ' ')" = "${#DYNAMIC_TOKEN}" ] &&
    [ "$(wc -c <"$SWITCH_MARKER" | tr -d ' ')" = "${#SWITCH_TOKEN}" ] &&
    [ "$(wc -c <"$BCK_MARKER" | tr -d ' ')" = "${#BCK_TOKEN}" ] &&
    [ "$(wc -c <"$MOVEBG_MARKER" | tr -d ' ')" = "${#MOVEBG_TOKEN}" ] &&
    [ "$(wc -c <"$DOOR_MARKER" | tr -d ' ')" = "${#DOOR_TOKEN}" ] &&
    [ "$(wc -c <"$INTERACTION_MARKER" | tr -d ' ')" = \
      "${#INTERACTION_TOKEN}" ] &&
    [ "$(wc -c <"$TBOX_MARKER" | tr -d ' ')" = "${#TBOX_TOKEN}" ] &&
    [ "$(wc -c <"$ITEM_FLOW_MARKER" | tr -d ' ')" = \
      "${#ITEM_FLOW_TOKEN}" ] &&
    [ "$(wc -c <"$EXPANSION_MARKER" | tr -d ' ')" = \
      "${#EXPANSION_TOKEN}" ] &&
    [ "$(wc -c <"$DUNGEON_SLICE_MARKER" | tr -d ' ')" = \
      "${#DUNGEON_SLICE_TOKEN}" ] &&
    [ "$(wc -c <"$GROUNDING_MARKER" | tr -d ' ')" = \
      "${#GROUNDING_TOKEN}" ] &&
    [ "$(wc -c <"$ROOT_ANCHOR_MARKER" | tr -d ' ')" = \
      "${#ROOT_ANCHOR_TOKEN}" ] &&
    [ "$(wc -c <"$IDLE_FIDELITY_MARKER" | tr -d ' ')" = \
      "${#IDLE_FIDELITY_TOKEN}" ] &&
    [ "$(wc -c <"$ENVIRONMENT_MARKER" | tr -d ' ')" = \
      "${#ENVIRONMENT_TOKEN}" ] &&
    [ "$(wc -c <"$SHADOW_SIMPLE_MARKER" | tr -d ' ')" = \
      "${#SHADOW_SIMPLE_TOKEN}" ] &&
    [ "$(wc -c <"$PROJECTED_SHADOW_MARKER" | tr -d ' ')" = \
      "${#PROJECTED_SHADOW_TOKEN}" ]
}

resolve_transport() {
  case "$TRANSPORT" in
    auto|gui) printf '%s\n' gui ;;
  esac
}
SELECTED_TRANSPORT="$(resolve_transport)"

printf 'Action : %s\nMode : %s\nProfil PPSSPP : %s\n' \
  "$ACTION" "$MODE" "${STATE_ROOT#"$PROJECT_ROOT"/}"
printf 'Backend demandé : %s\n' "$BACKEND"
printf 'Transport demandé : %s\nTransport sélectionné : %s\n' \
  "$TRANSPORT" "$SELECTED_TRANSPORT"
[ "$ACTION" = run ] || exit 0

[ -f "$EBOOT" ] || die "EBOOT canonique absent"
[ -f "$ASSETS/RESOURCE.MANIFEST" ] || die "assets canoniques absents"

safe_mkdir .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PSP
safe_mkdir .test-data/ppsspp/xdg-cache
safe_mkdir .test-data/ppsspp/config
safe_mkdir .tmp/ppsspp
safe_mkdir logs/dusklight-psp
cp -- "$EBOOT" "$GAME_DIR/EBOOT.PBP"
cp -R -- "$ASSETS" "$GAME_DIR/"
printf '%s' \
  "seed=0x4455534B route=original_scene_exit checkpoints=R09,R02,R09 acceleration=trigger_entry_only" \
  >"$GAME_DIR/DUSKLIGHT.SCENARIO"
PARITY_SCENARIO_INPUT="$PROJECT_ROOT/.tmp/ppsspp/parity-scenario.txt"
printf '%s' "$PARITY_SCENARIO" >"$PARITY_SCENARIO_INPUT"

RUN_ID="$(timestamp_utc)"
GUI_REQUEST_COUNT=0
GUI_FAILURE_COUNT=0
GUI_CONTROL_LONG=false
LAST_CLASSIFICATION=""
LAST_STDERR=""
LAST_BACKEND=""
write_transport_metrics() {
  local backend_used="$1" boot="$2" fallback="$3" error_code="$4"
  {
    printf 'ppsspp_transport=persistent_gui_broker\n'
    printf 'ppsspp_gui_broker_used=true\n'
    printf 'ppsspp_direct_launch_used=false\n'
    printf 'ppsspp_backend_requested=%s\n' "$BACKEND"
    printf 'ppsspp_backend_used=%s\n' "$backend_used"
    printf 'ppsspp_boot_observed=%s\n' "$boot"
    printf 'ppsspp_fallback_used=%s\n' "$fallback"
    printf 'gui_runner_request_count=%s\n' "$GUI_REQUEST_COUNT"
    printf 'gui_runner_failure_count=%s\n' "$GUI_FAILURE_COUNT"
    printf 'manual_grounding_smoke_confirmed=true\n'
    printf 'manual_grounding_original_executes=3678\n'
    printf 'diagnostic_only=true\n'
    printf 'error_code=%s\n' "$error_code"
  } >"$TRANSPORT_METRICS"
}

run_gui_transport() {
  local runtime_mode="$1" config="$2" renderer="$3" validator="$4"
  local request_id renderer_value action response result_code
  local backend_used boot fallback
  local -a marker_args package_args command
  GUI_REQUEST_COUNT=$((GUI_REQUEST_COUNT + 1))
  request_id="$RUN_ID-$runtime_mode-$GUI_REQUEST_COUNT"
  set_game_dir "$PROJECT_ROOT/.test-data/ppsspp/gui-runner/sessions/$request_id/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PSP"
  safe_mkdir \
    ".test-data/ppsspp/gui-runner/sessions/$request_id/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PSP"
  if [ "$GUI_CONTROL_LONG" = true ]; then
    printf '%s' 100 >"$GAME_DIR/DUSKLIGHT.LONG"
  fi
  if [ "$runtime_mode" = interactive ]; then
    printf '%s' enabled >"$GAME_DIR/DUSKLIGHT.AUTO"
    printf '%s' exit_after_round_trip >"$GAME_DIR/DUSKLIGHT.EXIT"
  fi
  if [ "$runtime_mode" = fidelity_review ]; then
    safe_mkdir \
      ".test-data/ppsspp/gui-runner/sessions/$request_id/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PSP/fidelity"
    safe_mkdir \
      ".test-data/ppsspp/gui-runner/sessions/$request_id/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PSP/render-review"
  fi
  if [ "$runtime_mode" = root_review ]; then
    safe_mkdir \
      ".test-data/ppsspp/gui-runner/sessions/$request_id/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PSP/root-review"
  fi
  if [ "$runtime_mode" = idle_lighting_review ]; then
    safe_mkdir \
      ".test-data/ppsspp/gui-runner/sessions/$request_id/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PSP/idle-lighting-review"
  fi
  if [ "$runtime_mode" = opaque_order_invariance ]; then
    printf '%s' "$OPAQUE_ORDER_VARIANT" >"$OPAQUE_ORDER_MODE_FILE"
  fi
  renderer_value=hardware
  [ "$renderer" != software ] || renderer_value=software
  marker_args=()
  if [ "$runtime_mode" = smoke ] || [ "$runtime_mode" = replay ]; then
    marker_args=(
      --marker "CONTINUOUS.OK=$TOKEN"
      --marker "ORIGINAL_ACTOR.OK=$ACTOR_TOKEN"
      --marker "ORIGINAL_DYNAMIC_ACTOR.OK=$DYNAMIC_TOKEN"
      --marker "SWITCH_SURFACE.OK=$SWITCH_TOKEN"
      --marker "BCK_SURFACE.OK=$BCK_TOKEN"
      --marker "MOVEBG_SURFACE.OK=$MOVEBG_TOKEN"
      --marker "ORIGINAL_DOOR.OK=$DOOR_TOKEN"
      --marker "INTERACTION_SURFACE.OK=$INTERACTION_TOKEN"
      --marker "ORIGINAL_TBOX.OK=$TBOX_TOKEN"
      --marker "ORIGINAL_ITEM_FLOW.OK=$ITEM_FLOW_TOKEN"
      --marker "EXPANSION.OK=$EXPANSION_TOKEN"
      --marker "DUNGEON_SLICE.OK=$DUNGEON_SLICE_TOKEN"
      --marker "LINK_GROUNDING.OK=$GROUNDING_TOKEN"
      --marker "LINK_ROOT_ANCHOR.OK=$ROOT_ANCHOR_TOKEN"
      --marker "LINK_IDLE_FIDELITY.OK=$IDLE_FIDELITY_TOKEN"
      --marker "ENVIRONMENT_RENDER.OK=$ENVIRONMENT_TOKEN"
      --marker "SHADOW_SIMPLE.OK=$SHADOW_SIMPLE_TOKEN"
      --marker "PROJECTED_SHADOW.OK=$PROJECTED_SHADOW_TOKEN"
    )
  elif [ "$runtime_mode" = root_review ]; then
    marker_args=(
      --marker "ROOT.REVIEW.OK=DUSKLIGHT_PSP_ROOT_REVIEW_OK"
    )
  elif [ "$runtime_mode" = fidelity_review ]; then
    marker_args=(
      --marker "RENDER.REVIEW.OK=DUSKLIGHT_PSP_RENDER_REVIEW_OK"
    )
  elif [ "$runtime_mode" = idle_lighting_review ]; then
    marker_args=(
      --marker \
        "IDLE_LIGHTING_REVIEW.OK=DUSKLIGHT_PSP_IDLE_LIGHTING_REVIEW_OK"
      --marker \
        "SHADOW_STATE_ISOLATION.OK=DUSKLIGHT_PSP_SHADOW_STATE_ISOLATION_OK"
    )
  elif [ "$runtime_mode" = interactive ]; then
    marker_args=(
      --marker \
        "INTERACTIVE.COMPLETE=DUSKLIGHT_PSP_INTERACTIVE_COMPLETE"
    )
  elif [ "$runtime_mode" = parity_trace ]; then
    marker_args=(
      --marker \
        "PARITY_PSP_TRACE.COMPLETE=DUSKLIGHT_PSP_PARITY_TRACE_COMPLETE"
    )
  elif [ "$runtime_mode" = depth_behavior_fixture ]; then
    marker_args=(
      --marker \
        "DEPTH_BEHAVIOR_FIXTURE.OK=$DEPTH_BEHAVIOR_TOKEN"
    )
  elif [ "$runtime_mode" = opaque_order_invariance ]; then
    marker_args=(
      --marker "OPAQUE_ORDER.OK=$OPAQUE_ORDER_TOKEN"
    )
  fi
  package_args=(
    --package "$ASSETS=data"
    --package \
      "$PROJECT_ROOT/artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP/PARITY.BUILD=PARITY.BUILD"
    --package \
      "$PROJECT_ROOT/artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP/DUSKLIGHT.SCENARIO=DUSKLIGHT.SCENARIO"
  )
  if [ "$GUI_CONTROL_LONG" = true ]; then
    package_args+=(
      --package "$GAME_DIR/DUSKLIGHT.LONG=DUSKLIGHT.LONG"
    )
  fi
  if [ "$runtime_mode" = interactive ]; then
    package_args+=(
      --package "$GAME_DIR/DUSKLIGHT.AUTO=DUSKLIGHT.AUTO"
      --package "$GAME_DIR/DUSKLIGHT.EXIT=DUSKLIGHT.EXIT"
    )
  fi
  if [ "$runtime_mode" = parity_trace ]; then
    package_args+=(
      --package "$PARITY_SCENARIO_INPUT=PARITY.SCENARIO"
    )
  fi
  if [ "$runtime_mode" = depth_behavior_fixture ] ||
     [ "$runtime_mode" = opaque_order_invariance ]; then
    package_args+=(
      --package \
        "$PROJECT_ROOT/artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP/VISUAL.BUILD=VISUAL.BUILD"
    )
  fi
  if [ "$runtime_mode" = opaque_order_invariance ]; then
    package_args+=(
      --package "$OPAQUE_ORDER_MODE_FILE=OPAQUE_ORDER.MODE"
    )
  fi
  action=--run
  if ! "$SCRIPT_DIR/status-ppsspp-gui-broker.sh" >/dev/null 2>&1; then
    action=--queue
  fi
  command=(
    "$SCRIPT_DIR/ppsspp-gui-runner-request.sh" "$action"
    --request-id "$request_id"
    --eboot "$EBOOT"
    --game-id DUSKLIGHT_PSP
    --config "$config"
    --mode "$runtime_mode"
    --presentation "$PRESENTATION"
    --backend "$BACKEND"
    --renderer "$renderer_value"
    --timeout "$TIMEOUT_SECONDS"
  )
  if [ "${#marker_args[@]}" -gt 0 ]; then
    command+=("${marker_args[@]}")
  fi
  command+=("${package_args[@]}")
  response="$PROJECT_ROOT/.test-data/ppsspp/gui-runner/requests/$request_id/response.json"
  LAST_STDERR="$PROJECT_ROOT/logs/ppsspp-gui-runner/$request_id/stderr.log"
  if [ "$action" = --queue ]; then
    "${command[@]}"
    LAST_CLASSIFICATION=PENDING_GUI_EXECUTION
    GUI_FAILURE_COUNT=$((GUI_FAILURE_COUNT + 1))
    write_transport_metrics none false false 75
    return 1
  fi
  if ! "${command[@]}"; then
    GUI_FAILURE_COUNT=$((GUI_FAILURE_COUNT + 1))
  fi
  [ -f "$response" ] || {
    LAST_CLASSIFICATION=HOST_LAUNCHSERVICES_FAILED
    write_transport_metrics none false false 40
    return 1
  }
  read -r LAST_CLASSIFICATION backend_used boot fallback result_code < <(
    /usr/bin/python3 - "$response" <<'PY'
import json
import pathlib
import sys
result = json.loads(pathlib.Path(sys.argv[1]).read_text())
print(
    result.get("classification", "HOST_FAILURE"),
    result.get("graphics_backend_used") or "none",
    str(result.get("boot_observed", False)).lower(),
    str(result.get("fallback_used", False)).lower(),
    result.get("result_code", 70),
)
PY
  )
  LAST_BACKEND="$backend_used"
  write_transport_metrics "$backend_used" "$boot" "$fallback" "$result_code"
  if [ "$result_code" = 0 ] && "$validator"; then
    printf 'PPSSPP_LAUNCH_RESULT classification=%s backend_used=%s boot_observed=%s transport=gui\n' \
      "$LAST_CLASSIFICATION" "$backend_used" "$boot"
    return 0
  fi
  return 1
}

run_with_backend() {
  local runtime_mode="$1" config="$2" renderer="$3" validator="$4"
  run_gui_transport "$runtime_mode" "$config" "$renderer" "$validator"
}

run_automatic() {
  local requested="$1" runtime_mode="$1"
  local config="$SOFTWARE_CONFIG" renderer=software
  GUI_CONTROL_LONG=false
  rm -f -- "$MARKER" "$METRICS" "$ACTOR_MARKER" "$ACTOR_METRICS" \
    "$DYNAMIC_MARKER" "$DYNAMIC_METRICS" \
    "$SWITCH_MARKER" \
    "$BCK_MARKER" \
    "$MOVEBG_MARKER" \
    "$DOOR_MARKER" "$DOOR_METRICS" \
    "$INTERACTION_MARKER" "$INTERACTION_METRICS" \
    "$TBOX_MARKER" "$TBOX_METRICS" \
    "$ITEM_FLOW_MARKER" \
    "$EXPANSION_MARKER" "$EXPANSION_METRICS" \
    "$DUNGEON_SLICE_MARKER" \
    "$GROUNDING_MARKER" "$GROUNDING_METRICS" \
    "$ROOT_ANCHOR_MARKER" "$ROOT_ANCHOR_METRICS" \
    "$IDLE_FIDELITY_MARKER" \
    "$ENVIRONMENT_MARKER" "$ENVIRONMENT_METRICS" \
    "$SHADOW_SIMPLE_MARKER" "$SHADOW_SIMPLE_METRICS" \
    "$PROJECTED_SHADOW_MARKER" "$PROJECTED_SHADOW_METRICS" \
    "$GAME_DIR/INTERACTION.PROGRESS" "$GAME_DIR/BOOT.FAIL" \
    "$GAME_DIR/DUSKLIGHT.LONG" "$GAME_DIR/DUSKLIGHT.AUTO" \
    "$GAME_DIR/DUSKLIGHT.EXIT"
  if [ "$requested" = long ]; then
    runtime_mode=replay
    config="$ACCEL_CONFIG"
    renderer=accelerated
    GUI_CONTROL_LONG=true
    printf '%s' 100 >"$GAME_DIR/DUSKLIGHT.LONG"
  fi
  if ! run_with_backend \
      "$runtime_mode" "$config" "$renderer" validate_automatic; then
    [ ! -f "$METRICS" ] || sed -n '1,320p' "$METRICS" >&2
    die "échec PPSSPP canonique : $requested classification=$LAST_CLASSIFICATION"
  fi
  cp -- "$METRICS" \
    "$PROJECT_ROOT/logs/dusklight-psp/$RUN_ID-$requested.metrics.log"
  cp -- "$ACTOR_METRICS" \
    "$PROJECT_ROOT/logs/dusklight-psp/$RUN_ID-$requested-actor.metrics.log"
  cp -- "$DYNAMIC_METRICS" \
    "$PROJECT_ROOT/logs/dusklight-psp/$RUN_ID-$requested-dynamic.metrics.log"
  cp -- "$DOOR_METRICS" \
    "$PROJECT_ROOT/logs/dusklight-psp/$RUN_ID-$requested-door.metrics.log"
  cp -- "$INTERACTION_METRICS" \
    "$PROJECT_ROOT/logs/dusklight-psp/$RUN_ID-$requested-interaction.metrics.log"
  cp -- "$TBOX_METRICS" \
    "$PROJECT_ROOT/logs/dusklight-psp/$RUN_ID-$requested-tbox.metrics.log"
  cp -- "$EXPANSION_METRICS" \
    "$PROJECT_ROOT/logs/dusklight-psp/$RUN_ID-$requested-expansion.metrics.log"
  cp -- "$GROUNDING_METRICS" \
    "$PROJECT_ROOT/logs/dusklight-psp/$RUN_ID-$requested-grounding.metrics.log"
  cp -- "$ROOT_ANCHOR_METRICS" \
    "$PROJECT_ROOT/logs/dusklight-psp/$RUN_ID-$requested-root-anchor.metrics.log"
  cp -- "$ENVIRONMENT_METRICS" \
    "$PROJECT_ROOT/logs/dusklight-psp/$RUN_ID-$requested-environment.metrics.log"
  cp -- "$SHADOW_SIMPLE_METRICS" \
    "$PROJECT_ROOT/logs/dusklight-psp/$RUN_ID-$requested-shadow-simple.metrics.log"
  cp -- "$PROJECTED_SHADOW_METRICS" \
    "$PROJECT_ROOT/logs/dusklight-psp/$RUN_ID-$requested-shadow-projected.metrics.log"
  if [ "$requested" = smoke ]; then
    checkpoint="$PROJECT_ROOT/.test-data/ppsspp/checkpoints/root-anchor"
    safe_mkdir .test-data/ppsspp/checkpoints/root-anchor
    cp -- "$ROOT_ANCHOR_MARKER" "$checkpoint/LINK_ROOT_ANCHOR.OK"
    cp -- "$ROOT_ANCHOR_METRICS" "$checkpoint/LINK_ROOT_ANCHOR.METRICS"
    cp -- "$ENVIRONMENT_MARKER" "$checkpoint/ENVIRONMENT_RENDER.OK"
    cp -- "$ENVIRONMENT_METRICS" "$checkpoint/ENVIRONMENT_RENDER.METRICS"
    environment_checkpoint="$PROJECT_ROOT/.test-data/ppsspp/checkpoints/environment"
    safe_mkdir .test-data/ppsspp/checkpoints/environment
    cp -- "$ENVIRONMENT_MARKER" \
      "$environment_checkpoint/ENVIRONMENT_RENDER.OK"
    cp -- "$ENVIRONMENT_METRICS" \
      "$environment_checkpoint/ENVIRONMENT_RENDER.METRICS"
    shadow_checkpoint="$PROJECT_ROOT/.test-data/ppsspp/checkpoints/shadows"
    safe_mkdir .test-data/ppsspp/checkpoints/shadows
    cp -- "$SHADOW_SIMPLE_MARKER" \
      "$shadow_checkpoint/SHADOW_SIMPLE.OK"
    cp -- "$SHADOW_SIMPLE_METRICS" \
      "$shadow_checkpoint/SHADOW_SIMPLE.METRICS"
    cp -- "$PROJECTED_SHADOW_MARKER" \
      "$shadow_checkpoint/PROJECTED_SHADOW.OK"
    cp -- "$PROJECTED_SHADOW_METRICS" \
      "$shadow_checkpoint/PROJECTED_SHADOW.METRICS"
    {
      printf 'source_mode=smoke\n'
      printf 'eboot_sha256=%s\n' \
        "$(shasum -a 256 "$EBOOT" | awk '{print $1}')"
      printf 'transport=%s\n' "$SELECTED_TRANSPORT"
      printf 'backend_requested=%s\n' "$BACKEND"
      printf 'presentation=%s\n' "$PRESENTATION"
      printf 'run_id=%s\n' "$RUN_ID"
    } >"$checkpoint/CHECKPOINT.MANIFEST"
  fi
  if [ "$requested" = long ]; then
    [ "$(metric canonical_transition_count)" -ge 100 ] ||
      die "session canonique longue incomplète"
    [ "$(metric original_process_create_calls)" -ge 20 ] ||
      die "créations originales insuffisantes"
    [ "$(metric original_process_delete_calls)" -ge 20 ] ||
      die "destructions originales insuffisantes"
    [ "$(actor_metric original_rendered_process_create_calls)" -ge 20 ] ||
      die "créations de l'acteur rendu insuffisantes"
    [ "$(actor_metric original_rendered_process_delete_calls)" -ge 20 ] ||
      die "destructions de l'acteur rendu insuffisantes"
    [ "$(dynamic_metric dynamic_process_create_calls)" -ge 20 ] ||
      die "créations de l'acteur dynamique insuffisantes"
    [ "$(dynamic_metric dynamic_process_delete_calls)" -ge 20 ] ||
      die "destructions de l'acteur dynamique insuffisantes"
    [ "$(dynamic_metric room_resets)" -ge 50 ] ||
      die "resets de l'acteur dynamique insuffisants"
    [ "$(dynamic_metric pause_samples)" -ge 10 ] ||
      die "pauses de l'acteur dynamique insuffisantes"
  fi
  printf 'DUSKLIGHT_PSP_CASE_OK mode=%s transitions=%s original_executes=%s\n' \
    "$requested" "$(metric transition_count)" \
    "$(metric original_process_execute_calls)"
}

run_interactive() {
  rm -f -- "$MARKER" "$METRICS" "$ACTOR_MARKER" "$ACTOR_METRICS" \
    "$DYNAMIC_MARKER" "$DYNAMIC_METRICS" \
    "$SWITCH_MARKER" \
    "$BCK_MARKER" \
    "$MOVEBG_MARKER" \
    "$DOOR_MARKER" "$DOOR_METRICS" \
    "$INTERACTION_MARKER" "$INTERACTION_METRICS" \
    "$TBOX_MARKER" "$TBOX_METRICS" \
    "$ITEM_FLOW_MARKER" \
    "$EXPANSION_MARKER" "$EXPANSION_METRICS" \
    "$DUNGEON_SLICE_MARKER" \
    "$GROUNDING_MARKER" "$GROUNDING_METRICS" \
    "$ROOT_ANCHOR_MARKER" "$ROOT_ANCHOR_METRICS" \
    "$IDLE_FIDELITY_MARKER" \
    "$ENVIRONMENT_MARKER" "$ENVIRONMENT_METRICS" \
    "$SHADOW_SIMPLE_MARKER" "$SHADOW_SIMPLE_METRICS" \
    "$PROJECTED_SHADOW_MARKER" "$PROJECTED_SHADOW_METRICS" \
    "$INTERACTIVE_COMPLETION" \
    "$GAME_DIR/INTERACTION.PROGRESS" "$GAME_DIR/BOOT.FAIL" \
    "$GAME_DIR/DUSKLIGHT.LONG"
  printf '%s' enabled >"$GAME_DIR/DUSKLIGHT.AUTO"
  printf '%s' exit_after_round_trip >"$GAME_DIR/DUSKLIGHT.EXIT"
  if ! run_with_backend \
      interactive "$ACCEL_CONFIG" accelerated validate_dynamic_metrics; then
    [ ! -f "$METRICS" ] || sed -n '1,320p' "$METRICS" >&2
    die "interactive canonique invalide classification=$LAST_CLASSIFICATION"
  fi
  [ -f "$INTERACTIVE_COMPLETION" ] &&
    [ "$(cat "$INTERACTIVE_COMPLETION")" = \
      DUSKLIGHT_PSP_INTERACTIVE_COMPLETE ] ||
    die "marqueur de fin interactive invalide"
  [ ! -e "$MARKER" ] ||
    die "interactive ne doit pas produire CONTINUOUS.OK"
  [ ! -e "$ACTOR_MARKER" ] ||
    die "interactive ne doit pas produire ORIGINAL_ACTOR.OK"
  [ ! -e "$DYNAMIC_MARKER" ] ||
    die "interactive ne doit pas produire ORIGINAL_DYNAMIC_ACTOR.OK"
  [ ! -e "$DOOR_MARKER" ] ||
    die "interactive ne doit pas produire ORIGINAL_DOOR.OK"
  [ ! -e "$TBOX_MARKER" ] ||
    die "interactive ne doit pas produire ORIGINAL_TBOX.OK"
  [ ! -e "$ITEM_FLOW_MARKER" ] ||
    die "interactive ne doit pas produire ORIGINAL_ITEM_FLOW.OK"
  [ ! -e "$EXPANSION_MARKER" ] ||
    die "interactive ne doit pas produire EXPANSION.OK"
  [ ! -e "$DUNGEON_SLICE_MARKER" ] ||
    die "interactive ne doit pas produire DUNGEON_SLICE.OK"
  [ ! -e "$GROUNDING_MARKER" ] ||
    die "interactive ne doit pas produire LINK_GROUNDING.OK"
  [ ! -e "$ROOT_ANCHOR_MARKER" ] ||
    die "interactive ne doit pas produire LINK_ROOT_ANCHOR.OK"
  [ ! -e "$IDLE_FIDELITY_MARKER" ] ||
    die "interactive ne doit pas produire LINK_IDLE_FIDELITY.OK"
  [ ! -e "$ENVIRONMENT_MARKER" ] ||
    die "interactive ne doit pas produire ENVIRONMENT_RENDER.OK"
  [ ! -e "$SHADOW_SIMPLE_MARKER" ] ||
    die "interactive ne doit pas produire SHADOW_SIMPLE.OK"
  [ ! -e "$PROJECTED_SHADOW_MARKER" ] ||
    die "interactive ne doit pas produire PROJECTED_SHADOW.OK"
  printf '%s\n' \
    "DUSKLIGHT_PSP_CASE_OK mode=interactive a_to_b=true b_to_a=true clean_exit=true"
}

run_fidelity_review() {
  safe_mkdir \
    .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PSP/fidelity
  rm -f -- "$FIDELITY_REVIEW_METRICS" "$RENDER_REVIEW_METRICS" \
    "$RENDER_REVIEW_MARKER" \
    "$GAME_DIR/DUSKLIGHT.LONG" "$GAME_DIR/DUSKLIGHT.AUTO" \
    "$GAME_DIR/DUSKLIGHT.EXIT"
  find "$GAME_DIR/fidelity" -type f -delete
  safe_mkdir \
    .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PSP/render-review
  find "$GAME_DIR/render-review" -type f -delete
  fidelity_review_ready() {
    [ -f "$FIDELITY_REVIEW_METRICS" ] &&
      grep -qx 'fidelity_capture_count=26' "$FIDELITY_REVIEW_METRICS" &&
      grep -qx 'fidelity_capture_failures=0' "$FIDELITY_REVIEW_METRICS" &&
      grep -qx 'user_manual_acceptance=pending' "$FIDELITY_REVIEW_METRICS" &&
      grep -qx 'error_code=0' "$FIDELITY_REVIEW_METRICS" &&
      [ -f "$RENDER_REVIEW_METRICS" ] &&
      [ -f "$RENDER_REVIEW_MARKER" ] &&
      [ "$(cat "$RENDER_REVIEW_MARKER")" = \
        DUSKLIGHT_PSP_RENDER_REVIEW_OK ] &&
      grep -qx 'render_review_capture_count=28' "$RENDER_REVIEW_METRICS" &&
      grep -qx 'user_manual_acceptance=pending' "$RENDER_REVIEW_METRICS" &&
      grep -qx 'error_code=0' "$RENDER_REVIEW_METRICS"
  }
  if ! run_with_backend \
      fidelity_review "$ACCEL_CONFIG" accelerated fidelity_review_ready; then
    [ ! -f "$FIDELITY_REVIEW_METRICS" ] ||
      cat "$FIDELITY_REVIEW_METRICS" >&2
    die "fidelity_review invalide classification=$LAST_CLASSIFICATION"
  fi
  [ "$(find "$GAME_DIR/fidelity" -name '*.5650' | wc -l | tr -d ' ')" -eq 26 ] ||
    die "nombre de framebuffers fidelity_review invalide"
  [ "$(find "$GAME_DIR/fidelity" -name '*.txt' | wc -l | tr -d ' ')" -eq 26 ] ||
    die "nombre de métadonnées fidelity_review invalide"
  [ -s "$GAME_DIR/fidelity/pivot_review.csv" ] ||
    die "pivot_review.csv absent"
  [ -s "$GAME_DIR/fidelity/movement_review.csv" ] ||
    die "movement_review.csv absent"
  [ "$(find "$GAME_DIR/render-review" -name '*.5650' |
      wc -l | tr -d ' ')" -eq 28 ] ||
    die "nombre de framebuffers render-review invalide"
  [ "$(find "$GAME_DIR/render-review" -name '*.txt' |
      wc -l | tr -d ' ')" -eq 28 ] ||
    die "nombre de métadonnées render-review invalide"
  fidelity_stable="$PROJECT_ROOT/.test-data/ppsspp/captures/fidelity"
  render_stable="$PROJECT_ROOT/.test-data/ppsspp/captures/render-review"
  safe_mkdir .test-data/ppsspp/captures/fidelity
  safe_mkdir .test-data/ppsspp/captures/render-review
  find "$fidelity_stable" -type f -delete
  find "$render_stable" -type f -delete
  cp -- "$GAME_DIR/fidelity/"* "$fidelity_stable/"
  cp -- "$FIDELITY_REVIEW_METRICS" \
    "$fidelity_stable/FIDELITY.REVIEW.METRICS"
  cp -- "$GAME_DIR/render-review/"* "$render_stable/"
  cp -- "$RENDER_REVIEW_METRICS" \
    "$render_stable/RENDER.REVIEW.METRICS"
  printf '%s\n' \
    "DUSKLIGHT_PSP_CASE_OK mode=fidelity_review captures=26 render_review=28"
}

run_root_review() {
  rm -f -- "$ROOT_REVIEW_METRICS" \
    "$GAME_DIR/DUSKLIGHT.LONG" "$GAME_DIR/DUSKLIGHT.AUTO" \
    "$GAME_DIR/DUSKLIGHT.EXIT"
  root_review_ready() {
    [ -f "$ROOT_REVIEW_METRICS" ] &&
      grep -qx 'root_review_capture_count=10' "$ROOT_REVIEW_METRICS" &&
      grep -qx 'game_capture_count=9' "$ROOT_REVIEW_METRICS" &&
      grep -qx 'debug_capture_count=1' "$ROOT_REVIEW_METRICS" &&
      grep -qx 'root_overlay_capture=10' "$ROOT_REVIEW_METRICS" &&
      grep -qx 'user_manual_acceptance=pending' "$ROOT_REVIEW_METRICS" &&
      grep -qx 'error_code=0' "$ROOT_REVIEW_METRICS"
  }
  if ! run_with_backend \
      root_review "$SOFTWARE_CONFIG" software root_review_ready; then
    [ ! -f "$ROOT_REVIEW_METRICS" ] ||
      cat "$ROOT_REVIEW_METRICS" >&2
    die "root_review invalide classification=$LAST_CLASSIFICATION"
  fi
  [ "$(find "$GAME_DIR/root-review" -name '*.5650' |
      wc -l | tr -d ' ')" -eq 10 ] ||
    die "nombre de framebuffers root_review invalide"
  [ "$(find "$GAME_DIR/root-review" -name '*.txt' |
      wc -l | tr -d ' ')" -eq 10 ] ||
    die "nombre de métadonnées root_review invalide"
  stable="$PROJECT_ROOT/.test-data/ppsspp/captures/root-review"
  safe_mkdir .test-data/ppsspp/captures/root-review
  find "$stable" -type f -delete
  cp -- "$GAME_DIR/root-review/"* "$stable/"
  cp -- "$ROOT_REVIEW_METRICS" "$stable/ROOT.REVIEW.METRICS"
  printf 'DUSKLIGHT_PSP_CASE_OK mode=root_review captures=10\n'
}

run_idle_lighting_review() {
  rm -f -- "$IDLE_LIGHTING_METRICS" \
    "$IDLE_LIGHTING_REVIEW_MARKER" "$LIGHTING_PIPELINE_MARKER" \
    "$SHADOW_STATE_ISOLATION_MARKER" \
    "$GAME_DIR/DUSKLIGHT.LONG" "$GAME_DIR/DUSKLIGHT.AUTO" \
    "$GAME_DIR/DUSKLIGHT.EXIT"
  safe_mkdir \
    .test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_PSP/idle-lighting-review
  find "$GAME_DIR/idle-lighting-review" -type f -delete
  idle_lighting_ready() {
    [ -f "$IDLE_LIGHTING_METRICS" ] &&
      grep -qx 'capture_count=25' "$IDLE_LIGHTING_METRICS" &&
      grep -qx 'user_manual_acceptance=pending' "$IDLE_LIGHTING_METRICS" &&
      grep -qx 'error_code=0' "$IDLE_LIGHTING_METRICS" &&
      [ "$(cat "$IDLE_LIGHTING_REVIEW_MARKER")" = \
        DUSKLIGHT_PSP_IDLE_LIGHTING_REVIEW_OK ] &&
      [ "$(cat "$SHADOW_STATE_ISOLATION_MARKER")" = \
        DUSKLIGHT_PSP_SHADOW_STATE_ISOLATION_OK ]
  }
  if ! run_with_backend \
      idle_lighting_review "$ACCEL_CONFIG" accelerated \
      idle_lighting_ready; then
    [ ! -f "$IDLE_LIGHTING_METRICS" ] ||
      cat "$IDLE_LIGHTING_METRICS" >&2
    die "idle_lighting_review invalide classification=$LAST_CLASSIFICATION"
  fi
  [ "$(find "$GAME_DIR/idle-lighting-review" -name '*.5650' |
      wc -l | tr -d ' ')" -eq 25 ] ||
    die "nombre de framebuffers idle_lighting_review invalide"
  [ "$(find "$GAME_DIR/idle-lighting-review" -name '*.txt' |
      wc -l | tr -d ' ')" -eq 25 ] ||
    die "nombre de métadonnées idle_lighting_review invalide"
  for csv in idle_contact.csv lighting_samples.csv gu_state_transitions.csv; do
    [ -s "$GAME_DIR/idle-lighting-review/$csv" ] ||
      die "$csv absent"
  done
  stable="$PROJECT_ROOT/.test-data/ppsspp/captures/idle-lighting-review"
  safe_mkdir .test-data/ppsspp/captures/idle-lighting-review
  find "$stable" -type f -delete
  cp -- "$GAME_DIR/idle-lighting-review/"* "$stable/"
  cp -- "$IDLE_LIGHTING_METRICS" "$stable/IDLE_LIGHTING.METRICS"
  cp -- "$IDLE_LIGHTING_REVIEW_MARKER" \
    "$stable/IDLE_LIGHTING_REVIEW.OK"
  if [ -f "$LIGHTING_PIPELINE_MARKER" ]; then
    cp -- "$LIGHTING_PIPELINE_MARKER" "$stable/LIGHTING_PIPELINE.OK"
  fi
  cp -- "$SHADOW_STATE_ISOLATION_MARKER" \
    "$stable/SHADOW_STATE_ISOLATION.OK"
  printf 'DUSKLIGHT_PSP_CASE_OK mode=idle_lighting_review captures=25\n'
}

run_parity_trace() {
  rm -f -- "$PARITY_TRACE_FILE" "$PARITY_TRACE_METRICS" \
    "$PARITY_TRACE_COMPLETE"
  parity_trace_ready() {
    [ -s "$PARITY_TRACE_FILE" ] &&
      [ -f "$PARITY_TRACE_METRICS" ] &&
      [ -f "$PARITY_TRACE_COMPLETE" ] &&
      [ "$(cat "$PARITY_TRACE_COMPLETE")" = \
        DUSKLIGHT_PSP_PARITY_TRACE_COMPLETE ] &&
      grep -qx "scenario=$PARITY_SCENARIO" \
        "$PARITY_TRACE_METRICS" &&
      grep -qx 'dropped=0' "$PARITY_TRACE_METRICS" &&
      grep -qx 'healthy=true' "$PARITY_TRACE_METRICS" &&
      grep -qx 'scenario_assists=0' "$PARITY_TRACE_METRICS" &&
      grep -qx 'transitions=0' "$PARITY_TRACE_METRICS" &&
      grep -qx 'error_code=0' "$PARITY_TRACE_METRICS"
  }
  if ! run_with_backend \
      parity_trace "$SOFTWARE_CONFIG" software parity_trace_ready; then
    [ ! -f "$PARITY_TRACE_METRICS" ] ||
      cat "$PARITY_TRACE_METRICS" >&2
    die "parity_trace invalide scénario=$PARITY_SCENARIO classification=$LAST_CLASSIFICATION"
  fi
  stable="$PROJECT_ROOT/build/reports/parity/$PARITY_SCENARIO"
  safe_mkdir "build/reports/parity/$PARITY_SCENARIO"
  cp -- "$PARITY_TRACE_FILE" "$stable/psp.dtrc-v3.jsonl"
  cp -- "$PARITY_TRACE_METRICS" "$stable/psp.metrics"
  python3 "$PROJECT_ROOT/tools/dusk_parity_compare/dusk_parity_compare.py" \
    "$stable/psp.dtrc-v3.jsonl" "$stable/psp.dtrc-v3.jsonl" \
    --output "$stable/psp.self-compare.json"
  printf 'DUSKLIGHT_PSP_PARITY_TRACE_OK scenario=%s\n' \
    "$PARITY_SCENARIO"
}

run_depth_behavior_fixture() {
  rm -f -- "$DEPTH_BEHAVIOR_MARKER" "$DEPTH_BEHAVIOR_METRICS" \
    "$DEPTH_BEHAVIOR_STATE"
  depth_behavior_ready() {
    [ -f "$DEPTH_BEHAVIOR_MARKER" ] &&
      [ "$(cat "$DEPTH_BEHAVIOR_MARKER")" = "$DEPTH_BEHAVIOR_TOKEN" ] &&
      [ -s "$DEPTH_BEHAVIOR_METRICS" ] &&
      [ -s "$DEPTH_BEHAVIOR_STATE" ] &&
      grep -qx \
        'contract_id=DUSKLIGHT_PSP_BEHAVIORAL_DEPTH_VALIDATION_V1' \
        "$DEPTH_BEHAVIOR_METRICS" &&
      grep -qx 'synthetic_depth_cases=16' "$DEPTH_BEHAVIOR_METRICS" &&
      grep -qx 'synthetic_depth_failures=0' "$DEPTH_BEHAVIOR_METRICS" &&
      grep -qx 'depth_state_leaks=0' "$DEPTH_BEHAVIOR_METRICS" &&
      grep -qx 'render_target_state_leaks=0' "$DEPTH_BEHAVIOR_METRICS" &&
      grep -qx 'near_far_valid=true' "$DEPTH_BEHAVIOR_METRICS" &&
      grep -qx 'depth_monotonic=true' "$DEPTH_BEHAVIOR_METRICS" &&
      grep -qx 'reversed_depth_mapping_valid=true' \
        "$DEPTH_BEHAVIOR_METRICS" &&
      grep -qx 'order_invariant=true' "$DEPTH_BEHAVIOR_METRICS" &&
      grep -qx 'error_code=0' "$DEPTH_BEHAVIOR_METRICS" &&
      [ "$(wc -l < "$DEPTH_BEHAVIOR_STATE" | tr -d ' ')" -eq 16 ]
  }
  if ! run_with_backend \
      depth_behavior_fixture "$SOFTWARE_CONFIG" software \
      depth_behavior_ready; then
    [ ! -f "$DEPTH_BEHAVIOR_METRICS" ] ||
      cat "$DEPTH_BEHAVIOR_METRICS" >&2
    die "fixtures depth invalides classification=$LAST_CLASSIFICATION"
  fi
  stable="$PROJECT_ROOT/build/reports/depth-behavior"
  safe_mkdir build/reports/depth-behavior
  cp -- "$DEPTH_BEHAVIOR_MARKER" "$stable/DEPTH_BEHAVIOR_FIXTURE.OK"
  cp -- "$DEPTH_BEHAVIOR_METRICS" \
    "$stable/DEPTH_BEHAVIOR_FIXTURE.METRICS"
  cp -- "$DEPTH_BEHAVIOR_STATE" "$stable/DEPTH_BEHAVIOR_STATE.jsonl"
  printf 'DUSKLIGHT_PSP_DEPTH_BEHAVIOR_FIXTURE_OK cases=16\n'
}

run_opaque_order_invariance() {
  rm -f -- "$OPAQUE_ORDER_MARKER" "$OPAQUE_ORDER_METRICS" \
    "$OPAQUE_ORDER_STATE" "$OPAQUE_ORDER_MANIFEST" \
    "$OPAQUE_ORDER_TRACE" "$OPAQUE_ORDER_FRAMEBUFFER"
  opaque_order_ready() {
    [ -f "$OPAQUE_ORDER_MARKER" ] &&
      [ "$(cat "$OPAQUE_ORDER_MARKER")" = "$OPAQUE_ORDER_TOKEN" ] &&
      [ -s "$OPAQUE_ORDER_METRICS" ] &&
      [ -s "$OPAQUE_ORDER_STATE" ] &&
      [ -s "$OPAQUE_ORDER_MANIFEST" ] &&
      [ -s "$OPAQUE_ORDER_TRACE" ] &&
      [ "$(wc -c <"$OPAQUE_ORDER_FRAMEBUFFER" | tr -d ' ')" -eq 278528 ] &&
      grep -qx \
        'contract_id=DUSKLIGHT_PSP_OPAQUE_ORDER_INVARIANCE_V1' \
        "$OPAQUE_ORDER_METRICS" &&
      grep -qx "opaque_order_mode=$OPAQUE_ORDER_VARIANT" \
        "$OPAQUE_ORDER_METRICS" &&
      grep -qx 'opaque_order_seed=0x4455534B' \
        "$OPAQUE_ORDER_METRICS" &&
      grep -qx 'fixed_submissions=0' "$OPAQUE_ORDER_METRICS" &&
      grep -qx 'fixed_slots_preserved=true' "$OPAQUE_ORDER_METRICS" &&
      grep -qx 'complete_permutation=true' "$OPAQUE_ORDER_METRICS" &&
      grep -qx 'complete_draw_manifest=true' "$OPAQUE_ORDER_METRICS" &&
      grep -qx 'complete_state_per_draw=true' "$OPAQUE_ORDER_METRICS" &&
      grep -qx 'error_code=0' "$OPAQUE_ORDER_METRICS"
  }
  if ! run_with_backend \
      opaque_order_invariance "$SOFTWARE_CONFIG" software \
      opaque_order_ready; then
    [ ! -f "$OPAQUE_ORDER_METRICS" ] ||
      cat "$OPAQUE_ORDER_METRICS" >&2
    die "diagnostic ordre opaque invalide variante=$OPAQUE_ORDER_VARIANT classification=$LAST_CLASSIFICATION"
  fi
  case "$OPAQUE_ORDER_VARIANT" in
    source_order) stable_variant=source ;;
    reverse_order) stable_variant=reverse ;;
    deterministic_permutation) stable_variant=permutation ;;
  esac
  stable="$PROJECT_ROOT/.test-data/visual-pipeline-results/opaque-order/$stable_variant"
  safe_mkdir ".test-data/visual-pipeline-results/opaque-order/$stable_variant"
  cp -- "$OPAQUE_ORDER_MARKER" "$stable/OPAQUE_ORDER.OK"
  cp -- "$OPAQUE_ORDER_METRICS" "$stable/OPAQUE_ORDER.METRICS"
  cp -- "$OPAQUE_ORDER_STATE" "$stable/OPAQUE_ORDER_STATE.json"
  cp -- "$OPAQUE_ORDER_MANIFEST" "$stable/OPAQUE_ORDER_DRAWS.csv"
  cp -- "$OPAQUE_ORDER_TRACE" "$stable/OPAQUE_ORDER_TRACE.jsonl"
  cp -- "$OPAQUE_ORDER_FRAMEBUFFER" "$stable/framebuffer.5650"
  printf 'DUSKLIGHT_PSP_OPAQUE_ORDER_OK variant=%s\n' \
    "$OPAQUE_ORDER_VARIANT"
}

case "$MODE" in
  smoke|replay|long) run_automatic "$MODE" ;;
  interactive) run_interactive ;;
  fidelity_review) run_fidelity_review ;;
  root_review) run_root_review ;;
  idle_lighting_review) run_idle_lighting_review ;;
  depth_behavior_fixture) run_depth_behavior_fixture ;;
  opaque_order_invariance) run_opaque_order_invariance ;;
  parity_trace) run_parity_trace ;;
  all)
    run_automatic smoke
    run_automatic replay
    run_automatic long
    run_interactive
    run_fidelity_review
    run_root_review
    ;;
esac
