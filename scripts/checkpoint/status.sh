#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
printf 'root=%s\n' "$ROOT"
if command -v git >/dev/null 2>&1 && [ -d "$ROOT/.git" ]; then
  git -C "$ROOT" status --short --branch
  printf 'head='; git -C "$ROOT" rev-parse HEAD
fi
printf '%s\n' 'key files:'
for f in AGENTS.md docs/STATUS.md docs/RESUME.md docs/COMMIT_LEDGER.md scripts/assemble-source.sh scripts/bootstrap-repro.sh; do
  if [ -f "$ROOT/$f" ]; then printf '  OK %s\n' "$f"; else printf '  MISSING %s\n' "$f"; fi
done
