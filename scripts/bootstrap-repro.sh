#!/usr/bin/env bash
set -euo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
TOOLS="$ROOT/.tools"
CACHE="$ROOT/.cache/downloads"
PSPDEV_URL='https://github.com/pspdev/pspdev/releases/download/v20260701/pspdev-ubuntu-latest-x86_64.tar.gz'
PSPDEV_SHA='f8f2f2235995836188e5fce2e6225c4b17a47232ea82dd850dbf7a5d99c90587'
PPSSPP_URL='https://github.com/hrydgard/ppsspp/releases/download/v1.20.4/PPSSPP-v1.20.4-anylinux-x86_64.AppImage'
PPSSPP_SHA='661c098e6b7f7610171a57b7c533ce8bba6f2312b71e76d61e850461973eba21'

[ "$(uname -s)" = Linux ] && [ "$(uname -m)" = x86_64 ] || { echo 'Exact validated bootstrap supports Linux x86_64.' >&2; exit 2; }
for c in curl git tar xz sha256sum cmake python3; do command -v "$c" >/dev/null || { echo "Missing host command: $c" >&2; exit 2; }; done
command -v ninja >/dev/null || { echo 'Install Ninja (Debian/Ubuntu: sudo apt-get install ninja-build).' >&2; exit 2; }
mkdir -p "$TOOLS" "$CACHE"

download_verify() {
  local url=$1 sha=$2 out=$3
  if [ ! -f "$out" ]; then
    curl --fail --location --proto '=https' --tlsv1.2 -o "$out.part" "$url"
    mv "$out.part" "$out"
  fi
  echo "$sha  $out" | sha256sum -c -
}

PSPDEV_ARCHIVE="$CACHE/pspdev-v20260701-linux-x86_64.tar.gz"
download_verify "$PSPDEV_URL" "$PSPDEV_SHA" "$PSPDEV_ARCHIVE"
if [ ! -x "$TOOLS/pspdev/bin/psp-gcc" ]; then
  stage=$(mktemp -d)
  trap 'rm -rf "$stage"' EXIT
  tar -xzf "$PSPDEV_ARCHIVE" -C "$stage"
  candidate=$(find "$stage" -type f -path '*/bin/psp-gcc' -print -quit)
  [ -n "$candidate" ] || { echo 'psp-gcc not found in PSPDEV archive' >&2; exit 1; }
  prefix=${candidate%/bin/psp-gcc}
  rm -rf "$TOOLS/pspdev"
  mkdir -p "$TOOLS/pspdev"
  cp -a "$prefix"/. "$TOOLS/pspdev"/
fi

mkdir -p "$TOOLS/ppsspp"
PPSSPP="$TOOLS/ppsspp/PPSSPP-v1.20.4-anylinux-x86_64.AppImage"
download_verify "$PPSSPP_URL" "$PPSSPP_SHA" "$PPSSPP"
chmod +x "$PPSSPP"
"$ROOT/scripts/assemble-source.sh" "$ROOT/.work/assembled"
cat <<EOF
Environment ready.
export PSPDEV='$TOOLS/pspdev'
export PSPSDK='$TOOLS/pspdev/psp/sdk'
export PATH='$TOOLS/pspdev/bin':\$PATH
PPSSPP='$PPSSPP'
Assembled project='$ROOT/.work/assembled'
EOF
