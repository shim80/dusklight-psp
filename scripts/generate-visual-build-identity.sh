#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

PARITY="$(assert_project_path build/reports/PARITY_BUILD_ID.metrics)"
RESUME="$(assert_project_path reference/parity/CAUSAL_RESUME_STATE.json)"
PACKAGE="$(assert_project_path artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP)"
WORKER="$(assert_project_path tools/macos/dusklight-ppsspp-gui-broker/request_worker.py)"
COLLECTOR="$(assert_project_path tools/macos/dusklight-ppsspp-gui-broker/artifact_collector.py)"
DESKTOP_GIT="$(assert_project_path .cache/provenance/dusklight.git)"
OUTPUT="$(assert_project_path build/reports/VISUAL_BUILD_ID.metrics)"
PAYLOAD="$(assert_project_path .tmp/visual-build-id-payload.txt)"

for required in "$PARITY" "$RESUME" "$WORKER" "$COLLECTOR" \
  "$PACKAGE/EBOOT.PBP" "$PACKAGE/BENCHMARK_V2_1.METRICS"; do
  [ -s "$required" ] || die "entrée d'identité visuelle absente : $required"
done

metric() {
  local key="$1"
  awk -F= -v key="$key" '$1 == key {print substr($0, length(key) + 2); exit}' \
    "$PARITY"
}

source_commit="$(metric psp_source_commit)"
eboot_sha256="$(metric eboot_sha256)"
elf_sha256="$(metric elf_sha256)"
resource_manifest_sha256="$(metric resource_manifest_sha256)"
package_set_sha256="$(metric package_set_sha256)"
scenario_set_sha256="$(metric scenario_set_sha256)"
desktop_oracle_commit="$(jq -r '.desktop_oracle_commit // empty' "$RESUME")"
worker_sha256="$(sha256_file "$WORKER")"
collector_sha256="$(sha256_file "$COLLECTOR")"

[[ "$source_commit" =~ ^[0-9a-f]{40}$ ]] || die "commit source PSP invalide"
git cat-file -e "$source_commit^{commit}" ||
  die "commit source PSP inconnu : $source_commit"
[[ "$desktop_oracle_commit" =~ ^[0-9a-f]{40}$ ]] ||
  die "commit oracle desktop invalide"
[ -d "$DESKTOP_GIT/objects" ] || die "miroir Git oracle desktop absent"
git --git-dir="$DESKTOP_GIT" cat-file -e "$desktop_oracle_commit^{commit}" ||
  die "commit oracle desktop inconnu : $desktop_oracle_commit"
for value in "$eboot_sha256" "$elf_sha256" "$resource_manifest_sha256" \
  "$package_set_sha256" "$scenario_set_sha256" "$worker_sha256" \
  "$collector_sha256"; do
  [[ "$value" =~ ^[0-9a-f]{64}$ ]] || die "hash d'identité visuelle invalide"
done

[ "$(sha256_file "$PACKAGE/EBOOT.PBP")" = "$eboot_sha256" ] ||
  die "EBOOT empaqueté différent de l'identité de parité"
grep -q '^contract_id=DUSKLIGHT_PSP_RENDERING_CONTRACT_V2_1$' \
  "$PACKAGE/BENCHMARK_V2_1.METRICS" || die "contrat v2.1 absent du paquet"

safe_mkdir build/reports
{
  printf 'source_commit=%s\n' "$source_commit"
  printf 'eboot_sha256=%s\n' "$eboot_sha256"
  printf 'elf_sha256=%s\n' "$elf_sha256"
  printf 'resource_manifest_sha256=%s\n' "$resource_manifest_sha256"
  printf 'package_set_sha256=%s\n' "$package_set_sha256"
  printf 'desktop_oracle_commit=%s\n' "$desktop_oracle_commit"
  printf 'desktop_trace_schema=DTRC_V3\n'
  printf 'psp_trace_schema=DTRC_V3_1\n'
  printf 'render_contract=DUSKLIGHT_PSP_RENDERING_CONTRACT_V2_1\n'
  printf 'scenario_set_sha256=%s\n' "$scenario_set_sha256"
  printf 'worker_sha256=%s\n' "$worker_sha256"
  printf 'collector_sha256=%s\n' "$collector_sha256"
} >"$PAYLOAD"
visual_build_id="$(sha256_file "$PAYLOAD")"

{
  printf 'format=DUSKLIGHT_VISUAL_BUILD_ID_V1\n'
  printf 'visual_build_id=sha256:%s\n' "$visual_build_id"
  cat "$PAYLOAD"
  printf 'baseline_tag=psp-pre-visual-pipeline-recovery-v1\n'
  printf 'network_used=false\n'
  printf 'error_code=0\n'
} >"$OUTPUT"
cp -- "$OUTPUT" "$PACKAGE/VISUAL.BUILD"
rm -f -- "$PAYLOAD"

printf 'VISUAL_BUILD_ID_OK id=sha256:%s source=%s eboot=%s\n' \
  "$visual_build_id" "$source_commit" "$eboot_sha256"
