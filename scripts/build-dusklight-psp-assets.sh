#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] ||
  die "DUSKLIGHT_GAME_IMAGE est requis pour régénérer les packages"
[ -f "$DUSKLIGHT_GAME_IMAGE" ] || die "image de jeu locale absente"
"$SCRIPT_DIR/verify-link-loader-sources.sh"

OUTPUT="$(assert_project_path "build/assets/dusklight-psp")"
PASS1="$(assert_project_path ".tmp/dusklight-psp-assets-pass1")"
COMMON="$(assert_project_path "build/assets/link-playable")"
ROOMS="$(assert_project_path "build/assets/room-transition")"
ACTOR="$(assert_project_path "build/assets/original-rendered-actor")"
DYNAMIC_ACTOR="$(assert_project_path "build/assets/original-dynamic-actor")"
DOOR="$(assert_project_path "build/assets/original-door")"
SPINNER_SWITCH="$(assert_project_path "build/assets/spinner-switch")"
TBOX="$(assert_project_path "build/assets/original-tbox")"
STARTUP="$(assert_project_path "build/assets/dusklight-startup")"
FSP102="$(assert_project_path "build/assets/fsp102-environment")"

ensure_startup_assets() {
  local manifest="$STARTUP/STARTUP.MANIFEST"
  local package expected actual
  if [ ! -f "$manifest" ]; then
    "$SCRIPT_DIR/build-dusklight-startup-assets.sh"
  fi
  for package in \
    startup.dpst startup_logos.dpsu title_ui.dpsu \
    file_select.dpsu title_camera.dpcm \
    title_room.dprm title_room.dptx \
    title_logo.dprm title_logo.dptx title_logo.dpan \
    fsp108_room.dprm fsp108_room.dptx \
    fsp108_room.dpcl fsp108_room.dpsc; do
    [ -f "$STARTUP/$package" ] ||
      die "package startup absent : $package"
    expected="$(awk -F= -v key="${package}_sha256" \
      '$1 == key {print $2; exit}' "$manifest")"
    [ -n "$expected" ] ||
      die "empreinte startup absente : $package"
    actual="$(sha256_file "$STARTUP/$package")"
    [ "$actual" = "$expected" ] ||
      die "empreinte startup invalide : $package"
  done
}

ensure_fsp102_environment_assets() {
  if [ ! -s "$FSP102/fsp102_environment.dprm" ] ||
     [ ! -s "$FSP102/fsp102_environment.dptx" ]; then
    "$SCRIPT_DIR/build-fsp102-environment-assets.sh"
  fi
  local expected actual package
  for package in fsp102_environment.dprm fsp102_environment.dptx; do
    expected="$(awk -v file="$package" '$2 ~ ("/" file "$") {print $1}' \
      "$FSP102/FSP102_ENVIRONMENT.SHA256")"
    [ -n "$expected" ] || die "empreinte F_SP102 absente : $package"
    actual="$(sha256_file "$FSP102/$package")"
    [ "$actual" = "$expected" ] ||
      die "empreinte F_SP102 invalide : $package"
  done
}

