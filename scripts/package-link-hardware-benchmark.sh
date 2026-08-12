#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

RUN_LABEL=replace_me
while [ "$#" -gt 0 ]; do
  case "$1" in
    --run-label)
      shift
      [ "$#" -gt 0 ] || die "--run-label exige une valeur"
      RUN_LABEL="$1"
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[[ "$RUN_LABEL" =~ ^[A-Za-z0-9._-]{1,63}$ ]] ||
  die "run_label doit contenir 1 à 63 caractères ASCII sûrs"

EBOOT="$(assert_project_path "build/psp/link-demo/EBOOT.PBP")"
PACKAGE="$(assert_project_path "build/assets/link-demo/link.dpmd")"
OUTPUT="$(assert_project_path "artifacts/psp-link-benchmark")"
GAME="$OUTPUT/PSP/GAME/DUSKLIGHT_LINK_DEMO"
DATA="$GAME/data"
MODE="$GAME/LINK_DEMO.MODE"
CONFIG="$GAME/LINK_BENCH.CONFIG"
README="$OUTPUT/README.txt"
MANIFEST="$OUTPUT/MANIFEST.sha256"

[ -f "$EBOOT" ] || die "EBOOT benchmark absent"
[ -f "$PACKAGE" ] || die "link.dpmd absent"
[ "$(shasum -a 256 "$PACKAGE" | awk '{print $1}')" = \
  b15eb6a5a8e077a888462eb7574282ce8cf730ff5baea7a747863edc76e18fd8 ] ||
  die "le DPMD diffère de la baseline"

safe_mkdir artifacts/psp-link-benchmark/PSP/GAME/DUSKLIGHT_LINK_DEMO/data
for path in \
  "$GAME/EBOOT.PBP" "$DATA/link.dpmd" "$MODE" "$CONFIG" \
  "$README" "$MANIFEST"; do
  if [ -e "$path" ]; then
    [ ! -L "$path" ] || die "lien symbolique refusé : $path"
    rm -f -- "$path"
  fi
done
cp -- "$EBOOT" "$GAME/EBOOT.PBP"
cp -- "$PACKAGE" "$DATA/link.dpmd"
printf '%s' benchmark >"$MODE"
printf 'target=hardware\nrun_label=%s\n' "$RUN_LABEL" >"$CONFIG"
printf '%s\n' \
  "Dusklight PSP — benchmark matériel Link non texturé" \
  "" \
  "1. Copiez le répertoire PSP à la racine de la Memory Stick." \
  "2. Définissez run_label en relançant ce script avec :" \
  "   scripts/package-link-hardware-benchmark.sh --run-label NOM_ASCII" \
  "   Ne modifiez pas ensuite les fichiers couverts par MANIFEST.sha256." \
  "3. Réglez la fréquence depuis le CFW si souhaité. L'EBOOT ne la change pas." \
  "4. Lancez Dusklight Link Diagnostic Demo depuis le menu Jeu." \
  "5. Récupérez LINK_BENCH.OK et LINK_BENCH.METRICS dans le répertoire du jeu." \
  "6. Effectuez au moins trois lancements sans modifier l'EBOOT entre eux." \
  "7. Pour revenir à l'interactif, remplacez le contenu de LINK_DEMO.MODE par interactive." \
  "" \
  "Le paquet n'inclut aucune image de jeu, BMD, BCK, texture, OBJ ou PPM." \
  "Les résultats ne deviennent une preuve matérielle qu'après import et vérification." \
  >"$README"

(
  cd "$OUTPUT"
  shasum -a 256 \
    PSP/GAME/DUSKLIGHT_LINK_DEMO/EBOOT.PBP \
    PSP/GAME/DUSKLIGHT_LINK_DEMO/data/link.dpmd \
    PSP/GAME/DUSKLIGHT_LINK_DEMO/LINK_DEMO.MODE \
    PSP/GAME/DUSKLIGHT_LINK_DEMO/LINK_BENCH.CONFIG \
    README.txt >MANIFEST.sha256
)

find "$OUTPUT" -type f | sort
printf 'LINK_HARDWARE_PACKAGE_OK run_label=%s\n' "$RUN_LABEL"
