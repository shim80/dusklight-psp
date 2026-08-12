#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

"$SCRIPT_DIR/generate-visual-build-identity.sh"
IDENTITY="$(assert_project_path build/reports/VISUAL_BUILD_ID.metrics)"
PACKAGE="$(assert_project_path artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP/VISUAL.BUILD)"

cmp -s "$IDENTITY" "$PACKAGE" || die "identité visuelle du paquet divergente"
grep -q '^format=DUSKLIGHT_VISUAL_BUILD_ID_V1$' "$IDENTITY" ||
  die "format VISUAL_BUILD_ID invalide"
grep -Eq '^visual_build_id=sha256:[0-9a-f]{64}$' "$IDENTITY" ||
  die "VISUAL_BUILD_ID invalide"
grep -q '^desktop_trace_schema=DTRC_V3$' "$IDENTITY" ||
  die "schéma desktop invalide"
grep -q '^psp_trace_schema=DTRC_V3_1$' "$IDENTITY" ||
  die "schéma PSP invalide"
grep -q '^render_contract=DUSKLIGHT_PSP_RENDERING_CONTRACT_V2_1$' "$IDENTITY" ||
  die "contrat visuel invalide"
grep -q '^baseline_tag=psp-pre-visual-pipeline-recovery-v1$' "$IDENTITY" ||
  die "tag baseline visuel absent"
source_commit="$(awk -F= '$1 == "source_commit" {print $2}' "$IDENTITY")"
git merge-base --is-ancestor \
  psp-pre-visual-pipeline-recovery-v1 "$source_commit" ||
  die "le commit source ne descend pas du baseline visuel"

printf 'VISUAL_BUILD_ID_HOST_TEST_OK mixed_builds=0 capture_identity_required=true\n'
