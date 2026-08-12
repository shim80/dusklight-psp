#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

require_project_root
[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] ||
  die "DUSKLIGHT_GAME_IMAGE est requis"
[ -f "$DUSKLIGHT_GAME_IMAGE" ] || die "image locale absente"
[ ! -L "$DUSKLIGHT_GAME_IMAGE" ] ||
  die "un lien symbolique n'est pas accepté comme image source"

PROBE="$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe"
[ -x "$PROBE" ] ||
  die "probe absent ; exécuter scripts/build-link-loader-probe.sh"

SOURCE="$PROJECT_ROOT/dusklight-main"
REPORT_DIR="$(assert_project_path "build/reports/startup")"
safe_mkdir "$REPORT_DIR"

assert_source_symbol() {
  local file="$1" pattern="$2" label="$3"
  rg -n --fixed-strings "$pattern" "$SOURCE/$file" \
    >"$REPORT_DIR/$label.source.txt" ||
    die "preuve source absente : $file :: $pattern"
}

assert_source_symbol src/d/d_s_logo.cpp \
  "dComIfG_changeOpeningScene(this, fpcNm_OPENING_SCENE_e)" \
  logo_to_opening
assert_source_symbol src/d/d_com_inf_game.cpp \
  'dComIfGp_setNextStage("F_SP102", 100, 0, 10)' \
  opening_stage
assert_source_symbol src/d/actor/d_a_title.cpp \
  "fpcNm_NAME_SCENE_e" \
  title_to_name
assert_source_symbol src/d/d_s_name.cpp \
  'dComIfGp_setNextStage("F_SP108", 21, 1, 13)' \
  new_game_stage

run_probe() {
  env \
    DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    DUSKLIGHT_TEST_EXPECT_DISC_ID=GZ2P01 \
    DUSKLIGHT_TEST_EXPECT_REVISION=0 \
    http_proxy=http://127.0.0.1:9 \
    https_proxy=http://127.0.0.1:9 \
    ALL_PROXY=http://127.0.0.1:9 \
    NO_PROXY= \
    "$@"
}

run_probe env DUSKLIGHT_LIST_DVD_PATH=/res/Stage/F_SP102 "$PROBE" \
  >"$REPORT_DIR/f_sp102.dvd.txt"
run_probe env DUSKLIGHT_LIST_RARC_PATH=/res/Object/LogoPal.arc "$PROBE" \
  >"$REPORT_DIR/logo_pal.rarc.txt"
run_probe env DUSKLIGHT_LIST_RARC_PATH=/res/Object/TitlePal.arc "$PROBE" \
  >"$REPORT_DIR/title_pal.rarc.txt"
run_probe env DUSKLIGHT_LIST_RARC_PATH=/res/Layout/Title2D.arc "$PROBE" \
  >"$REPORT_DIR/title_2d.rarc.txt"
run_probe env DUSKLIGHT_LIST_RARC_PATH=/res/Object/fileSel.arc "$PROBE" \
  >"$REPORT_DIR/file_select.rarc.txt"
run_probe env DUSKLIGHT_LIST_DVD_PATH=/Movie "$PROBE" \
  >"$REPORT_DIR/movie.dvd.txt"

grep -q 'path=/res/Stage/F_SP102/R00_00.arc ' \
  "$REPORT_DIR/f_sp102.dvd.txt" || die "room titre absente"
grep -q 'name=nintendo_376x104.bti ' \
  "$REPORT_DIR/logo_pal.rarc.txt" || die "logo Nintendo absent"
grep -q 'name=titlelogo_tm.bmd ' \
  "$REPORT_DIR/title_pal.rarc.txt" || die "modèle titre absent"
grep -q 'name=zelda_press_start.blo ' \
  "$REPORT_DIR/title_2d.rarc.txt" || die "layout Press Start absent"

printf '%s\n' \
  "STARTUP_FLOW_AUDIT_OK disc=GZ2P01 revision=0" \
  "STARTUP_FLOW_SOURCE_OK opening=F_SP102 title_to_name=true new_game=F_SP108" \
  "STARTUP_FLOW_REPORT_DIR=$REPORT_DIR"
