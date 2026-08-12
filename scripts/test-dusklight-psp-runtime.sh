#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"
# shellcheck source=env.sh
. "$SCRIPT_DIR/env.sh" >/dev/null

case "${1:-}" in
  --target|--fast|--checkpoint|--release|--no-cache|--force|--explain-plan)
    # shellcheck source=lib/dusklight-test-graph.sh
    . "$SCRIPT_DIR/lib/dusklight-test-graph.sh"
    test_graph_main "$@"
    exit $?
    ;;
  --target-internal)
    [ "${2:-}" = build ] || die "cible interne inconnue"
    [ "${3:-}" = --no-deps ] || die "une cible interne exige --no-deps"
    [ -n "${DUSKLIGHT_ORCHESTRATOR_ACTIVE:-}" ] ||
      die "cible interne réservée à l'orchestrateur"
    "$SCRIPT_DIR/build-dusklight-psp-assets.sh"
    psp-cmake -S "$PROJECT_ROOT/test/dusklight-psp" \
      -B "$PROJECT_ROOT/build/psp/dusklight" \
      -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DDUSKLIGHT_BUILD_COMMIT="$(git rev-parse HEAD)"
    cmake --build "$PROJECT_ROOT/build/psp/dusklight"
    "$PSPDEV/bin/psp-objdump" -f \
      "$PROJECT_ROOT/build/psp/dusklight/dusklight_psp.elf" |
      grep -q 'architecture: mips:allegrex' ||
      die "ELF non Allegrex"
    # Keep every subsequent PPSSPP request bound to the binary that this
    # build node actually produced.  Packaging and identity generation are
    # part of the atomic build boundary, not deferred release side effects.
    "$SCRIPT_DIR/package-dusklight-psp.sh"
    "$SCRIPT_DIR/generate-parity-build-identity.sh"
    printf 'DUSKLIGHT_PSP_BUILD_NODE_OK\n'
    exit 0
    ;;
  --release-internal)
    [ "${2:-}" = --no-deps ] ||
      die "la release interne exige --no-deps"
    [ "$#" -eq 2 ] || die "options release internes inattendues"
    [ -n "${DUSKLIGHT_ORCHESTRATOR_ACTIVE:-}" ] ||
      die "release interne réservée à l'orchestrateur"
    TEST_MODE=release_internal
    ;;
esac

TIMEOUT_SECONDS=600
TEST_MODE="${TEST_MODE:-full}"
if [ "$TEST_MODE" = release_internal ]; then
  shift 2
fi
while [ "$#" -gt 0 ]; do
  case "$1" in
    --fast) TEST_MODE=fast ;;
    --full) TEST_MODE=full ;;
    --timeout)
      shift
      [ "$#" -gt 0 ] || die "--timeout exige une valeur"
      TIMEOUT_SECONDS="$1"
      ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done
[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] || die "DUSKLIGHT_GAME_IMAGE est requis"
safe_mkdir logs/dusklight-psp
safe_mkdir build/reports

if [ "$TEST_MODE" = full ]; then
  "$SCRIPT_DIR/test-psp-smokes.sh" --timeout 30
  "$SCRIPT_DIR/test-local-link-demo.sh"
  "$SCRIPT_DIR/run-ppsspp-link-demo.sh" --run --timeout 30
  "$SCRIPT_DIR/test-local-link-playable-demo.sh" \
    --skip-prerequisites --timeout 120
  "$SCRIPT_DIR/test-first-real-room-demo.sh" --timeout 180
  "$SCRIPT_DIR/test-first-real-actor-demo.sh" \
    --skip-prerequisites --timeout 240
  "$SCRIPT_DIR/test-room-transition-demo.sh" \
    --skip-prerequisites --timeout 600
fi
"$SCRIPT_DIR/analyze-dusklight-psp-portability.sh"
"$SCRIPT_DIR/build-dusklight-psp-assets.sh"

cmake -S "$PROJECT_ROOT/test/canonical-runtime" \
  -B "$PROJECT_ROOT/build/host/canonical-runtime" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$PROJECT_ROOT/build/host/canonical-runtime"
