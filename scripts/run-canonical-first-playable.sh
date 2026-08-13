#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

PSP_GXX="$PROJECT_ROOT/.tools/pspdev/bin/psp-g++"
PPSSPP="$PROJECT_ROOT/.tools/ppsspp/PPSSPP-v1.20.4-anylinux-x86_64.AppImage"
PROOF_MARKER='DUSKLIGHT_PSP_FIRST_PLAYABLE_RENDER_OK stage=F_SP108 room=1 start=21 actors=9 render=opaque_unlit frame=1'

if [ ! -x "$PSP_GXX" ] || [ ! -x "$PPSSPP" ]; then
  printf '%s\n' 'Pinned PSPDEV/PPSSPP missing; bootstrapping reproducible tools.'
  bash "$SCRIPT_DIR/bootstrap-repro.sh"
fi

# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

bash "$SCRIPT_DIR/build-canonical-existing-assets.sh"

GAME_DIR="$(assert_project_path artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP)"
EBOOT="$GAME_DIR/EBOOT.PBP"
LOG="$GAME_DIR/first-playable-ppsspp.log"
[ -s "$EBOOT" ] || die "packaged canonical EBOOT absent"
[ ! -L "$EBOOT" ] || die "packaged canonical EBOOT symlink refused"
bash "$SCRIPT_DIR/validate-canonical-first-playable-assets.sh" "$GAME_DIR/data"

printf '%s\n' \
  'DUSKLIGHT_PSP_FIRST_PLAYABLE_RUN_READY' \
  "game_dir=$GAME_DIR" \
  "eboot_sha256=$(sha256_file "$EBOOT")" \
  'controls=UP/DOWN file, X/START confirm, analog move, L/R camera, X action, START pause' \
  'expected_route=file select -> NewGameTransition -> F_SP108/R01/start21 -> visible room+Link' \
  "expected_runtime_marker=$PROOF_MARKER" \
  "ppsspp_log=$LOG" \
  'proof_rule=the runtime marker plus an asset-backed gameplay framebuffer are required for visible first-playable proof'

cd "$GAME_DIR"
set +e
"$PPSSPP" EBOOT.PBP 2>&1 | tee "$LOG"
ppsspp_status=${PIPESTATUS[0]}
set -e

if ! grep -Fq "$PROOF_MARKER" "$LOG"; then
  die "first-playable runtime marker absent; asset-backed gameplay proof not established"
fi
printf 'DUSKLIGHT_PSP_FIRST_PLAYABLE_RUN_OK marker=1 framebuffer=manual-review-required log=%s\n' "$LOG"

if [ "$ppsspp_status" -ne 0 ]; then
  exit "$ppsspp_status"
fi
