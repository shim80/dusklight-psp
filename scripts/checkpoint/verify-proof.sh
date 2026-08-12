#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"
sha256sum -c artifacts/psp/getawait-heart-probe/SHA256SUMS
