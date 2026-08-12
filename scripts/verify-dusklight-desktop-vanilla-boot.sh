#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

run_id=
while [ "$#" -gt 0 ]; do
  case "$1" in
    --run-id)
      run_id="$2"
      shift 2
      ;;
    *)
      die "argument inconnu : $1"
      ;;
  esac
done
case "$run_id" in
  ''|*[!A-Za-z0-9._-]*) die "identifiant d'exécution invalide : $run_id" ;;
esac

SESSION="$(assert_project_path ".test-data/dusklight-reference/sessions/$run_id")"
RESULT="$SESSION/RESULT"
LOG="$SESSION/stdout.log"
ERR="$SESSION/stderr.log"
HOME_DIR="$SESSION/home"
DVD="$(assert_project_path "game iso/Legend of Zelda, The - Twilight Princess.iso")"
MARKER_DIR="$(assert_project_path ".test-data/dusklight-reference/markers")"
ARTIFACT_DIR="$(assert_project_path "artifacts/dusklight-desktop-reference")"
MARKER="$MARKER_DIR/VANILLA_BOOT.OK"
MANIFEST="$ARTIFACT_DIR/VANILLA_BOOT.MANIFEST"
EXPECTED_CONFIG="$HOME_DIR/Library/Application Support/TwilitRealm/Dusklight/config.json"

[ -f "$RESULT" ] && [ -f "$LOG" ] && [ -f "$ERR" ] ||
  die "preuves d'exécution incomplètes : $run_id"
grep -Fxq 'classification=DESKTOP_PROCESS_OBSERVED' "$RESULT" ||
  die "processus desktop non observé"
grep -Fxq 'variant=vanilla-relwithdebinfo' "$RESULT" ||
  die "variante de boot inattendue"
grep -Fxq 'transport=direct_gui' "$RESULT" ||
  die "transport de boot inattendu"
grep -Fxq 'launch_status=0' "$RESULT" ||
  die "sortie desktop non propre"
grep -Fq "Loading config from '$EXPECTED_CONFIG'" "$LOG" ||
  die "profil desktop non isolé"
grep -Fq 'Compatible surface: true' "$LOG" ||
  die "surface graphique compatible non observée"
grep -Fq 'Backend: Metal' "$LOG" ||
  die "backend Metal non observé"
grep -Fq 'Using surface format BGRA8Unorm, present mode Fifo' "$LOG" ||
  die "surface de présentation non initialisée"
grep -Fq 'Using framebuffer size ' "$LOG" ||
  die "framebuffer non créé"
grep -Fq 'Loaded game disc is GZ2P01' "$LOG" ||
  die "identifiant disque inattendu"
grep -Fq 'fapGm_Execute frame=0' "$LOG" ||
  die "première frame non observée"
grep -Fq 'fapGm_Execute frame=300' "$LOG" ||
  die "stabilité minimale de 300 frames non observée"
grep -Fq 'all mods unloaded' "$LOG" ||
  die "arrêt applicatif propre non observé"
if grep -Eiq '(fatal|panic|segmentation fault|addresssanitizer|uncaught exception)' "$LOG" "$ERR"; then
  die "défaillance fatale détectée dans les journaux"
fi

disc_id="$(dd if="$DVD" bs=1 count=6 2>/dev/null)"
disc_revision="$(od -An -tu1 -j 7 -N 1 "$DVD" | tr -d '[:space:]')"
[ "$disc_id" = GZ2P01 ] || die "header disque inattendu : $disc_id"
[ "$disc_revision" = 0 ] || die "révision disque inattendue : $disc_revision"

safe_mkdir ".test-data/dusklight-reference/markers"
safe_mkdir "artifacts/dusklight-desktop-reference"
{
  printf 'classification=READY_DUSKLIGHT_DESKTOP_VANILLA_EXECUTION\n'
  printf 'run_id=%s\n' "$run_id"
  printf 'variant=vanilla-relwithdebinfo\n'
  printf 'transport=direct_gui\n'
  printf 'profile=%s\n' "${HOME_DIR#"$PROJECT_ROOT"/}"
  printf 'disc_id=%s\n' "$disc_id"
  printf 'disc_revision=%s\n' "$disc_revision"
  printf 'graphics_backend=Metal\n'
  printf 'surface=BGRA8Unorm_Fifo\n'
  printf 'first_frame=observed\n'
  printf 'minimum_frame=300\n'
  printf 'exit=clean\n'
  printf 'visual_capture=unavailable_host_screen_recording_permission\n'
} >"$MANIFEST"
printf 'DUSKLIGHT_DESKTOP_VANILLA_BOOT_OK\n' >"$MARKER"

printf 'DUSKLIGHT_DESKTOP_VANILLA_BOOT_VERIFIED manifest=%s\n' \
  "${MANIFEST#"$PROJECT_ROOT"/}"
