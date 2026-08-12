#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

ELF="$(assert_project_path "build/psp/dusklight/dusklight_psp.elf")"
EBOOT="$(assert_project_path "build/psp/dusklight/EBOOT.PBP")"
PACKAGE="$(assert_project_path \
  "artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP")"
MANIFEST="$(assert_project_path \
  "build/assets/dusklight-psp/data/RESOURCE.MANIFEST")"
FUNCTIONAL_CONFIG="$(assert_project_path \
  "test/gu-smoke/ppsspp-software.ini")"
PERFORMANCE_CONFIG="$(assert_project_path \
  "test/link-playable/ppsspp-accelerated.ini")"
OUTPUT="$(assert_project_path "build/reports/PARITY_BUILD_ID.metrics")"
WORKER="$(assert_project_path \
  "tools/macos/dusklight-ppsspp-gui-broker/request_worker.py")"
SCENARIOS="$(assert_project_path "reference/parity/scenarios")"

for required in \
  "$ELF" "$EBOOT" "$PACKAGE/EBOOT.PBP" "$MANIFEST" \
  "$PACKAGE/data/RESOURCE.MANIFEST" "$FUNCTIONAL_CONFIG" \
  "$PERFORMANCE_CONFIG" "$WORKER" "$SCENARIOS/scenarios.toml"; do
  [ -s "$required" ] || die "entrée d'identité absente : $required"
done

commit="$(awk -F= \
  '$1 == "DUSKLIGHT_BUILD_COMMIT:UNINITIALIZED" {print $2}' \
  "$PROJECT_ROOT/build/psp/dusklight/CMakeCache.txt")"
[[ "$commit" =~ ^[0-9a-f]{40}$ ]] || die "commit de build absent du cache"
git cat-file -e "$commit^{commit}" || die "commit de build inconnu : $commit"
strings "$ELF" | awk -v wanted="$commit" \
  '$0 == wanted { found = 1 } END { exit !found }' ||
  die "commit de build absent de l'ELF"

elf_hash="$(sha256_file "$ELF")"
eboot_hash="$(sha256_file "$EBOOT")"
package_eboot_hash="$(sha256_file "$PACKAGE/EBOOT.PBP")"
manifest_hash="$(sha256_file "$MANIFEST")"
package_manifest_hash="$(sha256_file "$PACKAGE/data/RESOURCE.MANIFEST")"
[ "$eboot_hash" = "$package_eboot_hash" ] ||
  die "EBOOT de travail et empaqueté divergents"
[ "$manifest_hash" = "$package_manifest_hash" ] ||
  die "RESOURCE.MANIFEST de travail et empaqueté divergents"

package_set_hash="$(/usr/bin/python3 - "$PACKAGE/data" <<'PY'
import hashlib
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
digest = hashlib.sha256()
for item in sorted(
    (candidate for candidate in root.rglob("*") if candidate.is_file()),
    key=lambda candidate: candidate.relative_to(root).as_posix(),
):
    relative = item.relative_to(root).as_posix().encode("utf-8")
    content = hashlib.sha256(item.read_bytes()).digest()
    digest.update(len(relative).to_bytes(8, "big"))
    digest.update(relative)
    digest.update(content)
print(digest.hexdigest())
PY
)"
functional_config_hash="$(sha256_file "$FUNCTIONAL_CONFIG")"
performance_config_hash="$(sha256_file "$PERFORMANCE_CONFIG")"
worker_hash="$(sha256_file "$WORKER")"
scenario_set_hash="$(/usr/bin/python3 - "$SCENARIOS" <<'PY'
import hashlib
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
digest = hashlib.sha256()
for item in sorted(
    (candidate for candidate in root.rglob("*") if candidate.is_file()),
    key=lambda candidate: candidate.relative_to(root).as_posix(),
):
    relative = item.relative_to(root).as_posix().encode("utf-8")
    content = hashlib.sha256(item.read_bytes()).digest()
    digest.update(len(relative).to_bytes(8, "big"))
    digest.update(relative)
    digest.update(content)
print(digest.hexdigest())
PY
)"

safe_mkdir build/reports
payload="$PROJECT_ROOT/.tmp/parity-build-id-payload.txt"
{
  printf 'psp_source_commit=%s\n' "$commit"
  printf 'eboot_sha256=%s\n' "$eboot_hash"
  printf 'elf_sha256=%s\n' "$elf_hash"
  printf 'resource_manifest_sha256=%s\n' "$manifest_hash"
  printf 'package_set_sha256=%s\n' "$package_set_hash"
  printf 'trace_schema_version=DTRC_V3_1\n'
  printf 'parity_contract_version=DUSKLIGHT_DESKTOP_PSP_PARITY_CONTRACT_V1\n'
  printf 'worker_sha256=%s\n' "$worker_hash"
  printf 'scenario_set_sha256=%s\n' "$scenario_set_hash"
} >"$payload"
build_id="$(sha256_file "$payload")"

{
  printf 'format=DUSKLIGHT_PARITY_BUILD_ID_V2\n'
  printf 'parity_build_id=sha256:%s\n' "$build_id"
  printf 'commit=%s\n' "$commit"
  cat "$payload"
  printf 'functional_config_sha256=%s\n' "$functional_config_hash"
  printf 'performance_config_sha256=%s\n' "$performance_config_hash"
  printf 'working_eboot_sha256=%s\n' "$eboot_hash"
  printf 'packaged_eboot_sha256=%s\n' "$package_eboot_hash"
  printf 'network_used=false\n'
  printf 'error_code=0\n'
} >"$OUTPUT"
cp -- "$OUTPUT" "$PACKAGE/PARITY.BUILD"
rm -f -- "$payload"

printf 'PARITY_BUILD_ID_OK id=sha256:%s commit=%s eboot=%s\n' \
  "$build_id" "$commit" "$eboot_hash"
