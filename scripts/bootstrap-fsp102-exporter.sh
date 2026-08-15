#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"
require_project_root
GCLIB_COMMIT="64127742467acb633d51685b9b1798ab45bb4034"
ROOT="$(assert_project_path ".tools/sources/gclib")"
CHECKOUT="$ROOT/$GCLIB_COMMIT"
ARCHIVE="$(assert_project_path ".cache/downloads/gclib-6412774-source-temp.zip")"
ARCHIVE_SHA256="73a6b473b8e940042be8a92b85c55f692bf4d433411406bfb972261c0020c56b"
INNER="gclib-$GCLIB_COMMIT.tar.gz"
INNER_SHA256="e2452c0e0f2cb37ec5b6bb37b1a1b494cf0ef5c1180f08955fc293f53e0dfa57"
TRANSPORT="$ROOT/transport"
safe_mkdir ".tools/sources/gclib"
python3 - <<'PY' >/dev/null
from PIL import Image
PY
if [ -f "$CHECKOUT/LICENSE.txt" ] && [ -f "$CHECKOUT/gclib/j3d.py" ]; then
  printf 'FSP102_EXPORTER_BOOTSTRAP_OK gclib=%s cached=true\n' "$GCLIB_COMMIT"
  exit 0
fi
[ ! -e "$CHECKOUT" ] || die "partial gclib checkout present: $CHECKOUT"
[ -f "$ARCHIVE" ] || die "verified gclib artifact absent: place gclib-6412774-source-temp.zip in .cache/downloads/"
[ "$(sha256_file "$ARCHIVE")" = "$ARCHIVE_SHA256" ] || die "gclib artifact SHA-256 mismatch"
safe_mkdir ".tools/sources/gclib/transport"
unzip -q -o "$ARCHIVE" -d "$TRANSPORT"
[ "$(sha256_file "$TRANSPORT/$INNER")" = "$INNER_SHA256" ] || die "gclib inner archive SHA-256 mismatch"
mkdir -p -- "$CHECKOUT"
tar -xzf "$TRANSPORT/$INNER" -C "$CHECKOUT"
[ -f "$CHECKOUT/LICENSE.txt" ] && [ -f "$CHECKOUT/gclib/j3d.py" ] || die "gclib extraction incomplete"
printf 'FSP102_EXPORTER_BOOTSTRAP_OK gclib=%s cached=false\n' "$GCLIB_COMMIT"