prepare_tree() {
  local destination="$1"
  safe_mkdir "${destination#"$PROJECT_ROOT"/}/data/common"
  safe_mkdir "${destination#"$PROJECT_ROOT"/}/data/startup"
  safe_mkdir "${destination#"$PROJECT_ROOT"/}/data/ui"
  safe_mkdir "${destination#"$PROJECT_ROOT"/}/data/stages/D_MN10/R09"
  safe_mkdir "${destination#"$PROJECT_ROOT"/}/data/stages/D_MN10/R02"
  safe_mkdir "${destination#"$PROJECT_ROOT"/}/data/stages/F_SP108/R01"
  safe_mkdir "${destination#"$PROJECT_ROOT"/}/data/objects/L4HsMato"
  safe_mkdir "${destination#"$PROJECT_ROOT"/}/data/objects/P_Gear/small"
  safe_mkdir "${destination#"$PROJECT_ROOT"/}/data/objects/P_Gear/large"
  safe_mkdir "${destination#"$PROJECT_ROOT"/}/data/objects/L4R02Gate"
  safe_mkdir "${destination#"$PROJECT_ROOT"/}/data/objects/P_Sswitch/base"
  safe_mkdir "${destination#"$PROJECT_ROOT"/}/data/objects/P_Sswitch/top"
  safe_mkdir "${destination#"$PROJECT_ROOT"/}/data/objects/Dalways/large"
  for package in link.dpsk link.dptx link.dpan hud.dpui; do
    cp -- "$COMMON/$package" "$destination/data/common/$package"
  done
  for package in \
    startup.dpst startup_logos.dpsu title_ui.dpsu \
    file_select.dpsu title_camera.dpcm \
    title_room.dprm title_room.dptx \
    title_logo.dprm title_logo.dptx title_logo.dpan; do
    cp -- "$STARTUP/$package" "$destination/data/startup/$package"
  done
  cp -- "$FSP102/fsp102_environment.dprm" \
    "$destination/data/startup/fsp102_environment.dprm"
  cp -- "$FSP102/fsp102_environment.dptx" \
    "$destination/data/startup/fsp102_environment.dptx"
  printf '%s\n' \
    "owner=global" \
    "source=data/common/hud.dpui" \
    "startup_ui=data/startup/title_ui.dpsu,data/startup/file_select.dpsu" \
    >"$destination/data/ui/PACKAGE.OWNERSHIP"
  for package in room.dprm room.dptx room.dpcl room.dpsc; do
    cp -- "$STARTUP/fsp108_$package" \
      "$destination/data/stages/F_SP108/R01/$package"
  done
  python3 "$PROJECT_ROOT/tools/alpha_material/apply_material_state.py" \
    --manifest "$PROJECT_ROOT/reference/parity/alpha-f-sp108/material-state-v1.json" \
    --dprm "$destination/data/stages/F_SP108/R01/room.dprm" \
    --dptx "$destination/data/stages/F_SP108/R01/room.dptx"
  for room in R09 R02; do
    for package in room.dprm room.dptx room.dpcl room.dpsc ROOM.MANIFEST; do
      cp -- "$ROOMS/stages/D_MN10/$room/$package" \
        "$destination/data/stages/D_MN10/$room/$package"
    done
  done
  cp -- "$ACTOR/model.dprm" \
    "$destination/data/objects/L4HsMato/model.dprm"
  cp -- "$ACTOR/textures.dptx" \
    "$destination/data/objects/L4HsMato/textures.dptx"
  cp -- "$ACTOR/collision.dpcl" \
    "$destination/data/objects/L4HsMato/collision.dpcl"
  cp -- "$DYNAMIC_ACTOR/small/model.dprm" \
    "$destination/data/objects/P_Gear/small/model.dprm"
  cp -- "$DYNAMIC_ACTOR/small/textures.dptx" \
    "$destination/data/objects/P_Gear/small/textures.dptx"
  cp -- "$DYNAMIC_ACTOR/large/model.dprm" \
    "$destination/data/objects/P_Gear/large/model.dprm"
  cp -- "$DYNAMIC_ACTOR/large/textures.dptx" \
    "$destination/data/objects/P_Gear/large/textures.dptx"
  cp -- "$DOOR/model.dprm" \
    "$destination/data/objects/L4R02Gate/model.dprm"
  cp -- "$DOOR/textures.dptx" \
    "$destination/data/objects/L4R02Gate/textures.dptx"
  cp -- "$DOOR/collision.dpcl" \
    "$destination/data/objects/L4R02Gate/collision.dpcl"
  for part in base top; do
    cp -- "$SPINNER_SWITCH/$part/model.dprm" \
      "$destination/data/objects/P_Sswitch/$part/model.dprm"
    cp -- "$SPINNER_SWITCH/$part/textures.dptx" \
      "$destination/data/objects/P_Sswitch/$part/textures.dptx"
    cp -- "$SPINNER_SWITCH/$part/collision.dpcl" \
      "$destination/data/objects/P_Sswitch/$part/collision.dpcl"
  done
  cp -- "$TBOX/model.dprm" \
    "$destination/data/objects/Dalways/large/model.dprm"
  cp -- "$TBOX/textures.dptx" \
    "$destination/data/objects/Dalways/large/textures.dptx"
  cp -- "$TBOX/open.dpan" \
    "$destination/data/objects/Dalways/large/open.dpan"
  cp -- "$TBOX/closed.dpcl" \
    "$destination/data/objects/Dalways/large/closed.dpcl"
  cp -- "$TBOX/open.dpcl" \
    "$destination/data/objects/Dalways/large/open.dpcl"
  {
    printf '%s\n' DUSKLIGHT_RESOURCE_MANIFEST_V1
    manifest_entry "$destination" \
      data/common/link.dpsk SkinnedModel data/common/link.dpsk
    manifest_entry "$destination" \
      data/common/link.dptx TextureArchive data/common/link.dptx
    manifest_entry "$destination" \
      data/common/link.dpan AnimationArchive data/common/link.dpan
    manifest_entry "$destination" \
      data/common/hud.dpui UiArchive data/common/hud.dpui
    for room in R09 R02; do
      manifest_entry "$destination" \
        "data/stages/D_MN10/$room/room.dprm" RoomModel \
        "data/stages/D_MN10/$room/room.dprm"
      manifest_entry "$destination" \
        "data/stages/D_MN10/$room/room.dptx" TextureArchive \
        "data/stages/D_MN10/$room/room.dptx"
      manifest_entry "$destination" \
        "data/stages/D_MN10/$room/room.dpcl" RoomCollision \
        "data/stages/D_MN10/$room/room.dpcl"
      manifest_entry "$destination" \
        "data/stages/D_MN10/$room/room.dpsc" Scene \
        "data/stages/D_MN10/$room/room.dpsc"
    done
    manifest_entry "$destination" \
      "data/stages/F_SP108/R01/room.dprm" RoomModel \
      "data/stages/F_SP108/R01/room.dprm"
    manifest_entry "$destination" \
      "data/stages/F_SP108/R01/room.dptx" TextureArchive \
      "data/stages/F_SP108/R01/room.dptx"
    manifest_entry "$destination" \
      "data/stages/F_SP108/R01/room.dpcl" RoomCollision \
      "data/stages/F_SP108/R01/room.dpcl"
    manifest_entry "$destination" \
      "data/stages/F_SP108/R01/room.dpsc" Scene \
      "data/stages/F_SP108/R01/room.dpsc"
    manifest_entry "$destination" \
      "object:L4HsMato:4:model" StaticModel \
      "data/objects/L4HsMato/model.dprm"
    manifest_entry "$destination" \
      "object:L4HsMato:4:textures" TextureArchive \
      "data/objects/L4HsMato/textures.dptx"
    manifest_entry "$destination" \
      "object:L4HsMato:7:collision" RoomCollision \
      "data/objects/L4HsMato/collision.dpcl"
    manifest_entry "$destination" \
      "object:P_Gear:4:model" StaticModel \
      "data/objects/P_Gear/small/model.dprm"
    manifest_entry "$destination" \
      "object:P_Gear:4:textures" TextureArchive \
      "data/objects/P_Gear/small/textures.dptx"
    manifest_entry "$destination" \
      "object:P_Gear:3:model" StaticModel \
      "data/objects/P_Gear/large/model.dprm"
    manifest_entry "$destination" \
      "object:P_Gear:3:textures" TextureArchive \
      "data/objects/P_Gear/large/textures.dptx"
    manifest_entry "$destination" \
      "object:L4R02Gate:4:model" StaticModel \
      "data/objects/L4R02Gate/model.dprm"
    manifest_entry "$destination" \
      "object:L4R02Gate:4:textures" TextureArchive \
      "data/objects/L4R02Gate/textures.dptx"
    manifest_entry "$destination" \
      "object:L4R02Gate:7:collision" RoomCollision \
      "data/objects/L4R02Gate/collision.dpcl"
    manifest_entry "$destination" \
      "object:P_Sswitch:4:model" StaticModel \
      "data/objects/P_Sswitch/base/model.dprm"
    manifest_entry "$destination" \
      "object:P_Sswitch:4:textures" TextureArchive \
      "data/objects/P_Sswitch/base/textures.dptx"
    manifest_entry "$destination" \
      "object:P_Sswitch:9:collision" RoomCollision \
      "data/objects/P_Sswitch/base/collision.dpcl"
    manifest_entry "$destination" \
      "object:P_Sswitch:5:model" StaticModel \
      "data/objects/P_Sswitch/top/model.dprm"
    manifest_entry "$destination" \
      "object:P_Sswitch:5:textures" TextureArchive \
      "data/objects/P_Sswitch/top/textures.dptx"
    manifest_entry "$destination" \
      "object:P_Sswitch:8:collision" RoomCollision \
      "data/objects/P_Sswitch/top/collision.dpcl"
    manifest_entry "$destination" \
      "object:Dalways:13:model" StaticModel \
      "data/objects/Dalways/large/model.dprm"
    manifest_entry "$destination" \
      "object:Dalways:13:textures" TextureArchive \
      "data/objects/Dalways/large/textures.dptx"
    manifest_entry "$destination" \
      "object:Dalways:8:animation" AnimationArchive \
      "data/objects/Dalways/large/open.dpan"
    manifest_entry "$destination" \
      "object:Dalways:27:collision" RoomCollision \
      "data/objects/Dalways/large/closed.dpcl"
    manifest_entry "$destination" \
      "object:Dalways:28:collision" RoomCollision \
      "data/objects/Dalways/large/open.dpcl"
  } >"$destination/data/RESOURCE.MANIFEST"
}