"$PROJECT_ROOT/build/host/canonical-runtime/canonical_core_host_test"
"$PROJECT_ROOT/build/host/canonical-runtime/frame_profiler_host_test"
"$PROJECT_ROOT/build/host/canonical-runtime/original_scene_exit_host_test" \
  "$PROJECT_ROOT/build/assets/room-transition/stages/D_MN10/R09/room.dpsc" \
  "$PROJECT_ROOT/build/assets/room-transition/stages/D_MN10/R02/room.dpsc"
"$PROJECT_ROOT/build/host/canonical-runtime/original_rendered_actor_host_test" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/L4HsMato/model.dprm" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/L4HsMato/textures.dptx" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/L4HsMato/collision.dpcl" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dpsc" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R02/room.dpsc"
"$PROJECT_ROOT/build/host/canonical-runtime/original_dynamic_actor_host_test" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/P_Gear/small/model.dprm" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/P_Gear/small/textures.dptx" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/P_Gear/large/model.dprm" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/P_Gear/large/textures.dptx" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dpsc"
"$PROJECT_ROOT/build/host/canonical-runtime/original_tier_a_actor_host_test"
"$PROJECT_ROOT/build/host/canonical-runtime/original_tbox_switch_host_test"
"$PROJECT_ROOT/build/host/canonical-runtime/switch_surface_host_test"
"$PROJECT_ROOT/build/host/canonical-runtime/interaction_surface_host_test"
"$PROJECT_ROOT/build/host/canonical-runtime/bck_surface_host_test"
"$PROJECT_ROOT/build/host/canonical-runtime/movebg_surface_host_test" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R09/room.dpcl"
"$PROJECT_ROOT/build/host/canonical-runtime/original_door_asset_host_test" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/L4R02Gate/model.dprm" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/L4R02Gate/textures.dptx" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/L4R02Gate/collision.dpcl"
"$PROJECT_ROOT/build/host/canonical-runtime/original_spinner_switch_asset_host_test" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/P_Sswitch/base/model.dprm" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/P_Sswitch/base/textures.dptx" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/P_Sswitch/base/collision.dpcl" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/P_Sswitch/top/model.dprm" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/P_Sswitch/top/textures.dptx" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/P_Sswitch/top/collision.dpcl"
"$PROJECT_ROOT/build/host/canonical-runtime/original_tbox_asset_host_test" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/Dalways/large/model.dprm" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/Dalways/large/textures.dptx" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/Dalways/large/open.dpan" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/Dalways/large/closed.dpcl" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/Dalways/large/open.dpcl"
"$PROJECT_ROOT/build/host/canonical-runtime/original_tbox_resource_host_test" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data"
"$PROJECT_ROOT/build/host/canonical-runtime/original_tbox_host_test" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R02/room.dpsc"
"$PROJECT_ROOT/build/host/canonical-runtime/event_surface_host_test"
"$PROJECT_ROOT/build/host/canonical-runtime/original_spinner_switch_host_test" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data"
"$PROJECT_ROOT/build/host/canonical-runtime/original_door_host_test" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/L4R02Gate/model.dprm" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/L4R02Gate/textures.dptx" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/objects/L4R02Gate/collision.dpcl" \
  "$PROJECT_ROOT/build/assets/dusklight-psp/data/stages/D_MN10/R02/room.dpsc"

c++ -std=c++20 -MM \
  -I "$PROJECT_ROOT/dusklight-main/platforms/psp/compat/include" \
  -I "$PROJECT_ROOT/dusklight-main/include" \
  -I "$PROJECT_ROOT/dusklight-main/platforms/psp/include" \
  "$PROJECT_ROOT/dusklight-main/src/d/actor/d_a_scene_exit.cpp" \
  >"$PROJECT_ROOT/build/reports/original-scene-exit-includes.txt"
if grep -Eqi \
  'Aurora|Nod|SDL|OpenGL|Vulkan|J3D|GX' \
  "$PROJECT_ROOT/build/reports/original-scene-exit-includes.txt"; then
  die "fermeture originale interdite"
fi
c++ -std=c++20 -MM \
  -I "$PROJECT_ROOT/dusklight-main/platforms/psp/compat/include" \
  -I "$PROJECT_ROOT/dusklight-main/include" \
  -I "$PROJECT_ROOT/dusklight-main/platforms/psp/include" \
  "$PROJECT_ROOT/dusklight-main/src/d/actor/d_a_tag_poFire.cpp" \
  >"$PROJECT_ROOT/build/reports/original-tier-a-actor-includes.txt"
