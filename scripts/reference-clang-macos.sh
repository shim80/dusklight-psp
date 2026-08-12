#!/usr/bin/env bash
set -euo pipefail

command -v brew >/dev/null 2>&1 || {
  printf '%s\n' "Erreur : Homebrew introuvable pour localiser LLVM" >&2
  exit 1
}
compiler="$(brew --prefix llvm)/bin/clang"
[ -x "$compiler" ] || {
  printf 'Erreur : compilateur LLVM absent : %s\n' "$compiler" >&2
  exit 1
}

has_sysroot=false
for arg in "$@"; do
  case "$arg" in
    -isysroot|--sysroot|--sysroot=*)
      has_sysroot=true
      break
      ;;
  esac
done

if [ "$has_sysroot" = false ] && [ -n "${SDKROOT:-}" ] &&
   [ -d "$SDKROOT" ]; then
  exec "$compiler" -isysroot "$SDKROOT" "$@"
fi
exec "$compiler" "$@"