manifest_entry() {
  local root="$1" identifier="$2" type="$3" path="$4"
  local file="$root/$path" decimal
  [ -f "$file" ] || die "package absent : $path"
  decimal="$(od -An -tu4 -j 12 -N 4 "$file" | tr -d ' ')"
  [ -n "$decimal" ] || die "CRC absent : $path"
  printf '%s|%s|%s|%08X\n' "$identifier" "$type" "$path" "$decimal"
}

ensure_startup_assets
ensure_fsp102_environment_assets
"$SCRIPT_DIR/build-first-room-transition-assets.sh"
rm -rf -- "$ACTOR"
safe_mkdir build/assets/original-rendered-actor
for pass in 1 2; do
  actor_pass="$(assert_project_path \
    ".tmp/original-rendered-actor-pass$pass")"
  rm -rf -- "$actor_pass"
  safe_mkdir ".tmp/original-rendered-actor-pass$pass"
  env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_ORIGINAL_ACTOR_EXPORT=1 \
    DUSKLIGHT_DPRM_OUTPUT="$actor_pass/model.dprm" \
    DUSKLIGHT_ROOM_DPTX_OUTPUT="$actor_pass/textures.dptx" \
    DUSKLIGHT_ACTOR_DPCL_OUTPUT="$actor_pass/collision.dpcl" \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    "$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe" \
    >"$actor_pass/export.log"
