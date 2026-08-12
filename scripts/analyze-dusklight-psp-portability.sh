#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] ||
  die "DUSKLIGHT_GAME_IMAGE est requis"
[ -f "$DUSKLIGHT_GAME_IMAGE" ] || die "image de jeu introuvable"
[ ! -L "$DUSKLIGHT_GAME_IMAGE" ] ||
  die "un lien symbolique n'est pas accepté comme image source"

"$SCRIPT_DIR/verify-link-loader-sources.sh"
cmake -S "$PROJECT_ROOT/tools/dusk_link_loader_probe" \
  -B "$PROJECT_ROOT/build/host/link-loader/probe" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/host/link-loader/probe"

safe_mkdir build/reports
INVENTORY="$PROJECT_ROOT/build/reports/actor-portability-inventory.log"
env \
  DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
  DUSKLIGHT_INVENTORY_ROOMS=1 \
  http_proxy=http://127.0.0.1:9 \
  https_proxy=http://127.0.0.1:9 \
  ALL_PROXY=http://127.0.0.1:9 \
  NO_PROXY= \
  "$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe" \
  >"$INVENTORY"

grep -q '^ROOM_ACTOR stage=D_MN10 ' "$INVENTORY" ||
  die "aucun acteur D_MN10 inventorié"

"$PROJECT_ROOT/tools/dusk_psp_actor_portability/dusk_psp_actor_portability.py" \
  --root "$PROJECT_ROOT" \
  --inventory "$INVENTORY" \
  --stage D_MN10 \
  --json "$PROJECT_ROOT/build/reports/actor-portability.json" \
  --csv "$PROJECT_ROOT/build/reports/actor-portability.csv" \
  --markdown "$PROJECT_ROOT/docs/reports/45-actor-portability-overview.md" \
  --manifests-dir "$PROJECT_ROOT/porting/actors" \
  --probe \
  --host-compiler c++ \
  --psp-compiler "$PSPDEV/bin/psp-g++"

printf '%s\n' \
  "DUSKLIGHT_PSP_PORTABILITY_ANALYSIS_OK stage=D_MN10 network=disabled"
