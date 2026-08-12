#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

"$SCRIPT_DIR/generate-parity-build-identity.sh"
IDENTITY="$(assert_project_path "build/reports/PARITY_BUILD_ID.metrics")"
PACKAGE_IDENTITY="$(assert_project_path \
  "artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP/PARITY.BUILD")"

cmp -s "$IDENTITY" "$PACKAGE_IDENTITY" ||
  die "identité du paquet différente de l'identité de travail"
grep -Eq '^parity_build_id=sha256:[0-9a-f]{64}$' "$IDENTITY" ||
  die "PARITY_BUILD_ID invalide"
grep -q '^format=DUSKLIGHT_PARITY_BUILD_ID_V2$' "$IDENTITY" ||
  die "format d'identité invalide"
grep -Eq '^psp_source_commit=[0-9a-f]{40}$' "$IDENTITY" ||
  die "commit source PSP invalide"
grep -Eq '^worker_sha256=[0-9a-f]{64}$' "$IDENTITY" ||
  die "hash worker invalide"
grep -Eq '^scenario_set_sha256=[0-9a-f]{64}$' "$IDENTITY" ||
  die "hash scénarios invalide"
grep -q '^trace_schema_version=DTRC_V3_1$' "$IDENTITY" ||
  die "schéma de trace invalide"
grep -q \
  '^parity_contract_version=DUSKLIGHT_DESKTOP_PSP_PARITY_CONTRACT_V1$' \
  "$IDENTITY" || die "contrat de parité invalide"

printf 'PARITY_BUILD_ID_HOST_TEST_OK mixed_builds=0\n'