done
cmp "$PROJECT_ROOT/.tmp/original-rendered-actor-pass1/model.dprm" \
  "$PROJECT_ROOT/.tmp/original-rendered-actor-pass2/model.dprm"
cmp "$PROJECT_ROOT/.tmp/original-rendered-actor-pass1/textures.dptx" \
  "$PROJECT_ROOT/.tmp/original-rendered-actor-pass2/textures.dptx"
cmp "$PROJECT_ROOT/.tmp/original-rendered-actor-pass1/collision.dpcl" \
  "$PROJECT_ROOT/.tmp/original-rendered-actor-pass2/collision.dpcl"
cp -- "$PROJECT_ROOT/.tmp/original-rendered-actor-pass1/model.dprm" \
  "$ACTOR/model.dprm"
cp -- "$PROJECT_ROOT/.tmp/original-rendered-actor-pass1/textures.dptx" \
  "$ACTOR/textures.dptx"
cp -- "$PROJECT_ROOT/.tmp/original-rendered-actor-pass1/collision.dpcl" \
  "$ACTOR/collision.dpcl"
rm -rf -- "$DYNAMIC_ACTOR"
safe_mkdir build/assets/original-dynamic-actor/small
safe_mkdir build/assets/original-dynamic-actor/large
for pass in 1 2; do
  dynamic_pass="$(assert_project_path \
    ".tmp/original-dynamic-actor-pass$pass")"
  rm -rf -- "$dynamic_pass"
  safe_mkdir ".tmp/original-dynamic-actor-pass$pass/small"
  safe_mkdir ".tmp/original-dynamic-actor-pass$pass/large"
  env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_DYNAMIC_ACTOR_EXPORT=1 \
    DUSKLIGHT_GEAR_SMALL_DPRM_OUTPUT="$dynamic_pass/small/model.dprm" \
    DUSKLIGHT_GEAR_SMALL_DPTX_OUTPUT="$dynamic_pass/small/textures.dptx" \
    DUSKLIGHT_GEAR_LARGE_DPRM_OUTPUT="$dynamic_pass/large/model.dprm" \
    DUSKLIGHT_GEAR_LARGE_DPTX_OUTPUT="$dynamic_pass/large/textures.dptx" \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    "$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe" \
    >"$dynamic_pass/export.log"
