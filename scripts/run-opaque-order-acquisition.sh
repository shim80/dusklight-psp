#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

variant=
while [ "$#" -gt 0 ]; do
  case "$1" in
    --variant)
      shift
      [ "$#" -gt 0 ] || die "--variant exige une valeur"
      variant="$1"
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
case "$variant" in
  source_order|reverse_order|deterministic_permutation) ;;
  *) die "variante d'ordre opaque invalide : $variant" ;;
esac

exec "$SCRIPT_DIR/run-ppsspp-dusklight-psp.sh" --run \
  --mode opaque_order_invariance \
  --opaque-order-variant "$variant" \
  --presentation opaque_only \
  --backend opengl \
  --transport gui \
  --timeout 300
