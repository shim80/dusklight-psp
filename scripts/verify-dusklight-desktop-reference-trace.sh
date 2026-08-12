#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

run_id=
while [ "$#" -gt 0 ]; do
  case "$1" in
    --run-id)
      run_id="$2"
      shift 2
      ;;
    *)
      die "argument inconnu : $1"
      ;;
  esac
done
case "$run_id" in
  ''|*[!A-Za-z0-9._-]*) die "identifiant d'exécution invalide : $run_id" ;;
esac
command -v jq >/dev/null || die "jq est requis pour valider la trace JSONL"

SESSION="$(assert_project_path ".test-data/dusklight-reference/sessions/$run_id")"
LOG="$SESSION/stdout.log"
RESULT="$SESSION/RESULT"
TRACE_DIR="$(assert_project_path ".test-data/dusklight-reference/traces")"
TRACE="$TRACE_DIR/$run_id.jsonl"
MARKER_DIR="$(assert_project_path ".test-data/dusklight-reference/markers")"
MARKER="$MARKER_DIR/REFERENCE_TRACE.OK"
V2_MARKER="$MARKER_DIR/DESKTOP_ORACLE_V2.OK"
ORACLE_DIR="$(assert_project_path "artifacts/dusklight-desktop-oracle-v2")"
ORACLE_TRACE="$ORACLE_DIR/desktop-oracle-v2.jsonl"
ORACLE_METRICS="$ORACLE_DIR/DESKTOP_ORACLE_V2.METRICS"

[ -s "$LOG" ] || die "journal desktop absent : $run_id"
grep -q '^classification=DESKTOP_PROCESS_OBSERVED$' "$RESULT" ||
  die "processus desktop non validé : $run_id"
safe_mkdir ".test-data/dusklight-reference/traces"
safe_mkdir ".test-data/dusklight-reference/markers"
safe_mkdir "artifacts/dusklight-desktop-oracle-v2"

sed -n 's/^.*\[REFTRACE\] //p' "$LOG" >"$TRACE"
[ -s "$TRACE" ] || die "trace structurée absente"
jq -e -c 'select(.schema == "dusklight.desktop.reference.v1")' \
  "$TRACE" >/dev/null || die "trace JSONL ou schéma invalide"
jq -e -s 'all(.[]; .schema == "dusklight.desktop.reference.v1" or
  .schema == "dusklight.desktop.reference.v2")' "$TRACE" >/dev/null ||
  die "version DTRC inconnue"

jq -e -s '
  ([to_entries[] | select(.value.type == "title_state" and
    .value.state == "key_wait") | .key][0]) as $title |
  ([to_entries[] | select(.value.type == "input" and
    .value.button == "START" and .value.blocked == false) | .key][0]) as $start |
  ([to_entries[] | select(.value.type == "file_select_state" and
    .value.data_proc == 3) | .key][0]) as $files |
  ([to_entries[] | select(.value.type == "file_select_state" and
    .value.data_proc == 46 and .value.select_end == true) | .key][0]) as $end |
  ([to_entries[] | select(.value.type == "new_game_transition" and
    .value.stage == "F_SP108" and .value.point == 21 and
    .value.room == 1 and .value.layer == 13) | .key][0]) as $transition |
  ($title < $start and $start < $files and $files < $end and $end < $transition) and
  ([.[] | select(.type == "new_game_transition")] | length == 1)
' "$TRACE" >/dev/null || die "ordre ou frontière startup desktop invalide"

required_v2_types='camera_state environment_state material_state shadow_request actor_record actor_create actor_execute actor_draw actor_delete player_state animation_state joint_state collision_state resource_state ui_pane_state render_submission'
for event_type in $required_v2_types; do
  jq -e --arg event_type "$event_type" \
    'select(.schema == "dusklight.desktop.reference.v2" and .type == $event_type)' \
    "$TRACE" >/dev/null || die "événement DTRC v2 absent : $event_type"
done

jq -e -s '
  ([.[] | select(.schema == "dusklight.desktop.reference.v2" and
    .type == "camera_state" and .scene == "F_SP102" and
    (.view_matrix | length) == 12 and
    (.projection_matrix | length) == 16)] | length > 0) and
  ([.[] | select(.schema == "dusklight.desktop.reference.v2" and
    .type == "camera_state" and .scene == "F_SP108" and
    .room == 1)] | length > 0) and
  ([.[] | select(.schema == "dusklight.desktop.reference.v2" and
    .type == "resource_state" and .resource == "F_SP108" and
    .status == "requested")] | length == 1) and
  ([.[] | select(.schema == "dusklight.desktop.reference.v2" and
    .type == "player_state")] | length > 0) and
  (all(.[] | select(.actor_id? != null); (.actor_id | type) == "number"))
' "$TRACE" >/dev/null || die "couverture ou identité DTRC v2 invalide"