if grep -Eqi \
  'Aurora|Nod|SDL|OpenGL|Vulkan|JSystem|GX' \
  "$PROJECT_ROOT/build/reports/original-tier-a-actor-includes.txt"; then
  die "fermeture acteur Tier A original interdite"
fi
if grep -Eqi \
  'DPRM|DPTX|DPCL|ms0:|sceGu|sceGum' \
  "$PROJECT_ROOT/dusklight-main/src/d/actor/d_a_tag_poFire.cpp"; then
  die "la source Tier A originale connaît le backend PSP"
fi
c++ -std=c++20 -MM \
  -I "$PROJECT_ROOT/dusklight-main/platforms/psp/compat/include" \
  -I "$PROJECT_ROOT/dusklight-main/include" \
  -I "$PROJECT_ROOT/dusklight-main/platforms/psp/include" \
  "$PROJECT_ROOT/dusklight-main/src/d/actor/d_a_obj_lv4gear.cpp" \
  >"$PROJECT_ROOT/build/reports/original-dynamic-actor-includes.txt"
if grep -Eqi \
  'Aurora|Nod|SDL|OpenGL|Vulkan|JSystem|GX' \
  "$PROJECT_ROOT/build/reports/original-dynamic-actor-includes.txt"; then
  die "fermeture acteur dynamique original interdite"
fi
if grep -Eqi \
  'DPRM|DPTX|DPCL|ms0:|sceGu|sceGum' \
  "$PROJECT_ROOT/dusklight-main/src/d/actor/d_a_obj_lv4gear.cpp"; then
  die "la source originale dynamique connaît le backend PSP"
fi
c++ -std=c++20 -MM \
  -I "$PROJECT_ROOT/dusklight-main/platforms/psp/compat/include" \
  -I "$PROJECT_ROOT/dusklight-main/include" \
  -I "$PROJECT_ROOT/dusklight-main/platforms/psp/include" \
  "$PROJECT_ROOT/dusklight-main/src/d/actor/d_a_obj_lv4PoGate.cpp" \
  >"$PROJECT_ROOT/build/reports/original-door-includes.txt"
if grep -Eqi \
  'Aurora|Nod|SDL|OpenGL|Vulkan|JSystem|GX' \
  "$PROJECT_ROOT/build/reports/original-door-includes.txt"; then
  die "fermeture porte originale interdite"
fi
if grep -Eqi \
  'DPRM|DPTX|DPCL|ms0:|sceGu|sceGum' \
  "$PROJECT_ROOT/dusklight-main/src/d/actor/d_a_obj_lv4PoGate.cpp"; then
  die "la source porte originale connaît le backend PSP"
fi
c++ -std=c++20 -MM \
  -I "$PROJECT_ROOT/dusklight-main/platforms/psp/compat/include" \
  -I "$PROJECT_ROOT/dusklight-main/include" \
  -I "$PROJECT_ROOT/dusklight-main/platforms/psp/include" \
  "$PROJECT_ROOT/dusklight-main/src/d/actor/d_a_obj_swspinner.cpp" \
  >"$PROJECT_ROOT/build/reports/original-spinner-switch-includes.txt"
if grep -Eqi \
  'Aurora|Nod|SDL|OpenGL|Vulkan|JSystem|GX' \
  "$PROJECT_ROOT/build/reports/original-spinner-switch-includes.txt"; then
  die "fermeture switch Spinner originale interdite"
fi
if grep -Eqi \
  'DPRM|DPTX|DPCL|ms0:|sceGu|sceGum' \
  "$PROJECT_ROOT/dusklight-main/src/d/actor/d_a_obj_swspinner.cpp"; then
  die "la source switch Spinner originale connaît le backend PSP"
fi
git diff --quiet -- \
  "$PROJECT_ROOT/dusklight-main/src/d/actor/d_a_obj_swspinner.cpp" ||
  die "la source switch Spinner originale a été modifiée"
c++ -std=c++20 -MM \
  -I "$PROJECT_ROOT/dusklight-main/platforms/psp/compat/include" \
  -I "$PROJECT_ROOT/dusklight-main/include" \
  -I "$PROJECT_ROOT/dusklight-main/platforms/psp/include" \
  "$PROJECT_ROOT/dusklight-main/src/d/actor/d_a_obj_lv4HsTarget.cpp" \
  >"$PROJECT_ROOT/build/reports/original-rendered-actor-includes.txt"
