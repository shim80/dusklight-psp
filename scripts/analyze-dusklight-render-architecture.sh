#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

OUT_DIR="$(assert_project_path "build/reports")"
safe_mkdir build/reports

require_symbol() {
  local symbol="$1"
  local path="$2"
  rg -q --fixed-strings "$symbol" "$PROJECT_ROOT/$path" ||
    die "symbole source absent : $symbol ($path)"
}

require_symbol "void main01(void)" "dusklight-main/src/m_Do/m_Do_main.cpp"
require_symbol "void fapGm_Execute()" "dusklight-main/src/f_ap/f_ap_game.cpp"
require_symbol "void fpcM_Management(" "dusklight-main/src/f_pc/f_pc_manager.cpp"
require_symbol "int mDoGph_Painter()" "dusklight-main/src/m_Do/m_Do_graphic.cpp"
require_symbol "void dScnKy_env_light_c::exeKankyo()" "dusklight-main/src/d/d_kankyo.cpp"
require_symbol "void dScnKy_env_light_c::drawKankyo()" "dusklight-main/src/d/d_kankyo.cpp"
require_symbol "void dDlst_shadowControl_c::imageDraw" "dusklight-main/src/d/d_drawlist.cpp"
require_symbol "void dDlst_shadowControl_c::draw" "dusklight-main/src/d/d_drawlist.cpp"

CALLGRAPH="$OUT_DIR/dusklight-frame-callgraph.json"
COVERAGE="$OUT_DIR/dusklight-render-source-coverage.json"
ORDER="$OUT_DIR/dusklight-render-order-graph.json"
COMPAT="$OUT_DIR/dusklight-render-compatibility-map.json"

{
  printf '{\n  "schema": 1,\n  "snapshot_commit": "%s",\n' "$(git rev-parse HEAD)"
  printf '  "nodes": [\n'
  printf '    {"id":"main01","order":1,"file":"dusklight-main/src/m_Do/m_Do_main.cpp","reads":["host events","game clock"],"writes":["frame lifecycle"],"psp_state":"facade","divergence":"Aurora loop replaced"},\n'
  printf '    {"id":"fapGm_Execute","order":2,"file":"dusklight-main/src/f_ap/f_ap_game.cpp","reads":["global game state"],"writes":["process management","frame counter"],"psp_state":"facade","divergence":"bounded canonical loop"},\n'
  printf '    {"id":"fpcM_Management","order":3,"file":"dusklight-main/src/f_pc/f_pc_manager.cpp","reads":["process queues","pause"],"writes":["create/delete/execute/draw"],"psp_state":"source_subset","divergence":"fixed pools"},\n'
  printf '    {"id":"environment.execute","symbol":"dScnKy_env_light_c::exeKankyo","order":4,"file":"dusklight-main/src/d/d_kankyo.cpp","reads":["stage","room","time","influences"],"writes":["ambient","fog","light direction"],"psp_state":"planned","divergence":"TEV omitted"},\n'
  printf '    {"id":"actors.execute","symbol":"fpcEx_Handler","order":5,"file":"dusklight-main/src/f_pc/f_pc_executor.cpp","reads":["active processes"],"writes":["actor and scene state"],"psp_state":"source_subset","divergence":"supported profiles only"},\n'
  printf '    {"id":"camera.update","order":6,"file":"dusklight-main/src/f_op/f_op_camera.cpp","reads":["player","scene"],"writes":["view state"],"psp_state":"facade","divergence":"single camera"},\n'
  printf '    {"id":"draw.submit","symbol":"fpcDw_Handler","order":7,"file":"dusklight-main/src/f_pc/f_pc_draw.cpp","reads":["drawable processes"],"writes":["dDlst_list_c"],"psp_state":"facade","divergence":"PspRenderQueue"},\n'
  printf '    {"id":"shadow.image","symbol":"dDlst_shadowControl_c::imageDraw","order":8,"file":"dusklight-main/src/d/d_drawlist.cpp","reads":["real shadow casters"],"writes":["shadow textures"],"psp_state":"planned","divergence":"dedicated low-resolution target"},\n'
  printf '    {"id":"room.opaque","order":9,"file":"dusklight-main/src/m_Do/m_Do_graphic.cpp","reads":["BG draw buffers"],"writes":["color/depth"],"psp_state":"implemented","divergence":"DPRM/GU"},\n'
  printf '    {"id":"shadow.project","symbol":"dDlst_shadowControl_c::draw","order":10,"file":"dusklight-main/src/d/d_drawlist.cpp","reads":["shadow masks","receiver polys","fog"],"writes":["color buffer"],"psp_state":"planned","divergence":"DPCL receivers"},\n'
  printf '    {"id":"actors.draw","order":11,"file":"dusklight-main/src/m_Do/m_Do_graphic.cpp","reads":["opaque/xlu actor queues"],"writes":["color/depth"],"psp_state":"implemented","divergence":"fixed function materials"},\n'
  printf '    {"id":"ui.draw","order":12,"file":"dusklight-main/src/m_Do/m_Do_graphic.cpp","reads":["2D lists"],"writes":["color buffer"],"psp_state":"implemented","divergence":"DPUI v2"},\n'
  printf '    {"id":"present","symbol":"mDoGph_gInf_c::endRender","order":13,"file":"dusklight-main/src/m_Do/m_Do_graphic.cpp","reads":["framebuffer"],"writes":["display"],"psp_state":"implemented","divergence":"sceGuSync/swap"}\n'
  printf '  ],\n  "edges": [\n'
  printf '    ["main01","fapGm_Execute"],["fapGm_Execute","fpcM_Management"],["fpcM_Management","environment.execute"],["environment.execute","actors.execute"],["actors.execute","camera.update"],["camera.update","draw.submit"],["draw.submit","shadow.image"],["shadow.image","room.opaque"],["room.opaque","shadow.project"],["shadow.project","actors.draw"],["actors.draw","ui.draw"],["ui.draw","present"]\n'
  printf '  ]\n}\n'
} >"$CALLGRAPH"

