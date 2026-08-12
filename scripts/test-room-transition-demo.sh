#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

TIMEOUT_SECONDS=300
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
[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] || die "DUSKLIGHT_GAME_IMAGE est requis"
safe_mkdir logs/room-transition

if [ "$SKIP_PREREQUISITES" = false ]; then
  "$SCRIPT_DIR/test-psp-smokes.sh" --timeout 30
  "$SCRIPT_DIR/test-local-link-playable-demo.sh" --timeout 120
  "$SCRIPT_DIR/test-first-real-room-demo.sh" --timeout 180
  "$SCRIPT_DIR/test-first-real-actor-demo.sh" --timeout 240
fi
"$SCRIPT_DIR/verify-link-loader-sources.sh"
"$SCRIPT_DIR/build-first-room-transition-assets.sh"

INVENTORY="$PROJECT_ROOT/build/host/link-loader/room-transition-inventory.log"
SELECTION="$PROJECT_ROOT/build/assets/room-transition/PAIR.SELECTION"
grep -q '^ROOM_ARCHIVE_INVENTORY_OK ' "$INVENTORY" ||
  die "audit SCLS incomplet"
grep -q '^classification=REAL_BIDIRECTIONAL_ROOM_PAIR_SELECTED$' "$SELECTION" ||
  die "sélection de paire invalide"

HOST_TEST="$PROJECT_ROOT/build/host/room-transition/room_transition_host_test"
"$HOST_TEST" \
  "$PROJECT_ROOT/build/assets/room-transition/stages/D_MN10/R09" \
  "$PROJECT_ROOT/build/assets/room-transition/stages/D_MN10/R02" |
  tee "$PROJECT_ROOT/logs/room-transition/host-matrix.log"
grep -q 'negative_cases=37' \
  "$PROJECT_ROOT/logs/room-transition/host-matrix.log" ||
  die "matrice négative hôte incomplète"

psp-cmake -S "$PROJECT_ROOT/test/room-transition/psp" \
  -B "$PROJECT_ROOT/build/psp/room-transition-demo" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDUSKLIGHT_BUILD_COMMIT="$(git rev-parse HEAD)"
cmake --build "$PROJECT_ROOT/build/psp/room-transition-demo"

ELF="$PROJECT_ROOT/build/psp/room-transition-demo/dusklight_psp_room_transition_demo.elf"
PBP="$PROJECT_ROOT/build/psp/room-transition-demo/EBOOT.PBP"
HEADERS="$PROJECT_ROOT/build/psp/room-transition-demo/transition-headers.txt"
SYMBOLS="$PROJECT_ROOT/build/psp/room-transition-demo/transition-symbols.txt"
STRINGS="$PROJECT_ROOT/build/psp/room-transition-demo/transition-strings.txt"
[ -f "$ELF" ] && [ -f "$PBP" ] || die "binaire transition absent"
"$PSPDEV/bin/psp-objdump" -f "$ELF" >"$HEADERS"
"$PSPDEV/bin/psp-nm" -C "$ELF" >"$SYMBOLS"
"$PSPDEV/bin/psp-strings" "$ELF" >"$STRINGS"
grep -q 'architecture: mips' "$HEADERS" || die "ELF non MIPS"
for symbol in \
  ' main$' 'module_info' 'sceCtrl' \
  'PspStageRuntime' 'RoomResourceManager' 'RoomTransitionController' \
  'validate_dpsc' 'read_dpsc_exit_v3' 'read_dpsc_trigger_v3' \
  'read_dpsc_spawn_v3' 'initialize_actor_system' \
  'initialize_collision' 'render_black_transition_frame' \
  'initialize_runtime' 'draw_ui'; do
  grep -q "$symbol" "$SYMBOLS" ||
    die "symbole transition absent : $symbol"
done
if grep -Eqi \
  'Aurora|libnod|J3DModel|\\.bmd|\\.bck|\\.kcl|\\.dzr' \
  "$STRINGS"; then
  die "dépendance ou donnée brute interdite dans le binaire"
fi
if grep -Eqi 'PPSSPP|Aurora|libnod|J3DModel' "$SYMBOLS"; then
  die "API ou symbole hôte interdit dans le binaire"
fi
grep -q 'destination.destination_stage' \
  "$PROJECT_ROOT/test/room-transition/main.cpp" ||
  die "destination non lue depuis DPSC"
! grep -q 'sceGu' "$PROJECT_ROOT/test/room-transition/main.cpp" ||
  die "appel GU direct depuis main"
"$PSPDEV/bin/psp-objdump" -h "$ELF" >"$HEADERS"
grep -q '.debug_info' "$HEADERS" || die "RelWithDebInfo non confirmé"
[ "$(xxd -p -l 4 "$PBP")" = "00504250" ] || die "magie PBP invalide"
[ "$(stat -f %z "$ELF")" -le 6291456 ] || die "ELF trop volumineux"

"$SCRIPT_DIR/run-ppsspp-room-transition.sh" \
  --run --mode all --timeout "$TIMEOUT_SECONDS"
"$SCRIPT_DIR/package-room-transition-demo.sh"
[ -z "$(git status --short --untracked-files=no)" ] ||
  die "le contrôle Git final trouve des changements suivis"
printf '%s\n' \
  "ROOM_TRANSITION_LOCAL_TESTS_OK negative_cases=43 transitions=40"
