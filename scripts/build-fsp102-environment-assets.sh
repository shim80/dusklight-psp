#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"
require_project_root
SOURCE_ROOT="${DUSKLIGHT_FSP102_SOURCE_ROOT:-}"
[ -n "$SOURCE_ROOT" ] || die "DUSKLIGHT_FSP102_SOURCE_ROOT is required"
[ -d "$SOURCE_ROOT/R00/bmdr" ] || die "missing F_SP102 R00 source models"
[ -d "$SOURCE_ROOT/STG/bmdp" ] || die "missing F_SP102 stage source models"
GCLIB_COMMIT="64127742467acb633d51685b9b1798ab45bb4034"
GCLIB_ROOT="$(assert_project_path ".tools/sources/gclib/$GCLIB_COMMIT")"
if [ ! -d "$GCLIB_ROOT/.git" ]; then
  "$SCRIPT_DIR/bootstrap-fsp102-exporter.sh" >/dev/null
fi
OUT="$(assert_project_path "build/assets/fsp102-environment")"
PASS1="$(assert_project_path ".work/fsp102-environment-pass1")"
PASS2="$(assert_project_path ".work/fsp102-environment-pass2")"
rm -rf -- "$PASS1" "$PASS2" "$OUT"
mkdir -p -- "$PASS1" "$PASS2" "$OUT"
run_pass() {
  GCLIB_ROOT="$GCLIB_ROOT" python3 "$PROJECT_ROOT/tools/fsp102_environment_export.py" "$SOURCE_ROOT" "$1"
}
run_pass "$PASS1"
run_pass "$PASS2"
for file in fsp102_environment.dprm fsp102_environment.dptx FSP102_ENVIRONMENT.MANIFEST; do
  cmp "$PASS1/$file" "$PASS2/$file"
  cp -- "$PASS1/$file" "$OUT/$file"
done
sha256sum "$OUT/fsp102_environment.dprm" "$OUT/fsp102_environment.dptx" > "$OUT/FSP102_ENVIRONMENT.SHA256"
printf 'FSP102_ENVIRONMENT_ASSETS_OK deterministic=true models=9\n'
