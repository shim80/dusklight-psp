#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

trace=
while [ "$#" -gt 0 ]; do
  case "$1" in
    --trace)
      trace="$2"
      shift 2
      ;;
    *) die "argument inconnu : $1" ;;
  esac
done
[ -n "$trace" ] || die "--trace est requis"
TRACE="$(assert_project_path "$trace")"
[ -s "$TRACE" ] || die "trace desktop de rendu absente"
command -v jq >/dev/null || die "jq est requis"

jq -e -s '
  length > 0 and
  all(.[]; .schema == "dusklight.desktop.render.v1") and
  ([.[].frame] | unique | length) <= 4 and
  ([.[] | select(.type == "render_frame_begin")] | length) ==
    ([.[] | select(.type == "render_frame_end")] | length) and
  ([.[] | select(.type == "render_frame_begin")] | length) > 0 and
  all(.[] | select(.type == "render_submission");
    (.submission_id | type) == "number" and
    (.draw_order | type) == "number" and
    (.actor_id | type) == "number" and
    .actor_id != 4294967295 and
    (.profile | type) == "number" and
    .profile != 65535 and
    (.model_id | type) == "number" and
    .model_id != 65535 and
    (.material_id | type) == "number" and
    (.shape_id | type) == "number")
' "$TRACE" >/dev/null || die "contrat structurel de trace rendu invalide"

required_types='
render_frame_begin
render_frame_end
render_pass_begin
render_pass_end
render_bucket_begin
render_bucket_end
render_submission
render_material_state
render_texture_state
render_depth_state
render_alpha_state
render_blend_state
render_cull_state
render_fog_state
render_lighting_state'
for event_type in $required_types; do
  jq -e --arg type "$event_type" 'select(.type == $type)' "$TRACE" >/dev/null ||
    die "événement rendu desktop absent : $event_type"
done

events="$(wc -l <"$TRACE" | tr -d ' ')"
frames="$(jq -s '[.[].frame] | unique | length' "$TRACE")"
printf 'DUSKLIGHT_DESKTOP_RENDER_TRACE_OK events=%s frames=%s sha256=%s\n' \
  "$events" "$frames" "$(sha256_file "$TRACE")"
