#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

STRICT=0
case "${1:-}" in
  "") ;;
  --strict) STRICT=1 ;;
  -h|--help)
    printf '%s\n' "Usage: verify-environment.sh [--strict]"
    exit 0
    ;;
  *) die "option inconnue : $1" ;;
esac

missing_host=0
missing_psp=0

check_command() {
  local label="$1" command_name="$2"
  if command -v "$command_name" >/dev/null 2>&1; then
    printf '[OK] %-22s %s\n' "$label" "$(command -v "$command_name")"
  else
    printf '[MANQUANT] %-17s %s\n' "$label" "$command_name"
    missing_host=1
  fi
}

check_file() {
  local label="$1" path="$2"
  assert_project_path "$path" >/dev/null
  if [ -f "$path" ]; then
    printf '[OK] %-22s %s\n' "$label" "${path#"$PROJECT_ROOT"/}"
  else
    printf '[MANQUANT] %-17s %s\n' "$label" "${path#"$PROJECT_ROOT"/}"
    missing_psp=1
  fi
}

printf 'Racine Git       : %s\n' "$PROJECT_ROOT"
printf 'Plateforme       : %s\n' "$(host_platform)"
printf 'PSPDEV           : %s\n' "$PSPDEV"
printf 'État PPSSPP      : %s\n\n' "$PPSSPP_STATE_ROOT"

check_command "Git" git
check_command "CMake" cmake
check_command "Python 3" python3
check_command "SHA-256" shasum
check_command "Archive tar" tar
check_command "Archive zip" unzip

if command -v python3 >/dev/null 2>&1; then
  if python3 -c 'import pathlib, tomllib, sys; tomllib.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))' "$MANIFEST_PATH"; then
    printf '[OK] %-22s %s\n' "TOML manifeste" "toolchain/manifest.lock"
  else
    printf '[INVALIDE] %-18s %s\n' "TOML manifeste" "toolchain/manifest.lock"
    missing_host=1
  fi
fi

check_file "PSP GCC" "$PSPDEV/bin/psp-gcc"
check_file "PSP Config" "$PSPDEV/bin/psp-config"
check_file "PSP objdump" "$PSPDEV/bin/psp-objdump"
check_file "Créateur PBP" "$PSPDEV/bin/pack-pbp"

ppsspp_found=""
for candidate in \
  "$PROJECT_ROOT/.tools/ppsspp/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL" \
  "$PROJECT_ROOT/.tools/ppsspp/PPSSPP.app/Contents/MacOS/PPSSPP" \
  "$PROJECT_ROOT/.tools/ppsspp/PPSSPP.AppImage" \
  "$PROJECT_ROOT/.tools/ppsspp/PPSSPPWindows64.exe"; do
  assert_project_path "$candidate" >/dev/null
  if [ -f "$candidate" ]; then ppsspp_found="$candidate"; break; fi
done
if [ -n "$ppsspp_found" ]; then
  printf '[OK] %-22s %s\n' "PPSSPP" "${ppsspp_found#"$PROJECT_ROOT"/}"
else
  printf '[MANQUANT] %-17s %s\n' "PPSSPP" ".tools/ppsspp"
  missing_psp=1
fi

printf '\nHôte prêt : %s\n' "$([ "$missing_host" -eq 0 ] && printf oui || printf non)"
printf 'PSP/PPSSPP installé : %s\n' "$([ "$missing_psp" -eq 0 ] && printf oui || printf non)"
printf '%s\n' "Ce contrôle n'a exécuté aucun binaire situé sous .tools/."

if [ "$missing_host" -ne 0 ]; then exit 2; fi
if [ "$STRICT" -eq 1 ] && [ "$missing_psp" -ne 0 ]; then exit 1; fi
exit 0
