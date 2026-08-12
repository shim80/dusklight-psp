#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

HEADER="$PROJECT_ROOT/dusklight-main/include/d/d_drawlist.h"
SOURCE="$PROJECT_ROOT/dusklight-main/src/d/d_drawlist.cpp"
RECEIVER="$PROJECT_ROOT/dusklight-main/include/SSystem/SComponent/c_bg_s_shdw_draw.h"

for symbol in \
  dDlst_shadowControl_c dDlst_shadowReal_c dDlst_shadowSimple_c \
  dDlst_shadowRealPoly_c; do
  rg -q "$symbol" "$HEADER" "$SOURCE" ||
    die "symbole d'ombre source absent : $symbol"
done
rg -q 'mSimple\[128\]' "$HEADER" ||
  die "capacité simple source absente"
rg -q 'mReal\[8\]' "$HEADER" ||
  die "capacité projetée source absente"
rg -q 'mShadowTri\[256\]' "$HEADER" ||
  die "capacité receiver source absente"
rg -q 'mCallbackFun' "$RECEIVER" ||
  die "callback receiver source absent"
rg -q 'dComIfG_Bgsp\(\)\.ShdwDraw' "$SOURCE" ||
  die "soumission receiver collision absente"
rg -q 'static u16 l_realImageSize\[2\] = \{192, 64\}' "$SOURCE" ||
  die "résolutions source absentes"

printf '%s\n' \
  "HOST_SHADOW_SOURCE_AUDIT_OK real=8 simple=128 receiver_triangles=256 maps=192,64"
