#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

TIMEOUT_SECONDS=180
SKIP_PREREQUISITES=false
while [ "$#" -gt 0 ]; do
  case "$1" in
    --timeout) shift; [ "$#" -gt 0 ] || die "--timeout exige une valeur"; TIMEOUT_SECONDS="$1" ;;
    --skip-prerequisites) SKIP_PREREQUISITES=true ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] || die "DUSKLIGHT_GAME_IMAGE est requis"

if [ "$SKIP_PREREQUISITES" = false ]; then
  "$SCRIPT_DIR/test-psp-smokes.sh" --timeout 30
  "$SCRIPT_DIR/test-local-link-playable-demo.sh" --timeout 120
  "$SCRIPT_DIR/test-first-real-room-demo.sh" --timeout 180
fi
"$SCRIPT_DIR/verify-link-loader-sources.sh"
"$SCRIPT_DIR/build-first-real-actor-assets.sh"

psp-cmake -S "$PROJECT_ROOT/test/real-actor/psp" \
  -B "$PROJECT_ROOT/build/psp/real-actor-demo" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDUSKLIGHT_BUILD_COMMIT="$(git rev-parse HEAD)"
cmake --build "$PROJECT_ROOT/build/psp/real-actor-demo"

ELF="$PROJECT_ROOT/build/psp/real-actor-demo/dusklight_psp_real_actor_demo.elf"
ELF_SYMBOLS="$PROJECT_ROOT/build/psp/real-actor-demo/actor-symbols.txt"
ELF_STRINGS="$PROJECT_ROOT/build/psp/real-actor-demo/actor-strings.txt"
ELF_HEADERS="$PROJECT_ROOT/build/psp/real-actor-demo/actor-headers.txt"
[ -f "$PROJECT_ROOT/build/psp/real-actor-demo/EBOOT.PBP" ] ||
  die "EBOOT acteur absent"
"$PSPDEV/bin/psp-objdump" -f "$ELF" >"$ELF_HEADERS"
"$PSPDEV/bin/psp-nm" -C "$ELF" >"$ELF_SYMBOLS"
"$PSPDEV/bin/psp-strings" "$ELF" >"$ELF_STRINGS"
grep -q 'architecture: mips' "$ELF_HEADERS" ||
  die "architecture ELF acteur invalide"
grep -q 'initialize_actor_system' "$ELF_SYMBOLS" ||
  die "ActorSystem absent du binaire"
grep -q 'draw_actor_backend' "$ELF_SYMBOLS" ||
  die "backend acteur absent du binaire"
if grep -Eq 'Aurora|libnod|J3DModelData|fopAcM_ct' "$ELF_STRINGS"; then
  die "dépendance interdite trouvée dans l’EBOOT acteur"
fi
[ "$(stat -f %z "$ELF")" -le 6291456 ] ||
  die "budget mémoire binaire acteur dépassé"
"$PSPDEV/bin/psp-objdump" -h "$ELF" >"$ELF_HEADERS"
grep -q '.debug_info' "$ELF_HEADERS" || die "RelWithDebInfo non confirmé"
[ "$(xxd -p "$PROJECT_ROOT/build/assets/first-real-actor/room.dpsc" |
  tr -d '\n' | grep -c '01040408')" -gt 0 ] ||
  die "0x08040401 absent du package scène"
[ "$(xxd -p "$PROJECT_ROOT/build/assets/first-real-actor/room.dpsc" |
  tr -d '\n' | grep -c 'ff040408')" -gt 0 ] ||
  die "0x080404ff absent du package scène"
! grep -q 'sceGu' "$PROJECT_ROOT/test/real-room/main.cpp" ||
  die "main contient un appel GU direct"
[ "$(xxd -p -l 4 "$PROJECT_ROOT/build/psp/real-actor-demo/EBOOT.PBP")" = \
  "00504250" ] || die "magie PBP invalide"

"$SCRIPT_DIR/run-ppsspp-first-real-actor.sh" \
  --run --mode all --timeout "$TIMEOUT_SECONDS"
"$SCRIPT_DIR/package-first-real-actor-demo.sh"
[ -z "$(git status --short)" ] ||
  die "le contrôle Git final trouve des changements suivis"
printf '%s\n' "FIRST_REAL_ACTOR_LOCAL_TESTS_OK"
