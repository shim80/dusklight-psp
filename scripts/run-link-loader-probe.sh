#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

require_project_root
[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] ||
  die "DUSKLIGHT_GAME_IMAGE est requis"
[ -f "$DUSKLIGHT_GAME_IMAGE" ] ||
  die "image absente : $DUSKLIGHT_GAME_IMAGE"

PROBE="$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe"
[ -x "$PROBE" ] || die "probe absent ; exécuter scripts/build-link-loader-probe.sh"
exec "$PROBE"
