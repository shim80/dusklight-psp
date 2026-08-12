#!/usr/bin/env bash
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
OUT="${1:-$ROOT/.work/assembled}"
DUSKLIGHT_URL='https://github.com/TwilitRealm/dusklight.git'
DUSKLIGHT_COMMIT='1bae8a5e6a812217ca33ba533e707ecfa64b1553'
AURORA_URL='https://github.com/encounter/aurora.git'
AURORA_COMMIT='81f12f31d23ec822d8bde2031c91e94c470911eb'
NOD_URL='https://github.com/encounter/nod.git'
NOD_COMMIT='dc18d2ff129f05228b8510ea092d8b24c290a49a'
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
for c in git tar; do command -v "$c" >/dev/null || { echo "Missing command: $c" >&2; exit 2; }; done
rm -rf "$OUT"
mkdir -p "$OUT/dusklight-main" "$OUT/.tools/sources"

git clone --no-checkout "$DUSKLIGHT_URL" "$TMP/dusklight"
git -C "$TMP/dusklight" checkout --detach "$DUSKLIGHT_COMMIT"
git -C "$TMP/dusklight" archive "$DUSKLIGHT_COMMIT" | tar -x -C "$OUT/dusklight-main"
rm -rf "$OUT/dusklight-main/extern/aurora"
git clone "$AURORA_URL" "$OUT/dusklight-main/extern/aurora"
git -C "$OUT/dusklight-main/extern/aurora" checkout --detach "$AURORA_COMMIT"
git clone "$NOD_URL" "$OUT/.tools/sources/nod"
git -C "$OUT/.tools/sources/nod" checkout --detach "$NOD_COMMIT"

# Overlay every redistributable PSP/publication file from this repository.
for path in dusklight-main/platforms/psp test scripts docs reference toolchain artifacts AGENTS.md CONTRIBUTING.md README.md .gitignore; do
  [ -e "$ROOT/$path" ] || continue
  mkdir -p "$OUT/$(dirname "$path")"
  rm -rf "$OUT/$path"
  cp -a "$ROOT/$path" "$OUT/$path"
done

[ "$(git -C "$TMP/dusklight" rev-parse HEAD)" = "$DUSKLIGHT_COMMIT" ]
[ "$(git -C "$OUT/dusklight-main/extern/aurora" rev-parse HEAD)" = "$AURORA_COMMIT" ]
[ "$(git -C "$OUT/.tools/sources/nod" rev-parse HEAD)" = "$NOD_COMMIT" ]
printf 'Assembled reproducible source tree: %s\n' "$OUT"