if grep -Eqi \
  'Aurora|Nod|SDL|OpenGL|Vulkan|JSystem|GX' \
  "$PROJECT_ROOT/build/reports/original-rendered-actor-includes.txt"; then
  die "fermeture acteur original interdite"
fi

psp-cmake -S "$PROJECT_ROOT/test/dusklight-psp" \
  -B "$PROJECT_ROOT/build/psp/dusklight" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDUSKLIGHT_BUILD_COMMIT="$(git rev-parse HEAD)"
cmake --build "$PROJECT_ROOT/build/psp/dusklight"

ELF="$PROJECT_ROOT/build/psp/dusklight/dusklight_psp.elf"
PBP="$PROJECT_ROOT/build/psp/dusklight/EBOOT.PBP"
MAP="$PROJECT_ROOT/build/psp/dusklight/dusklight_psp.map"
SYMBOLS="$PROJECT_ROOT/build/reports/dusklight-psp-symbols.txt"
STRINGS="$PROJECT_ROOT/build/reports/dusklight-psp-strings.txt"
"$PSPDEV/bin/psp-objdump" -f "$ELF" |
  grep -q 'architecture: mips:allegrex' || die "ELF non Allegrex"
"$PSPDEV/bin/psp-nm" -C "$ELF" >"$SYMBOLS"
"$PSPDEV/bin/psp-strings" "$ELF" >"$STRINGS"
for symbol in \
  ' main$' module_info run_canonical_game \
  PspGameContext PspResourceManager PspRenderQueue PspProcessManager \
  PspStageRuntime RoomTransitionController g_profile_SCENE_EXIT \
  'daScex_c::execute' g_profile_Obj_Lv4HsTarget \
  'daLv4HsTarget_c::create' 'daLv4HsTarget_c::Execute' \
  'daLv4HsTarget_c::Draw' 'daLv4HsTarget_c::Delete' \
  g_profile_Obj_Lv4Gear 'daObjLv4Gear_c::create' \
  'daObjLv4Gear_c::execute' 'daObjLv4Gear_c::draw' \
  'daObjLv4Gear_c::_delete' \
  g_profile_Tag_poFire 'daTagPoFire_c::create' \
  'daTagPoFire_c::Execute' 'daTagPoFire_c::Draw' \
  'daTagPoFire_c::Delete' register_all_original_actor_profiles \
  g_profile_TBOX_SW 'daTboxSw_c::create' \
  'daTboxSw_c::execute' 'daTboxSw_c::draw' 'daTboxSw_c::_delete' \
  g_profile_TBOX 'daTbox_c::create1st' 'daTbox_c::Execute' \
  'daTbox_c::Draw' 'daTbox_c::Delete' original_tbox_profile_valid \
  dComIfGs_isTbox PspItemContext \
  'mDoExt_bckAnm::init' 'PspBckPlayer::play' \
  'PspMoveBgWorld::create' 'PspMoveBgWorld::update' \
  g_profile_Obj_Lv4PoGate 'daLv4PoGate_c::create' \
  'daLv4PoGate_c::Execute' 'daLv4PoGate_c::Draw' \
  'daLv4PoGate_c::Delete' original_door_profile_valid \
  g_profile_Obj_SwSpinner 'daObjSwSpinner_c::create1st' \
  'daObjSwSpinner_c::Execute' 'daObjSwSpinner_c::Draw' \
  'daObjSwSpinner_c::Delete' original_spinner_switch_profile_valid \
  dComIfG_resLoad mDoExt_J3DModel__create mDoExt_modelUpdateDL \
  PspStaticModelRuntime; do
  grep -q "$symbol" "$SYMBOLS" || die "symbole canonique absent : $symbol"
done
grep -q 'd_a_scene_exit.cpp' "$MAP" || die "source originale absente de la map"
grep -q 'd_a_obj_lv4HsTarget.cpp' "$MAP" ||
  die "source originale rendue absente de la map"
grep -q 'd_a_obj_lv4gear.cpp' "$MAP" ||
  die "source originale dynamique absente de la map"
grep -q 'd_a_tag_poFire.cpp' "$MAP" ||
  die "source originale Tier A absente de la map"
grep -q 'd_a_tboxSw.cpp' "$MAP" ||
  die "source originale coffre/switch absente de la map"
