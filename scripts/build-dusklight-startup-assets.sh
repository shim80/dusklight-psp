#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] ||
  die "DUSKLIGHT_GAME_IMAGE est requis"
[ -f "$DUSKLIGHT_GAME_IMAGE" ] || die "image locale absente"
[ ! -L "$DUSKLIGHT_GAME_IMAGE" ] ||
  die "un lien symbolique n'est pas accepté comme image source"

"$SCRIPT_DIR/verify-link-loader-sources.sh"
"$SCRIPT_DIR/build-link-loader-probe.sh"

BUILDER_BUILD="$(assert_project_path "build/host/startup-builder")"
safe_mkdir "$BUILDER_BUILD"
cmake -S "$PROJECT_ROOT/tools/dusk_startup_builder" \
  -B "$BUILDER_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILDER_BUILD"

PROBE="$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe"
BUILDER="$BUILDER_BUILD/dusk_startup_builder"
OUTPUT="$(assert_project_path "build/assets/dusklight-startup")"
PASS1="$(assert_project_path "build/assets/dusklight-startup-pass1")"
PASS2="$(assert_project_path "build/assets/dusklight-startup-pass2")"
for root in "$OUTPUT" "$PASS1" "$PASS2"; do
  safe_mkdir "${root#"$PROJECT_ROOT"/}"
done

BASE_FILES=(
  title_room.dprm title_room.dptx
  title_logo.dprm title_logo.dptx title_logo.dpan
  startup_logos.dpsu title_ui.dpsu
)
REUSE_BASE=true
for file in "${BASE_FILES[@]}"; do
  expected=
  if [ -f "$OUTPUT/STARTUP.MANIFEST" ]; then
    expected="$(awk -F= -v key="${file}_sha256" \
      '$1 == key {print $2; exit}' "$OUTPUT/STARTUP.MANIFEST")"
  fi
  if [ ! -f "$OUTPUT/$file" ] ||
     [ -z "$expected" ] ||
     [ "$(sha256_file "$OUTPUT/$file")" != "$expected" ]; then
    REUSE_BASE=false
    break
  fi
done

convert_pass() {
  local destination="$1"
  if [ "$REUSE_BASE" = true ]; then
    for file in "${BASE_FILES[@]}"; do
      cp -- "$OUTPUT/$file" "$destination/$file"
    done
  else
    env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_ROOM_STAGE=F_SP102 \
    DUSKLIGHT_ROOM_INDEX=0 \
    DUSKLIGHT_ROOM_TEXTURE_MAX=128 \
    DUSKLIGHT_DPRM_OUTPUT="$destination/title_room.dprm" \
    DUSKLIGHT_ROOM_DPTX_OUTPUT="$destination/title_room.dptx" \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    "$PROBE" >"$destination/title_room.export.log"
    env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_STARTUP_TITLE_EXPORT=1 \
    DUSKLIGHT_ROOM_TEXTURE_MAX=128 \
    DUSKLIGHT_ROOM_SOURCE_TEXTURE_MAX=2048 \
    DUSKLIGHT_TITLE_DPRM_OUTPUT="$destination/title_logo.dprm" \
    DUSKLIGHT_TITLE_DPTX_OUTPUT="$destination/title_logo.dptx" \
    DUSKLIGHT_TITLE_DPAN_OUTPUT="$destination/title_logo.dpan" \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    "$PROBE" >"$destination/title_logo.export.log"
    env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_STARTUP_UI_EXPORT=1 \
    DUSKLIGHT_STARTUP_LOGOS_DPSU_OUTPUT="$destination/startup_logos.dpsu" \
    DUSKLIGHT_STARTUP_TITLE_UI_DPSU_OUTPUT="$destination/title_ui.dpsu" \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    "$PROBE" >"$destination/startup_ui.export.log"
  fi
  env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_FILE_SELECT_UI_EXPORT=1 \
    DUSKLIGHT_FILE_SELECT_DPSU_OUTPUT="$destination/file_select.dpsu" \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    "$PROBE" >"$destination/file_select.export.log"
  env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_ROOM_STAGE=F_SP108 \
    DUSKLIGHT_ROOM_INDEX=1 \
    DUSKLIGHT_ROOM_INITIAL_START=21 \
    DUSKLIGHT_ROOM_NAME=Ordon_First_Playable \
    DUSKLIGHT_ROOM_TEXTURE_MAX=128 \
    DUSKLIGHT_DPSC_VERSION=4 \
    DUSKLIGHT_ROOM_SOURCE_ID=F_SP108/R01/room.kcl \
    DUSKLIGHT_DPRM_OUTPUT="$destination/fsp108_room.dprm" \
    DUSKLIGHT_ROOM_DPTX_OUTPUT="$destination/fsp108_room.dptx" \
    DUSKLIGHT_DPCL_OUTPUT="$destination/fsp108_room.dpcl" \
    DUSKLIGHT_DPSC_OUTPUT="$destination/fsp108_room.dpsc" \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    "$PROBE" >"$destination/fsp108.export.log"
  "$BUILDER" "$destination/startup.dpst" \
    >"$destination/startup.export.log"
}

convert_pass "$PASS1"
convert_pass "$PASS2"

FILES=(
  title_room.dprm title_room.dptx
  title_logo.dprm title_logo.dptx title_logo.dpan
  startup_logos.dpsu title_ui.dpsu file_select.dpsu
  fsp108_room.dprm fsp108_room.dptx
  fsp108_room.dpcl fsp108_room.dpsc
  startup.dpst
)
for file in "${FILES[@]}"; do
  cmp "$PASS1/$file" "$PASS2/$file"
  cp -- "$PASS1/$file" "$OUTPUT/$file"
done

{
  printf '%s\n' \
    "format=DUSKLIGHT_PSP_STARTUP_ASSETS_V1" \
    "disc_id=GZ2P01" \
    "disc_revision=0" \
    "network_used=false" \
    "source_stage=F_SP102" \
    "source_room=0" \
    "source_layer=10" \
    "source_title_archive=/res/Object/TitlePal.arc" \
    "source_title_model_id=10" \
    "source_title_bck_id=7" \
    "texture_max_dimension=128" \
    "deterministic=true" \
    "title_material_animation_status=unsupported_ids_13_16_19" \
    "startup_ui_status=converted_dpsu1" \
    "startup_warning_source=/res/Layout/LogoPalFr.arc" \
    "startup_title_message=Appuyez sur START" \
    "first_playable_stage=F_SP108" \
    "first_playable_room=1" \
    "first_playable_start=21" \
    "first_playable_actor_policy=source_observed_first_frame_lifecycle_adapters"
  for file in "${FILES[@]}"; do
    printf '%s_size=%s\n' "$file" "$(stat -f %z "$OUTPUT/$file")"
    printf '%s_sha256=%s\n' "$file" "$(sha256_file "$OUTPUT/$file")"
  done
} >"$OUTPUT/STARTUP.MANIFEST"

cmake --build "$PROJECT_ROOT/build/host/canonical-runtime" \
  --target startup_ui_host_test
"$PROJECT_ROOT/build/host/canonical-runtime/startup_ui_host_test" \
  "$OUTPUT/startup_logos.dpsu" "$OUTPUT/title_ui.dpsu" \
  "$OUTPUT/file_select.dpsu"

printf '%s\n' \
  "DUSKLIGHT_STARTUP_ASSETS_OK deterministic=true files=${#FILES[@]} ui=DPSU1"
