#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

CONTRACT="$(assert_project_path \
  "docs/design/dusklight-psp-coordinate-space-contract.md")"
HEADER="$(assert_project_path \
  "dusklight-main/platforms/psp/include/dusk/psp/link_fidelity.hpp")"
TEST="$(assert_project_path \
  "test/canonical-runtime/host_link_fidelity_test.cpp")"

for space in \
  SourceWorld SourceActor SourceModel SourceJointLocal SourceJointGlobal \
  SourceCollisionLocal SourceCollisionWorld PspWorld PspActor PspModel \
  PspJointLocal PspJointGlobal PspCollisionLocal PspCollisionWorld \
  CameraView ClipSpace ScreenSpace; do
  grep -Fq "\`$space\`" "$CONTRACT" ||
    die "espace absent du contrat : $space"
done
grep -Fq 'coordinate_contract_valid' "$HEADER" ||
  die "garde de coordonnées absente"
grep -Fq 'constexpr link::SourceToPspWorldTransform invalid[]' "$TEST" ||
  die "matrice négative de coordonnées absente"
grep -Fq 'coordinate_negatives=5' "$TEST" ||
  die "preuve des cinq cas négatifs absente"

printf '%s\n' \
  "COORDINATE_SPACE_CONTRACT_OK spaces=17 negative_cases=5 source_world_scale=1"
