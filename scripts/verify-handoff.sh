#!/usr/bin/env bash
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$ROOT"
fail() { printf 'HANDOFF_FAIL: %s\n' "$*" >&2; exit 1; }
need() { [ -e "$1" ] || fail "missing $1"; }

for p in \
  AGENTS.md README.md .gitignore \
  docs/STATUS.md docs/RESUME.md docs/ROADMAP.md docs/COMMIT_LEDGER.md \
  docs/REPRODUCIBILITY.md docs/EXTERNAL_HANDOFF.md \
  docs/reports/CAUSAL_ITERATIONS.jsonl \
  docs/reports/CAUSAL_PARITY_CHANGELOG.md \
  docs/reports/PARITY_ACTOR_MATRIX.md \
  docs/reports/PARITY_SCENE_MATRIX.md \
  docs/reports/200-p1-getawait-heart-checkpoint.md \
  toolchain/manifest.lock toolchain/REPRODUCIBLE_ENVIRONMENT.md \
  scripts/bootstrap-repro.sh scripts/assemble-source.sh \
  scripts/bootstrap-tools.sh scripts/bootstrap-tools.ps1 \
  test/getawait-heart-probe/CMakeLists.txt test/getawait-heart-probe/main.cpp \
  test/chest-source-full/CMakeLists.txt test/chest-source-full/main.cpp \
  dusklight-main/platforms/psp/CMakeLists.txt; do
  need "$p"
done

for p in \
  dusklight-main/platforms/psp/include/dusk/psp/playable_runtime.hpp \
  dusklight-main/platforms/psp/src/playable_runtime.cpp \
  dusklight-main/platforms/psp/src/original_tbox_bridge.cpp \
  dusklight-main/platforms/psp/src/original_demo_item_bridge.cpp; do
  need "$p"
done

grep -Fq '1bae8a5e6a812217ca33ba533e707ecfa64b1553' scripts/assemble-source.sh || fail 'Dusklight pin mismatch'
grep -Fq '81f12f31d23ec822d8bde2031c91e94c470911eb' scripts/assemble-source.sh || fail 'Aurora pin mismatch'
grep -Fq 'dc18d2ff129f05228b8510ea092d8b24c290a49a' scripts/assemble-source.sh || fail 'Nod pin mismatch'
grep -Fq 'f8f2f2235995836188e5fce2e6225c4b17a47232ea82dd850dbf7a5d99c90587' scripts/bootstrap-repro.sh || fail 'PSPDEV hash mismatch'
grep -Fq '661c098e6b7f7610171a57b7c533ce8bba6f2312b71e76d61e850461973eba21' scripts/bootstrap-repro.sh || fail 'PPSSPP hash mismatch'

command -v python3 >/dev/null || fail 'python3 required for manifest validation'
python3 - <<'PY'
import pathlib, tomllib
p = pathlib.Path('toolchain/manifest.lock')
with p.open('rb') as f:
    data = tomllib.load(f)
ids = {x['id'] for x in data.get('tool', [])}
required = {'pspdev-ubuntu-x86_64','ppsspp-linux-x86_64','pspdev-macos-arm64','ppsspp-macos'}
missing = sorted(required - ids)
if missing:
    raise SystemExit('manifest missing tools: ' + ', '.join(missing))
PY

for s in scripts/bootstrap-repro.sh scripts/assemble-source.sh scripts/bootstrap-tools.sh scripts/verify-handoff.sh; do
  bash -n "$s" || fail "bash syntax: $s"
done

# Recovered historical campaign has no numbered report 11; that absence predates publication.
for n in $(seq 0 193); do
  [ "$n" = 11 ] && continue
  compgen -G "docs/reports/${n}-*.md" >/dev/null || fail "missing recovered report ${n}"
done
for n in 194 195 196 197 198 200; do
  compgen -G "docs/reports/${n}-*.md" >/dev/null || fail "missing P1/publication report ${n}"
done

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  bad=$(git ls-files | grep -Ei '\.(iso|gcm|wbfs|rvz|wad|arc|bmd|bdl|bck|brk|btk|btp|bpk|bti|dzr|dzs|szs|rarc|dprm|dptx|dpan|dpcl|dpsc|dpui|dpsk)$' || true)
  [ -z "$bad" ] || fail "proprietary/generated file tracked: $bad"
fi

printf '%s\n' 'DUSKLIGHT_PSP_HANDOFF_OK'
