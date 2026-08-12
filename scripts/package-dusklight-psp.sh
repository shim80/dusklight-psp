#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

EBOOT="$(assert_project_path "build/psp/dusklight/EBOOT.PBP")"
ASSETS="$(assert_project_path "build/assets/dusklight-psp/data")"
DESTINATION="$(assert_project_path \
  "artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP")"
[ -f "$EBOOT" ] || die "EBOOT canonique absent"
[ -f "$ASSETS/RESOURCE.MANIFEST" ] || die "assets canoniques absents"

safe_mkdir artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP
cp -- "$EBOOT" "$DESTINATION/EBOOT.PBP"
cp -R -- "$ASSETS" "$DESTINATION/"
safe_mkdir artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP/data/ui
[ -f "$DESTINATION/data/ui/PACKAGE.OWNERSHIP" ] ||
  printf '%s\n' \
    "owner=global" \
    "source=data/common/hud.dpui" \
    "startup_ui=data/startup/title_ui.dpsu,data/startup/file_select.dpsu" \
    >"$DESTINATION/data/ui/PACKAGE.OWNERSHIP"
printf '%s' startup >"$DESTINATION/DUSKLIGHT.MODE"
printf '%s' game >"$DESTINATION/DUSKLIGHT.PRESENTATION"
printf '%s' \
  "seed=0x4455534B route=original_scene_exit checkpoints=R09,R02,R09 dynamic_actor=source_record acceleration=none" \
  >"$DESTINATION/DUSKLIGHT.SCENARIO"
data_hash="$(
  cd "$DESTINATION/data"
  find . -type f -print | LC_ALL=C sort |
    while IFS= read -r relative; do
      printf '%s|' "$relative"
      sha256_file "$relative"
    done |
    shasum -a 256 | awk '{print $1}'
)"
{
  printf 'format=DUSKLIGHT_PSP_CANONICAL_PACKAGE_V1\n'
  printf 'package_version=1\n'
  printf 'default_mode=startup\n'
  printf 'resource_manifest_version=1\n'
  printf 'resource_manifest_entries=37\n'
  printf 'resource_manifest_sha256=%s\n' \
    "$(sha256_file "$DESTINATION/data/RESOURCE.MANIFEST")"
  printf 'data_tree_sha256=%s\n' "$data_hash"
  printf 'ownership_common=global\n'
  printf 'ownership_startup=startup\n'
  printf 'ownership_stages=game\n'
  printf 'ownership_ui=global\n'
  printf 'ownership_objects=game\n'
  printf 'modes=startup,interactive,smoke,replay,long,benchmark_v1,benchmark_v2,functional,performance,psp_conservative,trace\n'
  printf 'network_used=false\n'
} >"$DESTINATION/CANONICAL.PACKAGE"
printf 'DUSKLIGHT_PSP_PACKAGE_OK destination=%s\n' "$DESTINATION"
