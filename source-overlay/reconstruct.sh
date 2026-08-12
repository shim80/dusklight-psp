#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${1:-/tmp/dusklight-psp-code-overlay.tar.xz}
cat "$root"/code-overlay-b64/part* | base64 -d > "$out"
echo "ce56f4d674c2faad781dfd53ae1ff3a5e7110d29a3ecfab756947c099a582527  $out" | sha256sum -c -
xz -t "$out"
echo "overlay OK: $out"
