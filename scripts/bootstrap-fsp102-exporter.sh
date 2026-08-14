#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"
require_project_root
GCLIB_URL="https://github.com/LagoLunatic/gclib.git"
GCLIB_COMMIT="64127742467acb633d51685b9b1798ab45bb4034"
ROOT="$(assert_project_path ".tools/sources/gclib")"
CHECKOUT="$ROOT/$GCLIB_COMMIT"
safe_mkdir ".tools/sources/gclib"
if [ ! -d "$CHECKOUT/.git" ]; then
  rm -rf -- "$CHECKOUT"
  git clone --filter=blob:none "$GCLIB_URL" "$CHECKOUT"
  git -C "$CHECKOUT" checkout --detach "$GCLIB_COMMIT"
fi
[ "$(git -C "$CHECKOUT" rev-parse HEAD)" = "$GCLIB_COMMIT" ] || die "gclib revision mismatch"
[ -z "$(git -C "$CHECKOUT" status --short)" ] || die "gclib checkout is dirty"
python3 - <<'PY' >/dev/null
from PIL import Image
PY
printf 'FSP102_EXPORTER_BOOTSTRAP_OK gclib=%s\n' "$GCLIB_COMMIT"
