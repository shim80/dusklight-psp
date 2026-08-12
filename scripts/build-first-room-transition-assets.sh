#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] ||
  die "DUSKLIGHT_GAME_IMAGE est requis"
[ -f "$DUSKLIGHT_GAME_IMAGE" ] || die "image de jeu introuvable"
[ ! -L "$DUSKLIGHT_GAME_IMAGE" ] ||
  die "un lien symbolique n'est pas accepté comme image source"

"$SCRIPT_DIR/verify-link-loader-sources.sh"
"$SCRIPT_DIR/build-link-playable-assets.sh"

OUTPUT="$PROJECT_ROOT/build/assets/room-transition"
PASS1="$PROJECT_ROOT/build/assets/room-transition-pass1"
PASS2="$PROJECT_ROOT/build/assets/room-transition-pass2"
for root in "$OUTPUT" "$PASS1" "$PASS2"; do
  mkdir -p "$root/stages/D_MN10/R09" "$root/stages/D_MN10/R02"
done

INVENTORY="$PROJECT_ROOT/build/host/link-loader/room-transition-inventory.log"
env \
  DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
  DUSKLIGHT_INVENTORY_ROOMS=1 \
  http_proxy=http://127.0.0.1:9 \
  https_proxy=http://127.0.0.1:9 \
  ALL_PROXY=http://127.0.0.1:9 \
  NO_PROXY= \
  "$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe" \
  >"$INVENTORY"
[ "$(grep -c '^ROOM_SCLS ' "$INVENTORY")" -ge 20 ] ||
  die "inventaire SCLS insuffisant"
grep -q \
  '^ROOM_SCLS stage=D_MN10 room=09 .* index=1 destination_stage=D_MN10 destination_start=1 destination_room=2 ' \
  "$INVENTORY" || die "sortie source A vers B absente"
grep -q \
  '^ROOM_SCLS stage=D_MN10 room=02 .* index=2 destination_stage=D_MN10 destination_start=2 destination_room=9 ' \
  "$INVENTORY" || die "sortie source B vers A absente"
grep -q \
  '^ROOM_ACTOR stage=D_MN10 room=09 .* name=scnChg params=0xff040001 ' \
  "$INVENTORY" || die "trigger source A absent"
grep -q \
  '^ROOM_ACTOR stage=D_MN10 room=02 .* name=scnChg params=0xff030002 ' \
  "$INVENTORY" || die "trigger source B absent"

convert_room() {
  local destination="$1" room="$2" initial="$3" selected="$4" return_exit="$5"
  env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_ROOM_STAGE=D_MN10 \
    DUSKLIGHT_ROOM_INDEX="$room" \
    DUSKLIGHT_ROOM_INITIAL_START="$initial" \
    DUSKLIGHT_ROOM_NAME="Arbiters_Grounds_R$room" \
    DUSKLIGHT_ROOM_TEXTURE_MAX=128 \
    DUSKLIGHT_DPSC_VERSION=4 \
    DUSKLIGHT_SELECTED_EXIT="$selected" \
    DUSKLIGHT_RETURN_EXIT="$return_exit" \
    DUSKLIGHT_ROOM_SOURCE_ID="D_MN10/R$room/room.kcl" \
    DUSKLIGHT_DPRM_OUTPUT="$destination/room.dprm" \
    DUSKLIGHT_ROOM_DPTX_OUTPUT="$destination/room.dptx" \
    DUSKLIGHT_DPCL_OUTPUT="$destination/room.dpcl" \
    DUSKLIGHT_DPSC_OUTPUT="$destination/room.dpsc" \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    "$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe" \
    >/dev/null
}

for pass in "$PASS1" "$PASS2"; do
  convert_room "$pass/stages/D_MN10/R09" 9 0 1 2
  convert_room "$pass/stages/D_MN10/R02" 2 1 2 1
done

for room in R09 R02; do
  for package in room.dprm room.dptx room.dpcl room.dpsc; do
    cmp \
      "$PASS1/stages/D_MN10/$room/$package" \
      "$PASS2/stages/D_MN10/$room/$package"
    cp \
      "$PASS1/stages/D_MN10/$room/$package" \
      "$OUTPUT/stages/D_MN10/$room/$package"
  done
done

cmake -S "$PROJECT_ROOT/test/room-transition" \
  -B "$PROJECT_ROOT/build/host/room-transition" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/host/room-transition"
"$PROJECT_ROOT/build/host/room-transition/room_transition_host_test" \
  "$OUTPUT/stages/D_MN10/R09" \
  "$OUTPUT/stages/D_MN10/R02"

write_manifest() {
  local room="$1" exit_index="$2" destination="$3" start="$4"
  local directory="$OUTPUT/stages/D_MN10/R$room"
  {
    printf '%s\n' \
      "format=ROOM_TRANSITION_MANIFEST_V1" \
      "network_access=disabled" \
      "stage_id=D_MN10" \
      "room_index=$((10#$room))" \
      "room_layer=0" \
      "source_exit_index=$exit_index" \
      "destination_room=$destination" \
      "destination_start=$start" \
      "trigger_source=scnChg" \
      "trigger_process_id=0x030C" \
      "dpsc_version=4" \
      "texture_max_dimension=128" \
      "texture_fallback=deterministic_nearest_reduction"
    for package in room.dprm room.dptx room.dpcl room.dpsc; do
      printf '%s_size=%s\n' \
        "$package" "$(stat -f %z "$directory/$package")"
      printf '%s_sha256=%s\n' \
        "$package" "$(sha256_file "$directory/$package")"
    done
  } >"$directory/ROOM.MANIFEST"
}

write_manifest 09 1 2 1
write_manifest 02 2 9 2

{
  printf '%s\n' \
    "classification=REAL_BIDIRECTIONAL_ROOM_PAIR_SELECTED" \
    "score=94" \
    "stage_a=D_MN10" \
    "room_a=9" \
    "exit_a=1" \
    "stage_b=D_MN10" \
    "room_b=2" \
    "exit_b=2" \
    "bidirectional=true" \
    "reason=same_stage_source_scls_source_scnchg_valid_spawns_within_budgets"
} >"$OUTPUT/PAIR.SELECTION"
{
  printf '%s\n' \
    "format=ROOM_TRANSITION_FRAMEBUFFER_REFERENCE_V1" \
    "room_a_source=D_MN10/R09" \
    "room_a_spawn=0,450,-4575" \
    "room_a_trigger=-1.58094,449.617,-3883.41" \
    "room_b_source=D_MN10/R02" \
    "room_b_spawn=1.51679,450,-4002.2" \
    "room_b_trigger=-12.9646,448.851,-4843.94" \
    "regions=projected_floor,projected_architecture,converted_texture,link,hud,source_trigger,fade_black,stable_post_fade" \
    "selection=source_geometry_camera_spawn_trigger_derived"
} >"$OUTPUT/FRAMEBUFFER.REFERENCES"

printf '%s\n' \
  "ROOM_TRANSITION_ASSETS_OK pair=D_MN10/R09:D_MN10/R02 deterministic=true"
