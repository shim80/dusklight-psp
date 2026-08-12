#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

MODE=plan
case "${1:-}" in
  ""|--plan) ;;
  --execute) MODE=execute ;;
  -h|--help)
    printf '%s\n' "Usage: clean-generated.sh [--plan|--execute]"
    exit 0
    ;;
  *) die "option inconnue : $1" ;;
esac

TARGETS=(
  ".tmp"
  "build/host"
  "build/psp"
  "logs/audit"
  "logs/bootstrap"
  "logs/ppsspp"
  "artifacts"
  ".test-data/ppsspp"
)

items=()
for relative in "${TARGETS[@]}"; do
  base="$(assert_project_path "$relative")"
  [ ! -L "$base" ] || die "cible de nettoyage symbolique refusée : $relative"
  [ -d "$base" ] || continue
  while IFS= read -r -d '' item; do
    [ ! -L "$item" ] || die "lien symbolique détecté; nettoyage annulé : $item"
    assert_project_path "$item" >/dev/null
    items+=("$item")
  done < <(find "$base" -mindepth 1 -maxdepth 1 ! -name .gitkeep -print0)
done

if [ "${#items[@]}" -eq 0 ]; then
  printf '%s\n' "Aucun fichier généré à nettoyer."
  exit 0
fi

printf 'Mode : %s\n' "$MODE"
for item in "${items[@]}"; do
  printf '  %s\n' "${item#"$PROJECT_ROOT"/}"
done

if [ "$MODE" = plan ]; then
  printf '%s\n' "Plan uniquement; relancer avec --execute après examen."
  exit 0
fi

for item in "${items[@]}"; do
  rm -rf -- "$item"
done
printf '%s\n' "Nettoyage terminé sur la liste fermée ci-dessus."