{
  printf '{\n  "schema": 1,\n  "files": [\n'
  first=true
  for path in \
    dusklight-main/src/m_Do/m_Do_main.cpp \
    dusklight-main/src/f_ap/f_ap_game.cpp \
    dusklight-main/src/f_pc/f_pc_manager.cpp \
    dusklight-main/src/m_Do/m_Do_graphic.cpp \
    dusklight-main/src/d/d_kankyo.cpp \
    dusklight-main/src/d/d_kankyo_data.cpp \
    dusklight-main/src/d/d_drawlist.cpp \
    dusklight-main/src/SSystem/SComponent/c_bg_s_shdw_draw.cpp; do
    $first || printf ',\n'
    first=false
    printf '    {"path":"%s","lines":%s,"original_psp_compiled":false}' \
      "$path" "$(wc -l <"$PROJECT_ROOT/$path" | tr -d ' ')"
  done
  printf '\n  ]\n}\n'
} >"$COVERAGE"

cp -- "$CALLGRAPH" "$ORDER"

{
  printf '{\n  "schema": 1,\n  "mappings": [\n'
  printf '    {"source":"fpcM_Management","psp":"PspProcessManager","status":"source_subset"},\n'
  printf '    {"source":"dScnKy_env_light_c","psp":"PspEnvironmentRuntime","status":"planned"},\n'
  printf '    {"source":"dKy_tevstr_c","psp":"PspTevStruct","status":"planned"},\n'
  printf '    {"source":"J3DDrawBuffer/dDlst_list_c","psp":"PspRenderQueue","status":"implemented_subset"},\n'
  printf '    {"source":"dDlst_shadowControl_c","psp":"PspShadowSystem","status":"planned"},\n'
  printf '    {"source":"cBgS_ShdwDraw","psp":"DPCL receiver selection","status":"planned"},\n'
  printf '    {"source":"J2D draw lists","psp":"DPUI v2","status":"implemented"}\n'
  printf '  ]\n}\n'
} >"$COMPAT"

printf 'DUSKLIGHT_RENDER_ARCHITECTURE_OK nodes=13 outputs=4\n'
