#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

TIMEOUT_SECONDS=180
while [ "$#" -gt 0 ]; do
  case "$1" in
    --timeout) shift; [ "$#" -gt 0 ] || die "--timeout exige une valeur"; TIMEOUT_SECONDS="$1" ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] || die "DUSKLIGHT_GAME_IMAGE est requis"

"$SCRIPT_DIR/build-first-real-room-assets.sh"
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
"$HOST/room_negative_matrix_host_test" \
  "$PROJECT_ROOT/build/assets/first-real-room/room.dprm" \
  "$PROJECT_ROOT/build/assets/first-real-room/room.dptx" \
  "$PROJECT_ROOT/build/assets/first-real-room/room.dpcl" \
  "$PROJECT_ROOT/build/assets/first-real-room/room.dpsc"
NEGATIVE_MARKER="$PROJECT_ROOT/.test-data/ppsspp/home/.config/ppsspp/PSP/GAME/DUSKLIGHT_REAL_ROOM/REAL_ROOM.OK"
rm -f -- "$NEGATIVE_MARKER"
[ ! -e "$NEGATIVE_MARKER" ] ||
  die "la matrice hôte ne doit pas produire REAL_ROOM.OK"

psp-cmake -S "$PROJECT_ROOT/test/real-room/psp" \
  -B "$PROJECT_ROOT/build/psp/real-room-demo" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDUSKLIGHT_BUILD_COMMIT="$(git rev-parse HEAD)"
cmake --build "$PROJECT_ROOT/build/psp/real-room-demo"
[ -f "$PROJECT_ROOT/build/psp/real-room-demo/EBOOT.PBP" ] ||
  die "EBOOT.PBP absent"
"$PSPDEV/bin/psp-objdump" -f \
  "$PROJECT_ROOT/build/psp/real-room-demo/dusklight_psp_real_room_demo.elf" |
  grep -q 'architecture: mips' || die "architecture ELF PSP invalide"

"$SCRIPT_DIR/run-ppsspp-first-real-room.sh" \
  --run --mode all --timeout "$TIMEOUT_SECONDS"
"$SCRIPT_DIR/package-first-real-room-demo.sh"
printf '%s\n' "FIRST_REAL_ROOM_LOCAL_TESTS_OK"
