#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"

CHECKPOINT="$(assert_project_path \
  ".test-data/ppsspp/checkpoints/idle-lighting/CHECKPOINT.MANIFEST")"
EBOOT="$(assert_project_path "build/psp/dusklight/EBOOT.PBP")"
ELF="$(assert_project_path "build/psp/dusklight/dusklight_psp.elf")"
[ -f "$CHECKPOINT" ] || die "checkpoint idle-lighting absent"
grep -qx \
  'classification=READY_DUSKLIGHT_PSP_IDLE_FIDELITY_REVIEW' \
  "$CHECKPOINT"
grep -qx "eboot_sha256=$(shasum -a 256 "$EBOOT" | awk '{print $1}')" \
  "$CHECKPOINT"
file "$ELF" | grep -q 'ELF 32-bit.*MIPS'
STRINGS="$(assert_project_path ".tmp/idle-lighting-eboot-strings.txt")"
strings "$ELF" >"$STRINGS"
grep -q 'DUSKLIGHT_PSP_LINK_IDLE_FIDELITY_OK' "$STRINGS"
grep -q 'DUSKLIGHT_PSP_LIGHTING_PIPELINE_OK' "$STRINGS"
grep -q 'DUSKLIGHT_PSP_SHADOW_STATE_ISOLATION_OK' "$STRINGS"
printf '%s\n' \
  'IDLE_LIGHTING_RELEASE_OK profile=known_good_unlit lighting=not_accepted'