done
for type in small large; do
  for package in model.dprm textures.dptx; do
    cmp \
      "$PROJECT_ROOT/.tmp/original-dynamic-actor-pass1/$type/$package" \
      "$PROJECT_ROOT/.tmp/original-dynamic-actor-pass2/$type/$package"
    cp -- \
      "$PROJECT_ROOT/.tmp/original-dynamic-actor-pass1/$type/$package" \
      "$DYNAMIC_ACTOR/$type/$package"
  done
done
rm -rf -- "$DOOR"
safe_mkdir build/assets/original-door
for pass in 1 2; do
  door_pass="$(assert_project_path ".tmp/original-door-pass$pass")"
  rm -rf -- "$door_pass"
  safe_mkdir ".tmp/original-door-pass$pass"
  env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_ORIGINAL_DOOR_EXPORT=1 \
    DUSKLIGHT_DOOR_DPRM_OUTPUT="$door_pass/model.dprm" \
    DUSKLIGHT_DOOR_DPTX_OUTPUT="$door_pass/textures.dptx" \
    DUSKLIGHT_DOOR_DPCL_OUTPUT="$door_pass/collision.dpcl" \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    "$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe" \
    >"$door_pass/export.log"
done
for package in model.dprm textures.dptx collision.dpcl; do
  cmp "$PROJECT_ROOT/.tmp/original-door-pass1/$package" \
    "$PROJECT_ROOT/.tmp/original-door-pass2/$package"
  cp -- "$PROJECT_ROOT/.tmp/original-door-pass1/$package" \
    "$DOOR/$package"
