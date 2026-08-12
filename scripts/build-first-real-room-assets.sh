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

"$SCRIPT_DIR/build-link-playable-assets.sh"

OUTPUT="$PROJECT_ROOT/build/assets/first-real-room"
FIRST="$PROJECT_ROOT/build/assets/first-real-room-pass1"
SECOND="$PROJECT_ROOT/build/assets/first-real-room-pass2"
mkdir -p "$OUTPUT" "$FIRST" "$SECOND"

convert_once() {
  local destination="$1"
  env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_DPRM_OUTPUT="$destination/room.dprm" \
    DUSKLIGHT_ROOM_DPTX_OUTPUT="$destination/room.dptx" \
    DUSKLIGHT_DPCL_OUTPUT="$destination/room.dpcl" \
    DUSKLIGHT_DPSC_OUTPUT="$destination/room.dpsc" \
    "$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe" \
    >/dev/null
}

convert_once "$FIRST"
convert_once "$SECOND"
for package in room.dprm room.dptx room.dpcl room.dpsc; do
  cmp "$FIRST/$package" "$SECOND/$package"
  cp "$FIRST/$package" "$OUTPUT/$package"
done

cmake -S "$PROJECT_ROOT/test/link-playable" \
  -B "$PROJECT_ROOT/build/host/link-playable" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/host/link-playable"
"$PROJECT_ROOT/build/host/link-playable/room_package_host_test" \
  "$OUTPUT/room.dprm" "$OUTPUT/room.dptx" \
  "$OUTPUT/room.dpcl" "$OUTPUT/room.dpsc"

manifest="$OUTPUT/ROOM.MANIFEST"
{
  printf '%s\n' \
    "format=ROOM_MANIFEST_V1" \
    "stage_id=F_SP110" \
    "room_index=2" \
    "room_layer=0" \
    "dprm_version=1" \
    "dptx_version=1" \
    "dpcl_version=1" \
    "dpsc_version=1" \
    "source_room_triangles=1192" \
    "runtime_room_triangles=1192" \
    "runtime_room_vertices=1726" \
    "room_geometry_decimated=false" \
    "room_non_degenerate_triangles_removed=0" \
    "room_materials=6" \
    "room_draws=6" \
    "room_textures=7" \
    "room_texture_source_bytes=86016" \
    "room_texture_runtime_bytes=311296" \
    "room_texture_edram_bytes=311296" \
    "collision_triangles=413" \
    "collision_vertices=1239" \
    "collision_grid_cells=77" \
    "collision_grid_references=984" \
    "edram_remaining=530304" \
    "tracked_runtime_memory_limit=27262976"
  for package in room.dprm room.dptx room.dpcl room.dpsc; do
    crc="$(od -An -tx4 -j12 -N4 "$OUTPUT/$package" | tr -d ' ')"
    printf '%s_size=%s\n' "$package" "$(stat -f %z "$OUTPUT/$package")"
    printf '%s_crc32=0x%s\n' "$package" "$crc"
    printf '%s_sha256=%s\n' \
      "$package" "$(shasum -a 256 "$OUTPUT/$package" | awk '{print $1}')"
  done
} >"$manifest"

cat >"$OUTPUT/ROOM.PIXEL_REFERENCES" <<'EOF'
format=ROOM_PIXEL_REFERENCES_V1
selection_method=DPSC_camera_projection_and_large_DPRM_surface_regions
edge_margin_pixels=4
point_00=24,20,hud_heart_1
point_01=48,20,hud_heart_2
point_02=72,20,hud_heart_3
point_03=398,24,hud_ruby
point_04=420,24,hud_counter_left
point_05=450,24,hud_counter_right
point_06=170,110,room_left_wall
point_07=300,90,room_rear_wall
point_08=350,160,room_right_wall
point_09=250,220,room_floor_near
point_10=120,180,room_floor_far
point_11=235,145,link_torso
point_12=229,125,link_face
point_13=265,150,link_hand
point_14=240,100,architectural_region
point_15=260,130,interaction_region
EOF

printf '%s\n' "FIRST_REAL_ROOM_ASSETS_OK output=$OUTPUT"
