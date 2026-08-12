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
"$SCRIPT_DIR/build-first-real-room-assets.sh"

OUTPUT="$PROJECT_ROOT/build/assets/first-real-actor"
FIRST="$PROJECT_ROOT/build/assets/first-real-actor-pass1"
SECOND="$PROJECT_ROOT/build/assets/first-real-actor-pass2"
mkdir -p "$OUTPUT" "$FIRST" "$SECOND"

convert_scene() {
  local destination="$1"
  env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_DPSC_VERSION=2 \
    DUSKLIGHT_DPSC_OUTPUT="$destination/room.dpsc" \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    "$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe" \
    >/dev/null
}

convert_scene "$FIRST"
convert_scene "$SECOND"
cmp "$FIRST/room.dpsc" "$SECOND/room.dpsc"
cp "$FIRST/room.dpsc" "$OUTPUT/room.dpsc"

cmake -S "$PROJECT_ROOT/test/real-actor" \
  -B "$PROJECT_ROOT/build/host/real-actor" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/host/real-actor"
"$PROJECT_ROOT/build/host/real-actor/real_actor_runtime_host_test" \
  "$PROJECT_ROOT/build/assets/first-real-room/room.dpsc" \
  "$OUTPUT/room.dpsc"

manifest="$OUTPUT/ACTOR.MANIFEST"
{
  printf '%s\n' \
    "format=ACTOR_MANIFEST_V1" \
    "network_access=disabled" \
    "stage_id=F_SP110" \
    "room_index=2" \
    "room_layer=0" \
    "dpsc_version=2" \
    "source_record_count=3" \
    "supported_record_count=2" \
    "unsupported_record_count=1" \
    "geyser_process_id=0x0167" \
    "geyser_profile=g_profile_Obj_Geyser" \
    "geyser_class=daObjGeyser_c" \
    "geyser_0_source_index=0" \
    "geyser_0_params=0x08040401" \
    "geyser_0_position=4630.31,1350,3707.59" \
    "geyser_0_rotation=511,15473,0" \
    "geyser_0_scale=0.6,0.6,0.6" \
    "geyser_1_source_index=1" \
    "geyser_1_params=0x080404ff" \
    "geyser_1_position=4898.74,1150.57,3032.88" \
    "geyser_1_rotation=255,-14563,0" \
    "geyser_1_scale=1,1,1" \
    "visual_fallback=procedural_oriented_jet_and_particles" \
    "actor_texture_edram_bytes=0" \
    "actor_dynamic_buffer_limit=24576" \
    "actor_capacity=16" \
    "particle_capacity=96"
  printf 'room.dpsc_size=%s\n' "$(stat -f %z "$OUTPUT/room.dpsc")"
  printf 'room.dpsc_sha256=%s\n' \
    "$(shasum -a 256 "$OUTPUT/room.dpsc" | awk '{print $1}')"
} >"$manifest"

printf '%s\n' "FIRST_REAL_ACTOR_ASSETS_OK output=$OUTPUT deterministic=true"
