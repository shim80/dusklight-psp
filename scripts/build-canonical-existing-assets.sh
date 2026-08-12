#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

ASSETS="$(assert_project_path build/assets/dusklight-psp/data)"
MANIFEST="$ASSETS/RESOURCE.MANIFEST"
[ -s "$MANIFEST" ] || die "assets canoniques locaux absents"
[ ! -L "$ASSETS" ] || die "répertoire assets symbolique refusé"
runtime_commit="$(git log -1 --format=%H -- \
  dusklight-main/platforms/psp \
  test/dusklight-psp \
  test/room-transition/main.cpp)"
[[ "$runtime_commit" =~ ^[0-9a-f]{40}$ ]] ||
  die "commit source runtime introuvable"

psp-cmake -S "$PROJECT_ROOT/test/dusklight-psp" \
  -B "$PROJECT_ROOT/build/psp/dusklight" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDUSKLIGHT_BUILD_COMMIT="$runtime_commit"
cmake --build "$PROJECT_ROOT/build/psp/dusklight"
"$PSPDEV/bin/psp-objdump" -f \
  "$PROJECT_ROOT/build/psp/dusklight/dusklight_psp.elf" |
  grep -q 'architecture: mips:allegrex' || die "ELF non Allegrex"
"$SCRIPT_DIR/package-dusklight-psp.sh"
"$SCRIPT_DIR/generate-parity-build-identity.sh"
"$SCRIPT_DIR/test-parity-build-identity.sh"

printf 'DUSKLIGHT_PSP_EXISTING_ASSETS_BUILD_OK network_used=false\n'