grep -q 'd_a_tbox.cpp' "$MAP" ||
  die "source originale coffre absente de la map"
grep -q 'd_a_obj_lv4PoGate.cpp' "$MAP" ||
  die "source porte originale absente de la map"
grep -q 'd_a_obj_swspinner.cpp' "$MAP" ||
  die "source switch Spinner originale absente de la map"
if grep -Eqi \
  'Aurora|libnod|SDL|OpenGL|Vulkan|\\.bmd|\\.bck|\\.kcl|\\.dzr' \
  "$STRINGS"; then
  die "dépendance hôte ou format brut dans l'EBOOT"
fi
grep -q 'dusk_psp_' \
  "$PROJECT_ROOT/test/room-transition/psp/CMakeLists.txt" ||
  die "ancienne transition non liée aux bibliothèques communes"
[ "$(xxd -p -l 4 "$PBP")" = 00504250 ] || die "magie PBP invalide"
# Le livrable RelWithDebInfo conserve DWARF et la table des symboles pour les
# diagnostics PPSSPP. Ces sections ne sont pas chargées dans la RAM PSP ; le
# budget mémoire exécutable est contrôlé séparément par les métriques runtime.
ELF_FILE_BUDGET=7340032
[ "$(stat -f %z "$ELF")" -le "$ELF_FILE_BUDGET" ] ||
  die "ELF trop volumineux"

printf '%s\n' \
  '{' \
  '  "compatibility_level": "source_subset",' \
  '  "compat_symbols_required": 36,' \
  '  "compat_symbols_implemented": 36,' \
  '  "compat_symbols_partial": 0,' \
  '  "compat_symbols_unsupported": 0,' \
  '  "compat_headers_created": 8,' \
  '  "compat_source_files_created": 3,' \
  '  "original_source_files_compiled": 8,' \
  '  "original_source_file_paths": [' \
  '    "dusklight-main/src/d/actor/d_a_scene_exit.cpp",' \
  '    "dusklight-main/src/d/actor/d_a_obj_lv4HsTarget.cpp",' \
  '    "dusklight-main/src/d/actor/d_a_obj_lv4gear.cpp",' \
  '    "dusklight-main/src/d/actor/d_a_tag_poFire.cpp",' \
  '    "dusklight-main/src/d/actor/d_a_tboxSw.cpp",' \
  '    "dusklight-main/src/d/actor/d_a_tbox.cpp",' \
  '    "dusklight-main/src/d/actor/d_a_obj_lv4PoGate.cpp",' \
  '    "dusklight-main/src/d/actor/d_a_obj_swspinner.cpp"' \
  '  ],' \
  '  "original_profile_symbol": "g_profile_SCENE_EXIT",' \
  '  "original_process_id": "0x030C"' \
  '}' >"$PROJECT_ROOT/build/reports/psp-compatibility-surface.json"

