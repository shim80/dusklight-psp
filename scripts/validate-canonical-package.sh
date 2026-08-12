#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

SEAL=false
case "${1:-}" in
  "") ;;
  --seal) SEAL=true ;;
  *) die "usage: validate-canonical-package.sh [--seal]" ;;
esac

PACKAGE="$(assert_project_path \
  "artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP")"
DATA="$PACKAGE/data"
MANIFEST="$DATA/RESOURCE.MANIFEST"
CONTRACT="$PACKAGE/CANONICAL.PACKAGE"

[ -f "$PACKAGE/EBOOT.PBP" ] || die "EBOOT canonique absent"
[ "$(cat "$PACKAGE/DUSKLIGHT.MODE")" = startup ] ||
  die "le package utilisateur ne démarre pas en mode startup"
[ "$(cat "$PACKAGE/DUSKLIGHT.PRESENTATION")" = game ] ||
  die "présentation utilisateur non game"
[ -f "$MANIFEST" ] && [ -f "$CONTRACT" ] ||
  die "contrat ou manifeste canonique absent"
for directory in common startup stages ui objects; do
  [ -d "$DATA/$directory" ] || die "répertoire union absent : data/$directory"
done
for required in \
  startup/startup.dpst startup/startup_logos.dpsu \
  startup/title_ui.dpsu startup/file_select.dpsu \
  stages/F_SP108/R01/room.dprm stages/F_SP108/R01/room.dptx \
  stages/F_SP108/R01/room.dpcl stages/F_SP108/R01/room.dpsc \
  stages/D_MN10/R09/room.dpsc stages/D_MN10/R02/room.dpsc \
  objects/L4HsMato/collision.dpcl \
  common/link.dpsk common/link.dptx common/link.dpan common/hud.dpui \
  ui/PACKAGE.OWNERSHIP; do
  [ -f "$DATA/$required" ] || die "ressource union absente : $required"
done

[ "$(sed -n '1p' "$MANIFEST")" = DUSKLIGHT_RESOURCE_MANIFEST_V1 ] ||
  die "version du manifeste de ressources invalide"
entries="$(awk 'NR > 1 && NF {count++} END {print count+0}' "$MANIFEST")"
[ "$entries" -eq 37 ] || die "nombre d'entrées inattendu : $entries"
awk -F'|' '
  NR == 1 {next}
  NF != 4 || seen_id[$1]++ || seen_path[$3]++ {exit 1}
  END {if (NR != 38) exit 1}
' "$MANIFEST" || die "clé ambiguë ou chemin dupliqué dans le manifeste"

expected_manifest_hash="$(
  awk -F= '$1 == "resource_manifest_sha256" {print $2; exit}' "$CONTRACT"
)"
[ "$expected_manifest_hash" = "$(sha256_file "$MANIFEST")" ] ||
  die "empreinte du manifeste canonique invalide"
expected_data_hash="$(
  awk -F= '$1 == "data_tree_sha256" {print $2; exit}' "$CONTRACT"
)"
actual_data_hash="$(
  cd "$DATA"
  find . -type f -print | LC_ALL=C sort |
    while IFS= read -r relative; do
      printf '%s|' "$relative"
      sha256_file "$relative"
    done |
    shasum -a 256 | awk '{print $1}'
)"
[ "$expected_data_hash" = "$actual_data_hash" ] ||
  die "empreinte de l'arbre de données invalide"

if [ "$SEAL" = true ]; then
  [ -f "$PACKAGE/CANONICAL_RELEASE_GATES.OK" ] ||
    die "portes de release non validées"
  [ -f "$PACKAGE/STARTUP_PARITY_V2.OK" ] ||
    die "parité startup v2 non validée"
  {
    printf 'canonical_package_valid=true\n'
    printf 'canonical_package_manifest_entries=%s\n' "$entries"
    printf 'canonical_package_modes=%s\n' \
      "$(awk -F= '$1 == "modes" {print $2; exit}' "$CONTRACT")"
    printf 'canonical_package_startup_to_gameplay=true\n'
    printf 'data_tree_sha256=%s\n' "$actual_data_hash"
    printf 'hardware_validation=deferred_by_user\n'
    printf 'user_manual_acceptance=pending\n'
    printf 'error_code=0\n'
  } >"$PACKAGE/CANONICAL_PACKAGE.METRICS"
  printf '%s' DUSKLIGHT_PSP_CANONICAL_PACKAGE_OK \
    >"$PACKAGE/CANONICAL_PACKAGE.OK"
fi

printf 'CANONICAL_PACKAGE_VALID entries=%s sealed=%s data_sha256=%s\n' \
  "$entries" "$SEAL" "$actual_data_hash"
