#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

CHECKPOINT="$(assert_project_path \
  ".test-data/ppsspp/checkpoints/root-anchor")"
MARKER="$CHECKPOINT/LINK_ROOT_ANCHOR.OK"
METRICS="$CHECKPOINT/LINK_ROOT_ANCHOR.METRICS"
MANIFEST="$CHECKPOINT/CHECKPOINT.MANIFEST"
EBOOT="$(assert_project_path "build/psp/dusklight/EBOOT.PBP")"
TOKEN=DUSKLIGHT_PSP_LINK_ROOT_ANCHOR_OK

[ -f "$MARKER" ] && [ -f "$METRICS" ] && [ -f "$MANIFEST" ] ||
  die "checkpoint root-anchor absent; exécuter d'abord psp.canonical.smoke"
[ "$(cat "$MARKER")" = "$TOKEN" ] ||
  die "marqueur root-anchor invalide"
[ "$(wc -c <"$MARKER" | tr -d ' ')" = "${#TOKEN}" ] ||
  die "taille du marqueur root-anchor invalide"

for expected in \
  root_anchor_source_derived=true \
  root_horizontal_motion_removed=false \
  root_horizontal_motion_preserved=true \
  root_horizontal_motion_double_applied=false \
  idle_actor_origin_stable=true \
  idle_root_reference_valid=true \
  idle_pelvis_motion_preserved=true \
  idle_feet_grounded=true \
  walk_root_reference_valid=true \
  run_root_reference_valid=true \
  collision_model_origin_parity=true \
  error_code=0; do
  grep -qx "$expected" "$METRICS" ||
    die "métrique root-anchor absente : $expected"
done

recorded_hash="$(awk -F= '$1=="eboot_sha256"{print $2}' "$MANIFEST")"
actual_hash="$(shasum -a 256 "$EBOOT" | awk '{print $1}')"
[ -n "$recorded_hash" ] && [ "$recorded_hash" = "$actual_hash" ] ||
  die "le checkpoint ne correspond pas au EBOOT courant"
grep -qx 'source_mode=smoke' "$MANIFEST" ||
  die "le checkpoint ne provient pas du smoke canonique"
grep -qx 'presentation=game' "$MANIFEST" ||
  die "le checkpoint ne provient pas de la présentation game"

printf 'DUSKLIGHT_LINK_ROOT_ANCHOR_CHECKPOINT_OK eboot_sha256=%s\n' \
  "$actual_hash"