done
rm -rf -- "$SPINNER_SWITCH"
safe_mkdir build/assets/spinner-switch/base
safe_mkdir build/assets/spinner-switch/top
for pass in 1 2; do
  spinner_pass="$(assert_project_path \
    ".tmp/spinner-switch-pass$pass")"
  rm -rf -- "$spinner_pass"
  safe_mkdir ".tmp/spinner-switch-pass$pass/base"
  safe_mkdir ".tmp/spinner-switch-pass$pass/top"
  env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_SPINNER_SWITCH_EXPORT=1 \
    DUSKLIGHT_SPINNER_SWITCH_BASE_DPRM_OUTPUT="$spinner_pass/base/model.dprm" \
    DUSKLIGHT_SPINNER_SWITCH_BASE_DPTX_OUTPUT="$spinner_pass/base/textures.dptx" \
    DUSKLIGHT_SPINNER_SWITCH_BASE_DPCL_OUTPUT="$spinner_pass/base/collision.dpcl" \
    DUSKLIGHT_SPINNER_SWITCH_TOP_DPRM_OUTPUT="$spinner_pass/top/model.dprm" \
    DUSKLIGHT_SPINNER_SWITCH_TOP_DPTX_OUTPUT="$spinner_pass/top/textures.dptx" \
    DUSKLIGHT_SPINNER_SWITCH_TOP_DPCL_OUTPUT="$spinner_pass/top/collision.dpcl" \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    "$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe" \
    >"$spinner_pass/export.log"
done
for part in base top; do
  for package in model.dprm textures.dptx collision.dpcl; do
    cmp "$PROJECT_ROOT/.tmp/spinner-switch-pass1/$part/$package" \
      "$PROJECT_ROOT/.tmp/spinner-switch-pass2/$part/$package"
    cp -- "$PROJECT_ROOT/.tmp/spinner-switch-pass1/$part/$package" \
      "$SPINNER_SWITCH/$part/$package"
  done
done
rm -rf -- "$TBOX"
safe_mkdir build/assets/original-tbox
for pass in 1 2; do
  tbox_pass="$(assert_project_path ".tmp/original-tbox-pass$pass")"
  rm -rf -- "$tbox_pass"
  safe_mkdir ".tmp/original-tbox-pass$pass"
  env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_TBOX_EXPORT=1 \
    DUSKLIGHT_TBOX_DPRM_OUTPUT="$tbox_pass/model.dprm" \
    DUSKLIGHT_TBOX_DPTX_OUTPUT="$tbox_pass/textures.dptx" \
    DUSKLIGHT_TBOX_DPAN_OUTPUT="$tbox_pass/open.dpan" \
    DUSKLIGHT_TBOX_CLOSED_DPCL_OUTPUT="$tbox_pass/closed.dpcl" \
    DUSKLIGHT_TBOX_OPEN_DPCL_OUTPUT="$tbox_pass/open.dpcl" \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    "$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe" \
    >"$tbox_pass/export.log"
done
for package in \
  model.dprm textures.dptx open.dpan closed.dpcl open.dpcl; do
  cmp "$PROJECT_ROOT/.tmp/original-tbox-pass1/$package" \
    "$PROJECT_ROOT/.tmp/original-tbox-pass2/$package"
  cp -- "$PROJECT_ROOT/.tmp/original-tbox-pass1/$package" \
    "$TBOX/$package"
done
rm -rf -- "$PASS1"
safe_mkdir .tmp/dusklight-psp-assets-pass1
prepare_tree "$PASS1"

"$SCRIPT_DIR/build-first-room-transition-assets.sh"
rm -rf -- "$OUTPUT"
safe_mkdir build/assets/dusklight-psp
prepare_tree "$OUTPUT"

diff -qr "$PASS1" "$OUTPUT" >/dev/null ||
  die "génération canonique non déterministe"
for package in \
  "$OUTPUT"/data/common/* \
  "$OUTPUT"/data/stages/D_MN10/R09/room.* \
  "$OUTPUT"/data/stages/D_MN10/R02/room.* \
  "$OUTPUT"/data/stages/F_SP108/R01/room.*; do
  [ -s "$package" ] || die "package vide : $package"
done
entries="$(awk -F'|' 'NR > 1 && NF == 4 {count++} END {print count+0}' \
  "$OUTPUT/data/RESOURCE.MANIFEST")"
[ "$entries" -eq 37 ] || die "RESOURCE.MANIFEST incomplet"
printf 'DUSKLIGHT_PSP_ASSETS_OK entries=%s deterministic=true output=%s\n' \
  "$entries" "${OUTPUT#"$PROJECT_ROOT"/}"
