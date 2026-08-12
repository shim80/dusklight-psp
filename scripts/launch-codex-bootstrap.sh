#!/usr/bin/env bash
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
PROJECT_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd -P)"
cd -- "$PROJECT_ROOT"

if ! command -v codex >/dev/null 2>&1; then
  printf '%s\n' "Erreur : la commande 'codex' est introuvable." >&2
  exit 127
fi

exec codex \
  --sandbox workspace-write \
  --ask-for-approval on-request \
  --config sandbox_workspace_write.network_access=true
