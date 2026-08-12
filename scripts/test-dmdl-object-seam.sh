#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

HOST_CXX="${CXX:-c++}"
command -v "$HOST_CXX" >/dev/null 2>&1 ||
  die "compilateur C++ hôte absent : $HOST_CXX"
command -v psp-g++ >/dev/null 2>&1 ||
  die "psp-g++ absent de la toolchain locale"

safe_mkdir build/host/model-obj-seam
safe_mkdir build/psp/model-obj-seam

SOURCE="$PROJECT_ROOT/test/model-obj-seam/model_obj_probe.cpp"
INCLUDE_GAME="$PROJECT_ROOT/dusklight-main/include"
INCLUDE_DOLPHIN="$PROJECT_ROOT/dusklight-main/libs/dolphin/include"
HOST_BINARY="$PROJECT_ROOT/build/host/model-obj-seam/model-obj-probe"
HOST_DEPS="$PROJECT_ROOT/build/host/model-obj-seam/model-obj-probe.d"
PSP_OBJECT="$PROJECT_ROOT/build/psp/model-obj-seam/model-obj-probe.o"
PSP_DEPS="$PROJECT_ROOT/build/psp/model-obj-seam/model-obj-probe.d"

"$HOST_CXX" \
  -std=c++20 -Wall -Wextra -Werror \
  -I"$INCLUDE_GAME" \
  -I"$INCLUDE_DOLPHIN" \
  -MMD -MF "$HOST_DEPS" \
  "$SOURCE" \
  -o "$HOST_BINARY"

psp-g++ \
  -std=c++20 -Wall -Wextra -Werror -G0 \
  -I"$INCLUDE_GAME" \
  -I"$INCLUDE_DOLPHIN" \
  -MMD -MF "$PSP_DEPS" \
  -c "$SOURCE" \
  -o "$PSP_OBJECT"

for dependency_file in "$HOST_DEPS" "$PSP_DEPS"; do
  [ -f "$dependency_file" ] ||
    die "fichier de dépendances absent : $dependency_file"
  if rg -i \
    'extern/aurora|J3DGraphBase|J3DPacket|GXAurora|ppc_math\.h|/gx(\.h|/)' \
    "$dependency_file" >/dev/null; then
    die "fermeture interdite détectée : $dependency_file"
  fi
done
printf '%s\n' \
  "[OK] Fermeture d'en-têtes légère sans Aurora, J3D ni GX."

"$HOST_BINARY"
printf '%s\n' \
  "[OK] ABI et copie bit à bit de dMdl_obj_c validées sur l'hôte."

[ -f "$PSP_OBJECT" ] ||
  die "objet du probe PSPSDK absent"
printf '%s\n' \
  "[OK] ABI 32 bits et taille 0x34 de dMdl_obj_c validées par PSPSDK."

[ "$(rg -l '^class dMdl_obj_c \{' "$PROJECT_ROOT/dusklight-main/include" |
  wc -l | tr -d ' ')" = 1 ] ||
  die "dMdl_obj_c doit avoir une définition unique"
rg -F '#include "d/d_model_obj.h"' \
  "$PROJECT_ROOT/dusklight-main/include/d/d_model.h" >/dev/null ||
  die "d_model.h n'inclut pas l'en-tête léger"
if rg '^class dMdl_obj_c \{' \
  "$PROJECT_ROOT/dusklight-main/include/d/d_model.h" >/dev/null; then
  die "d_model.h redéfinit encore dMdl_obj_c"
fi
rg -F 'void entryObj(dMdl_obj_c*);' \
  "$PROJECT_ROOT/dusklight-main/include/d/d_model.h" >/dev/null ||
  die "signature dMdl_c::entryObj modifiée"
rg -F 'dMdl_obj_c* mpModelObj;' \
  "$PROJECT_ROOT/dusklight-main/include/d/d_model.h" >/dev/null ||
  die "type de dMdl_c::mpModelObj modifié"
rg -F 'void dMdl_c::entryObj(dMdl_obj_c* i_obj)' \
  "$PROJECT_ROOT/dusklight-main/src/d/d_model.cpp" >/dev/null ||
  die "implémentation dMdl_c::entryObj modifiée"
rg -F 'obj = obj->mpObj' \
  "$PROJECT_ROOT/dusklight-main/src/d/d_model.cpp" >/dev/null ||
  die "chaînage dMdl_obj_c modifié"

printf '%s\n' \
  "[OK] dMdl_obj_c réel et définition unique validés."
