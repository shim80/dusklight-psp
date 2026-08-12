#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

require_project_root
PROBE="$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe"
[ -x "$PROBE" ] || die "probe absent ; exécuter scripts/build-link-loader-probe.sh"
[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] || die "DUSKLIGHT_GAME_IMAGE est requis pour ces tests"
[ -f "$DUSKLIGHT_GAME_IMAGE" ] || die "image locale absente"

TEST_DIR="$(assert_project_path ".tmp/link-loader-negative-tests")"
safe_mkdir "$TEST_DIR"
TRUNCATED="$TEST_DIR/truncated.img"
OUT_OF_BOUNDS="$TEST_DIR/out-of-bounds.img"
POSITIVE_LOG="$TEST_DIR/positive.log"
printf 'RARC' > "$TRUNCATED"
printf 'RARC\000\000\001\000\000\000\000\040\377\377\377\377' > "$OUT_OF_BOUNDS"

env DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" "$PROBE" >"$POSITIVE_LOG" 2>&1
[ "$(grep -c '^HAND_SHAPE_METRICS ' "$POSITIVE_LOG")" -eq 11 ] ||
  die "l'inventaire ne contient pas exactement 11 shapes de mains"
grep -q '^HAND_SHAPE_METRICS index=4 .* name=al_handLG_m .* triangles=248 .* rigid_joints=1 .* probable_side=left$' \
  "$POSITIVE_LOG" ||
  die "la shape neutre gauche 4 ne correspond pas au modèle"
grep -q '^HAND_SHAPE_METRICS index=10 .* name=al_handRE_m .* triangles=250 .* rigid_joints=2 .* probable_side=right$' \
  "$POSITIVE_LOG" ||
  die "la shape neutre droite 10 ne correspond pas au modèle"
grep -q '^NEUTRAL_HAND_SELECTION .* left_hand_shape_index=4 right_hand_shape_index=10 .* unresolved_fields=none$' \
  "$POSITIVE_LOG" ||
  die "la sélection neutre des mains n'est pas prouvée"
printf '%s\n' \
  "LINK_HAND_INVENTORY_OK shapes=11 left=4 right=10 triangles=248,250"

expect_failure() {
  local name="$1"
  shift
  if "$@" >"$TEST_DIR/$name.log" 2>&1; then
    die "test négatif accepté à tort : $name"
  fi
  printf 'NEGATIVE_TEST_OK name=%s\n' "$name"
}

expect_failure missing_env env -u DUSKLIGHT_GAME_IMAGE "$PROBE"
expect_failure nonexistent_path env DUSKLIGHT_GAME_IMAGE="$TEST_DIR/absent.iso" "$PROBE"
expect_failure non_gamecube env DUSKLIGHT_GAME_IMAGE="$TRUNCATED" "$PROBE"
expect_failure wrong_disc_id env DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
  DUSKLIGHT_TEST_EXPECT_DISC_ID=GZ2E01 "$PROBE"
expect_failure wrong_revision env DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
  DUSKLIGHT_TEST_EXPECT_REVISION=1 "$PROBE"
expect_failure missing_kmdl env DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
  DUSKLIGHT_TEST_KMDL_PATH=/res/Object/absent.arc "$PROBE"
expect_failure missing_alanm env DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
  DUSKLIGHT_TEST_ANIMATION_PATH=/res/Object/absent.arc "$PROBE"
expect_failure missing_bmd env DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
  DUSKLIGHT_TEST_MODEL_NAME=absent.bmd "$PROBE"
expect_failure missing_bck env DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
  DUSKLIGHT_TEST_ANIMATION_ID=0xffff "$PROBE"
expect_failure truncated_archive_fixture env \
  DUSKLIGHT_TEST_RARC_FIXTURE="$TRUNCATED" "$PROBE"
expect_failure out_of_bounds_fixture env \
  DUSKLIGHT_TEST_RARC_FIXTURE="$OUT_OF_BOUNDS" "$PROBE"
expect_failure wrong_aurora env \
  DUSKLIGHT_TEST_AURORA_COMMIT=0000000000000000000000000000000000000000 \
  "$SCRIPT_DIR/verify-link-loader-sources.sh"
expect_failure wrong_nod env \
  DUSKLIGHT_TEST_NOD_COMMIT=0000000000000000000000000000000000000000 \
  "$SCRIPT_DIR/verify-link-loader-sources.sh"

printf '%s\n' "LINK_LOADER_NEGATIVE_TESTS_OK count=13"
