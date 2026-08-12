#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"
if [ -f artifacts/psp/getawait-heart-probe/SHA256SUMS ]; then
  sha256sum -c artifacts/psp/getawait-heart-probe/SHA256SUMS
else
  echo 'Proof artifacts are optional in a clean source checkout.'
fi
