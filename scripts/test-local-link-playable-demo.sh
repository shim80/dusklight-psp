#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

TIMEOUT_SECONDS=120
SKIP_PREREQUISITES=false
while [ "$#" -gt 0 ]; do
  case "$1" in
    --timeout)
      shift
      [ "$#" -gt 0 ] || die "--timeout exige une valeur"
      TIMEOUT_SECONDS="$1"
      ;;
    --skip-prerequisites) SKIP_PREREQUISITES=true ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] ||
  die "DUSKLIGHT_GAME_IMAGE est requis"

if [ "$SKIP_PREREQUISITES" = false ]; then
  "$SCRIPT_DIR/test-psp-smokes.sh" --timeout 30
  "$SCRIPT_DIR/test-local-link-demo.sh"
  "$SCRIPT_DIR/run-ppsspp-link-demo.sh" \
    --run --timeout 30
fi
"$SCRIPT_DIR/verify-link-loader-sources.sh"
"$SCRIPT_DIR/test-link-loader-probe.sh"
"$SCRIPT_DIR/build-link-playable-assets.sh"

HOST="$PROJECT_ROOT/build/host/link-playable"
"$HOST/playable_package_host_test" \
  "$PROJECT_ROOT/build/assets/link-playable/link.dpsk" \
  "$PROJECT_ROOT/build/assets/link-playable/link.dptx" \
  "$PROJECT_ROOT/build/assets/link-playable/link.dpan" \
  "$PROJECT_ROOT/build/assets/link-playable/hud.dpui"
"$HOST/playable_runtime_host_test" \
  "$PROJECT_ROOT/build/assets/link-playable/link.dpsk" \
  "$PROJECT_ROOT/build/assets/link-playable/link.dptx" \
  "$PROJECT_ROOT/build/assets/link-playable/link.dpan" \
  "$PROJECT_ROOT/build/assets/link-playable/hud.dpui"

psp-cmake -S "$PROJECT_ROOT/test/link-playable/psp" \
  -B "$PROJECT_ROOT/build/psp/link-playable-demo" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDUSKLIGHT_BUILD_COMMIT="$(git rev-parse HEAD)"
cmake --build "$PROJECT_ROOT/build/psp/link-playable-demo"
"$PROJECT_ROOT/test/link-playable/verify-binary.sh"

"$SCRIPT_DIR/run-ppsspp-link-playable-demo.sh" \
  --run --mode all --timeout "$TIMEOUT_SECONDS"
"$SCRIPT_DIR/package-link-playable-demo.sh"

if ! git diff --quiet -- . ':!docs/reports'; then
  die "les tests ont modifié des fichiers suivis"
fi
printf '%s\n' "LINK_PLAYABLE_LOCAL_TESTS_OK"
