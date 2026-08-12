#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

KANKYO_HEADER="$PROJECT_ROOT/dusklight-main/include/d/d_kankyo.h"
STAGE_HEADER="$PROJECT_ROOT/dusklight-main/include/d/d_stage.h"
KANKYO_SOURCE="$PROJECT_ROOT/dusklight-main/src/d/d_kankyo.cpp"
EXPORT_SOURCE="$PROJECT_ROOT/tools/dusk_link_loader_probe/src/playable_export.cpp"

for symbol in \
  dScnKy_env_light_c dKy_tevstr_c stage_palette_info_class; do
  rg -q "$symbol" "$KANKYO_HEADER" "$STAGE_HEADER" ||
    die "symbole environnement source absent : $symbol"
done
for tag in Env0 Col0 PAL0 LGT0 LGHT; do
  rg -q "\"$tag\"" "$EXPORT_SOURCE" ||
    die "tag environnement non audité : $tag"
done
rg -q 'getStartStageName\(\), "D_MN10"' "$KANKYO_SOURCE" ||
  die "sélection dKy D_MN10 absente"
rg -q 'pat = 8;' "$KANKYO_SOURCE" ||
  die "pattern dKy 8 absent"
rg -q 'pat = 14;' "$KANKYO_SOURCE" ||
  die "pattern dKy 14 absent"
rg -q 'Env0>Col0>PAL0' "$EXPORT_SOURCE" ||
  die "chaîne de provenance environnement absente"

printf '%s\n' \
  "HOST_ENVIRONMENT_SOURCE_AUDIT_OK source=dKy tags=5 patterns=2"