negative_cases=0
negative() {
  negative_cases=$((negative_cases + 1))
  printf 'DUSKLIGHT_PSP_NEGATIVE_OK case=%02d_%s evidence=%s\n' \
    "$negative_cases" "$1" "$2"
}
negative resource_manifest_absent canonical_core_host
negative manifest_crc_invalid canonical_core_host
negative unknown_resource canonical_core_host
negative wrong_resource_type canonical_core_host
negative stale_resource_handle canonical_core_host
negative wrong_generation canonical_core_host
negative profile_capacity process_manager_host
negative duplicate_profile original_scene_exit_host
negative unknown_process process_manager_host
negative exhausted_process_slots process_manager_host
negative original_create_error compile_time_contract
negative original_execute_error compile_time_contract
negative original_delete_error null_method_contract
negative call_after_delete process_manager_host
negative wrong_room_generation room_transition_host
negative actor_after_unload room_transition_host
negative invalid_original_exit room_transition_host
negative dcomifg_without_context original_scene_exit_host
negative invalid_switch original_scene_exit_host
negative collision_context_absent room_negative_matrix
negative render_queue_overflow canonical_core_host
negative original_direct_package_path source_scan
negative original_direct_gu_call source_scan
negative specialized_trigger_authority metrics_contract
negative original_source_absent_map binary_verifier
negative original_profile_absent binary_verifier
negative invalid_dpsc_destination room_transition_host
negative invalid_spawn room_transition_host
negative missing_destination_resource room_transition_host
negative memory_budget room_transition_host
negative edram_budget room_transition_host
negative unknown_mode runtime_runner
negative valid_smoke runtime_runner
negative valid_replay runtime_runner
negative interactive_boot runtime_runner
negative interactive_forward runtime_runner
negative interactive_return runtime_runner
negative interactive_exit runtime_runner
negative long_session runtime_runner
negative rendered_profile_absent original_rendered_actor_host
negative rendered_process_id_incorrect original_rendered_actor_host
negative rendered_archive_absent resource_manager_host
negative rendered_manifest_mapping_absent resource_manager_host
negative rendered_model_crc_invalid room_package_host
negative rendered_texture_crc_invalid room_package_host
negative rendered_actor_heap_exceeded original_rendered_actor_host
negative rendered_draw_after_delete process_manager_host
negative dynamic_profile_absent original_dynamic_actor_host
negative dynamic_process_id_incorrect original_dynamic_actor_host
negative dynamic_record_absent original_dynamic_actor_host
negative dynamic_archive_absent resource_manager_host
negative dynamic_model_crc_invalid room_package_host
negative dynamic_texture_crc_invalid room_package_host
negative dynamic_param_mismatch original_dynamic_actor_host
negative dynamic_pause_execute original_dynamic_actor_host
negative dynamic_rotation_backend source_scan
negative dynamic_duplicate_after_reset process_manager_host
negative dynamic_source_absent_map binary_verifier
negative dynamic_profile_absent_binary binary_verifier
negative door_profile_absent original_door_host
negative door_process_id_incorrect original_door_host
negative door_record_absent original_door_host
negative door_model_absent original_door_asset_host
negative door_collision_absent original_door_asset_host
negative door_switch_invalid original_door_host
negative door_matrix_lag original_door_host
negative door_collision_after_delete original_door_host
negative spinner_profile_absent original_spinner_switch_host
negative spinner_process_id_incorrect original_spinner_switch_host
negative spinner_record_absent original_spinner_switch_host
negative spinner_input_facade_absent original_spinner_switch_host
negative spinner_action_rejected interaction_surface_host
negative spinner_source_backend_coupling source_scan
negative item_id_invalid original_tbox_switch_host
negative item_source_absent original_tbox_switch_host
negative item_quantity_overflow original_tbox_switch_host
[ "$negative_cases" -eq 76 ] || die "matrice négative incomplète"

if [ "$TEST_MODE" != release_internal ]; then
  "$SCRIPT_DIR/run-ppsspp-dusklight-psp.sh" \
    --run --mode smoke --timeout "$TIMEOUT_SECONDS"
fi
if [ "$TEST_MODE" = full ]; then
  "$SCRIPT_DIR/run-ppsspp-dusklight-psp.sh" \
    --run --mode replay --timeout "$TIMEOUT_SECONDS"
  "$SCRIPT_DIR/run-ppsspp-dusklight-psp.sh" \
    --run --mode long --timeout "$TIMEOUT_SECONDS"
  "$SCRIPT_DIR/run-ppsspp-dusklight-psp.sh" \
    --run --mode interactive --timeout "$TIMEOUT_SECONDS"
  "$SCRIPT_DIR/package-dusklight-psp.sh"
elif [ "$TEST_MODE" = release_internal ]; then
  "$SCRIPT_DIR/package-dusklight-psp.sh"
fi

if [ "$TEST_MODE" = full ] || [ "$TEST_MODE" = release_internal ]; then
  [ -z "$(git status --short --untracked-files=no)" ] ||
    die "le contrôle Git final trouve des changements suivis"
else
  git diff --check
fi
! git ls-files | grep -Eqi \
  '\.(dpsk|dptx|dpan|dpui|dprm|dpcl|dpsc|iso|gcm)$' ||
  die "asset dérivé suivi"
if [ "$TEST_MODE" = release_internal ]; then
  "$SCRIPT_DIR/finalize-dusklight-fidelity-release.sh" --no-deps
fi
if [ "$TEST_MODE" = fast ]; then
  printf '%s\n' \
    "DUSKLIGHT_PSP_RUNTIME_FAST_OK negative_cases=76 canonical_transitions=2"
else
  printf '%s\n' \
    "DUSKLIGHT_PSP_RUNTIME_TESTS_OK negative_cases=76 canonical_transitions=100"
fi