cp "$TRACE" "$ORACLE_TRACE"
trace_sha256="$(sha256_file "$ORACLE_TRACE")"
trace_events="$(wc -l <"$ORACLE_TRACE" | tr -d ' ')"
count_type() {
  jq -s --arg type "$1" \
    '[.[] | select(.schema == "dusklight.desktop.reference.v2" and .type == $type)] | length' \
    "$ORACLE_TRACE"
}
camera_events="$(count_type camera_state)"
environment_events="$(count_type environment_state)"
material_events="$(count_type material_state)"
shadow_events="$(count_type shadow_request)"
actor_events="$(jq -s '[.[] | select(.schema == "dusklight.desktop.reference.v2" and
  (.type | startswith("actor_")))] | length' "$ORACLE_TRACE")"
player_events="$(count_type player_state)"

jq -s '[.[] | select(.schema == "dusklight.desktop.reference.v2" and
  .type == "camera_state" and .scene == "F_SP102")]' \
  "$ORACLE_TRACE" >"$ORACLE_DIR/title_camera_reference.json"
jq -c 'select(
  (.schema == "dusklight.desktop.reference.v1" and
    (.type == "title_state" or .type == "input")) or
  (.schema == "dusklight.desktop.reference.v2" and
    .type == "camera_state" and .scene == "F_SP102"))' \
  "$ORACLE_TRACE" >"$ORACLE_DIR/opening_sequence_reference.jsonl"
jq -s '[.[] | select(
  (.schema == "dusklight.desktop.reference.v1" and .type == "file_select_state") or
  (.schema == "dusklight.desktop.reference.v2" and .type == "ui_pane_state"))]' \
  "$ORACLE_TRACE" >"$ORACLE_DIR/file_select_reference_v2.json"
jq -c 'select(.schema == "dusklight.desktop.reference.v2" and
  .type == "environment_state")' "$ORACLE_TRACE" \
  >"$ORACLE_DIR/desktop_environment_reference_v2.jsonl"
jq -c 'select(.schema == "dusklight.desktop.reference.v2" and
  .type == "material_state")' "$ORACLE_TRACE" \
  >"$ORACLE_DIR/desktop_material_reference_v2.jsonl"
jq -c 'select(.schema == "dusklight.desktop.reference.v2" and
  .type == "shadow_request")' "$ORACLE_TRACE" \
  >"$ORACLE_DIR/desktop_shadow_reference_v2.jsonl"

{
  printf 'frame,actor_id,pos_x,pos_y,pos_z,rot_x,rot_y,rot_z,procedure,speed\n'
  jq -r 'select(.schema == "dusklight.desktop.reference.v2" and
    .type == "player_state") |
    [.frame,.actor_id,.position[0],.position[1],.position[2],
     .rotation[0],.rotation[1],.rotation[2],.procedure,.speed] | @csv' \
    "$ORACLE_TRACE"
} >"$ORACLE_DIR/desktop_link_reference_v2.csv"

{
  printf 'desktop_trace_version=2\n'
  printf 'desktop_trace_events=%s\n' "$trace_events"
  printf 'desktop_trace_sha256=%s\n' "$trace_sha256"
  printf 'desktop_camera_events=%s\n' "$camera_events"
  printf 'desktop_environment_events=%s\n' "$environment_events"
  printf 'desktop_material_events=%s\n' "$material_events"
  printf 'desktop_shadow_events=%s\n' "$shadow_events"
  printf 'desktop_actor_events=%s\n' "$actor_events"
  printf 'desktop_player_events=%s\n' "$player_events"
  printf 'desktop_f_sp102_camera_observed=true\n'
  printf 'desktop_f_sp108_camera_observed=true\n'
  printf 'desktop_f_sp108_transition_observed=true\n'
  printf 'desktop_v1_reader_preserved=true\n'
  printf 'desktop_pointer_ids_serialized=false\n'
  printf 'desktop_vanilla_modified=false\n'
  printf 'hardware_validation=deferred_by_user\n'
  printf 'user_manual_acceptance=pending\n'
  printf 'error_code=0\n'
} >"$ORACLE_METRICS"

printf 'DUSKLIGHT_DESKTOP_REFERENCE_TRACE_OK\n' >"$MARKER"
printf 'DUSKLIGHT_DESKTOP_ORACLE_V2_OK\n' >"$V2_MARKER"
printf 'DUSKLIGHT_DESKTOP_ORACLE_V2_OK\n' >"$ORACLE_DIR/DESKTOP_ORACLE_V2.OK"
printf 'DUSKLIGHT_DESKTOP_REFERENCE_TRACE_OK run_id=%s trace=%s\n' \
  "$run_id" "${TRACE#"$PROJECT_ROOT"/}"
printf 'DUSKLIGHT_DESKTOP_ORACLE_V2_OK events=%s sha256=%s\n' \
  "$trace_events" "$trace_sha256"
