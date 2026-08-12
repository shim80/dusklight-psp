#!/usr/bin/env bash

test_graph_main() {
  local target="" mode="" no_cache=false force=false explain=false
  while [ "$#" -gt 0 ]; do
    case "$1" in
      --target) shift; target="${1:-}" ;;
      --fast) mode=fast ;;
      --checkpoint)
        shift
        case "${1:-}" in
          fidelity) mode=checkpoint_fidelity ;;
          root-anchor) mode=checkpoint_root_anchor ;;
          environment) mode=checkpoint_environment ;;
          shadows) mode=checkpoint_shadows ;;
          idle-lighting) mode=checkpoint_idle_lighting ;;
          parity-link) mode=checkpoint_parity_link ;;
          parity-objects) mode=checkpoint_parity_objects ;;
          parity-scenes) mode=checkpoint_parity_scenes ;;
          parity-global) mode=checkpoint_parity_global ;;
          *) die "checkpoint inconnu" ;;
        esac
        ;;
      --release) mode=release ;;
      --no-cache) no_cache=true ;;
      --force) force=true ;;
      --explain-plan) explain=true ;;
      *) die "option d'orchestration inconnue : $1" ;;
    esac
    shift
  done
  [ -z "${DUSKLIGHT_ORCHESTRATOR_ACTIVE:-}" ] ||
    die "nested_orchestrator_invocations=1"
  export DUSKLIGHT_ORCHESTRATOR_ACTIVE=1
  local cache="$PROJECT_ROOT/.test-cache/dusklight-psp"
  local log="$PROJECT_ROOT/logs/test-orchestration/invocations.jsonl"
  mkdir -p "$cache" "$(dirname "$log")"
  local campaign
  campaign="$(date -u +%Y%m%dT%H%M%SZ)-$$"
  export DUSKLIGHT_TEST_CAMPAIGN="$campaign"
  export DUSKLIGHT_TEST_SUITE_MODE="${mode:-target}"
  local planned="|"
  local executed="|"
  local -a order=()

  node_deps() {
    case "$1" in
      host.ppsspp.gui_broker)
        echo "host.ppsspp.gui_runner parity.build.identity" ;;
      host.ppsspp_gui_broker.ready) echo host.ppsspp.gui_broker ;;
      psp.ppsspp.gui_runner.selftest) echo host.ppsspp_gui_broker.ready ;;
      host.link.pivot|host.link.forward_axis)
        echo host.link.coordinate_pipeline ;;
      host.link.locomotion)
        echo "host.link.forward_axis host.link.pivot" ;;
      host.link.root_anchor) echo host.link.coordinate_pipeline ;;
      host.ui.layout_compile) echo host.ui.asset_inventory ;;
      psp.canonical.build)
        echo "host.link.locomotion host.ui.layout_compile" ;;
      psp.canonical.smoke)
        echo "psp.canonical.build host.ppsspp_gui_broker.ready" ;;
      psp.canonical.replay|psp.canonical.interactive)
        echo "psp.canonical.build host.ppsspp_gui_broker.ready" ;;
      psp.canonical.fidelity_review) echo psp.canonical.smoke ;;
      psp.rendering.release)
        echo "psp.shadow.projected psp.canonical.fidelity_review" ;;
      psp.link.root_anchor)
        echo "psp.canonical.smoke host.link.root_anchor" ;;
      host.environment.runtime) echo host.environment.audit ;;
      host.shadow.audit) echo host.environment.audit ;;
      host.shadow.simple)
        echo "host.shadow.audit host.environment.runtime" ;;
      psp.shadow.simple)
        echo "psp.canonical.smoke host.shadow.simple" ;;
      host.shadow.projected) echo host.shadow.simple ;;
      psp.shadow.projected)
        echo "psp.shadow.simple host.shadow.projected" ;;
      psp.environment.checkpoint)
        echo "psp.canonical.smoke host.environment.runtime" ;;
      host.link.waits_dpan_parity)
        echo host.link.waits_j3d_reference ;;
      host.link.idle_foot_slip)
        echo host.link.waits_dpan_parity ;;
      host.render.normal_pipeline)
        echo host.render.color_packing ;;
      host.render.material_lighting)
        echo host.render.normal_pipeline ;;
      host.render.light_space)
        echo host.render.material_lighting ;;
      host.render.gu_state_isolation)
        echo host.render.light_space ;;
      psp.link.idle_review)
        echo "psp.canonical.build host.ppsspp_gui_broker.ready host.link.idle_foot_slip host.render.gu_state_isolation" ;;
      psp.render.diagnostic_matrix)
        echo psp.link.idle_review ;;
      psp.render.shadow_state_isolation)
        echo psp.link.idle_review ;;
      psp.idle_lighting.checkpoint)
        echo "psp.render.diagnostic_matrix psp.render.shadow_state_isolation" ;;
      psp.idle_lighting.release)
        echo psp.idle_lighting.checkpoint ;;
      desktop.parity.trace.v3)
        echo desktop.parity.inventory ;;
      desktop.parity.link)
        echo desktop.parity.trace.v3 ;;
      psp.parity.trace.v3)
        echo "psp.canonical.build host.ppsspp_gui_broker.ready desktop.parity.trace.v3" ;;
      psp.parity.link)
        echo "psp.parity.trace.v3 desktop.parity.link" ;;
      parity.coordinates)
        echo "desktop.parity.trace.v3 host.link.coordinate_pipeline" ;;
      parity.link.behavior)
        echo "parity.coordinates psp.parity.link" ;;
      parity.object.pivots)
        echo parity.coordinates ;;
      parity.rooms)
        echo parity.coordinates ;;
      parity.actors)
        echo parity.rooms ;;
      parity.actor.lifecycle)
        echo "parity.actors parity.object.pivots" ;;
      parity.geyser)
        echo parity.actor.lifecycle ;;
      parity.ui)
        echo parity.geyser ;;
      parity.camera)
        echo parity.ui ;;
      parity.scene.behavior)
        echo parity.camera ;;
      parity.actor.matrix)
        echo parity.scene.behavior ;;
      parity.render)
        echo parity.actor.matrix ;;
      parity.performance)
        echo "parity.render parity.build.identity" ;;
      parity.review)
        echo parity.performance ;;
      psp.canonical.stress) echo psp.canonical.replay ;;
      psp.release.full)
        echo "psp.ppsspp.gui_runner.selftest psp.historical.core_smoke psp.link.root_anchor psp.environment.checkpoint psp.rendering.release psp.idle_lighting.release psp.canonical.replay psp.canonical.stress psp.canonical.interactive" ;;
    esac
  }
  plan_node() {
    local node="$1" dep
    case "$planned" in *"|$node|"*) return ;; esac
    for dep in $(node_deps "$node"); do plan_node "$dep"; done
    planned="${planned}${node}|"
    order+=("$node")
  }
  case "$mode" in
    fast)
      plan_node host.link.locomotion
      plan_node host.ui.layout_compile
      plan_node psp.canonical.build ;;
    checkpoint_fidelity)
      plan_node psp.canonical.fidelity_review ;;
    checkpoint_root_anchor)
      plan_node psp.link.root_anchor ;;
    checkpoint_environment)
      plan_node psp.environment.checkpoint ;;
    checkpoint_shadows)
      plan_node psp.shadow.projected ;;
    checkpoint_idle_lighting)
      plan_node psp.idle_lighting.checkpoint ;;
    checkpoint_parity_link)
      plan_node parity.link.behavior ;;
    checkpoint_parity_objects)
      plan_node parity.object.pivots ;;
    checkpoint_parity_scenes)
      plan_node parity.scene.behavior ;;
    checkpoint_parity_global)
      plan_node parity.review ;;
    release)
      plan_node psp.release.full ;;
    "")
      [ -n "$target" ] || die "--target exige un identifiant"
      plan_node "$target" ;;
    *) die "mode de graphe invalide" ;;
  esac
  export DUSKLIGHT_TEST_NODES_TOTAL="${#order[@]}"

  node_inputs() {
    case "$1" in
      parity.build.identity)
        echo "build/psp/dusklight/dusklight_psp.elf build/psp/dusklight/EBOOT.PBP build/assets/dusklight-psp/data artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP scripts/generate-parity-build-identity.sh scripts/test-parity-build-identity.sh" ;;
      host.ppsspp.gui_broker)
        echo "tools/macos/dusklight-ppsspp-gui-broker tools/macos/ppsspp-gui-runner/runner.py scripts/build-ppsspp-gui-broker.sh scripts/start-ppsspp-gui-broker.sh scripts/status-ppsspp-gui-broker.sh scripts/stop-ppsspp-gui-broker.sh scripts/submit-ppsspp-gui-request.sh scripts/test-ppsspp-gui-broker.sh build/reports/PARITY_BUILD_ID.metrics" ;;
      host.ppsspp_gui_broker.ready)
        echo "scripts/ensure-ppsspp-gui-broker.sh scripts/status-ppsspp-gui-broker.sh .test-data/ppsspp-gui-broker/heartbeat.json" ;;
      host.ppsspp.gui_runner)
        echo "tools/macos/ppsspp-gui-runner scripts/ppsspp-gui-runner-build.sh scripts/ppsspp-gui-runner-status.sh scripts/ppsspp-gui-runner-request.sh scripts/ppsspp-gui-runner-selftest.sh scripts/test-ppsspp-gui-runner-host.sh" ;;
      desktop.parity.inventory)
        echo "tools/dusk_parity_inventory reference/parity/scenarios/scenarios.toml build/assets/dusklight-psp/data/RESOURCE.MANIFEST" ;;
      desktop.parity.trace.v3)
        echo "reference/parity/dtrc-v3.schema.json reference/parity/tolerances.toml dusklight-main/platforms/psp/src/parity_trace.cpp test/canonical-runtime/parity_trace_host_test.cpp tools/dusk_parity_compare scripts/test-parity-trace-v3.sh" ;;
      desktop.parity.link)
        echo "reference/parity/scenarios reference/desktop/patches scripts/run-dusklight-desktop-reference.sh scripts/run-dusklight-desktop-link-parity.sh tools/dusk_desktop_parity_trace" ;;
      psp.parity.trace.v3)
        echo "test/room-transition/main.cpp dusklight-main/platforms/psp/include/dusk/psp/parity_trace.hpp dusklight-main/platforms/psp/src/parity_trace.cpp scripts/run-ppsspp-dusklight-psp.sh tools/macos/ppsspp-gui-runner" ;;
      psp.parity.link)
        echo "build/psp/dusklight/EBOOT.PBP build/assets/dusklight-psp/data scripts/run-dusklight-link-parity-suite.sh" ;;
      parity.coordinates)
        echo "docs/design/dusklight-psp-coordinate-space-contract.md dusklight-main/platforms/psp/include/dusk/psp/link_fidelity.hpp test/canonical-runtime/host_link_fidelity_test.cpp scripts/verify-coordinate-space-contract.sh" ;;
      parity.link.behavior)
        echo "tools/dusk_parity_compare reference/parity/tolerances.toml build/reports/parity scripts/run-dusklight-link-parity-suite.sh" ;;
      parity.object.pivots)
        echo "tools/dusk_object_pivot_audit scripts/test-object-pivot-audit.sh dusklight-main/src/d/actor dusklight-main/platforms/psp/src/model_runtime.cpp build/assets/dusklight-psp/data/RESOURCE.MANIFEST" ;;
      parity.rooms)
        echo "scripts/test-room-model-collision-parity.sh test/canonical-runtime/room_model_collision_parity_host_test.cpp dusklight-main/platforms/psp/src/room_package.cpp dusklight-main/platforms/psp/src/room_collision.cpp build/assets/dusklight-psp/data/stages" ;;
      parity.actors)
        echo "tools/dusk_f_sp108_adapter_audit scripts/test-f-sp108-adapter-parity.sh dusklight-main/platforms/psp/src/actor_runtime.cpp dusklight-main/platforms/psp/include/dusk/psp/actor_runtime.hpp artifacts/dusklight-desktop-oracle-v2/f_sp108_actor_activation.csv build/assets/dusklight-psp/data/stages/F_SP108/R01/room.dpsc" ;;
      parity.actor.lifecycle)
        echo "reference/parity/original-source-actor-matrix.csv scripts/test-original-actor-parity-matrix.sh dusklight-main/src/d/actor build/psp/dusklight/dusklight_psp.map" ;;
      parity.geyser)
        echo "scripts/test-geyser-parity.sh test/real-actor/host_actor_runtime_test.cpp dusklight-main/src/d/actor/d_a_obj_geyser.cpp dusklight-main/platforms/psp/src/actor_runtime.cpp build/assets/first-real-actor" ;;
      parity.ui)
        echo "reference/parity/startup-ui-parity.csv scripts/test-startup-ui-transform-parity.sh test/canonical-runtime/startup_runtime_host_test.cpp test/canonical-runtime/startup_ui_host_test.cpp test/canonical-runtime/startup_title_asset_host_test.cpp test/canonical-runtime/startup_camera_parity_host_test.cpp build/assets/dusklight-psp/data/startup build/assets/dusklight-psp/data/common/hud.dpui" ;;
      parity.camera)
        echo "reference/parity/camera-parity-matrix.csv scripts/test-camera-parity-matrix.sh dusklight-main/platforms/psp/src/startup_camera.cpp dusklight-main/platforms/psp/src/real_room_runtime.cpp dusklight-main/platforms/psp/src/room_collision.cpp" ;;
      parity.scene.behavior)
        echo "reference/parity/scenarios tools/dusk_scene_parity_matrix scripts/test-scene-parity-matrix.sh build/reports/parity build/psp/dusklight/EBOOT.PBP" ;;
      parity.actor.matrix)
        echo "tools/dusk_actor_parity_matrix scripts/test-actor-parity-matrix.sh build/assets/dusklight-psp/data/stages build/assets/first-real-actor/room.dpsc" ;;
      parity.render)
        echo "tools/dusk_visual_parity_review scripts/test-visual-parity-review.sh" ;;
      parity.performance)
        echo "scripts/test-parity-performance-provenance.sh artifacts/dusklight-psp-benchmarks artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP/BENCHMARK_V2_1.METRICS build/psp/dusklight/EBOOT.PBP" ;;
      parity.review)
        echo "scripts/stage-global-parity-review.sh build/reports/PARITY_SCENE_MATRIX.csv build/reports/PARITY_ACTOR_MATRIX.csv build/reports/parity-performance-status.metrics docs/reports" ;;
      psp.ppsspp.gui_runner.selftest)
        echo "build/psp/smoke/EBOOT.PBP artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP build/psp/dusklight/EBOOT.PBP build/assets/dusklight-psp/data .tools/ppsspp/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL scripts/ppsspp-gui-runner-selftest.sh" ;;
      host.link.coordinate_pipeline)
        echo "dusklight-main/src/d/actor/d_a_alink.cpp dusklight-main/platforms/psp/include/dusk/psp/link_fidelity.hpp test/canonical-runtime/host_link_fidelity_test.cpp" ;;
      host.link.pivot|host.link.forward_axis)
        echo "dusklight-main/platforms/psp/include/dusk/psp/link_fidelity.hpp test/canonical-runtime/host_link_fidelity_test.cpp" ;;
      host.link.locomotion)
        echo "dusklight-main/platforms/psp/src/real_room_runtime.cpp dusklight-main/platforms/psp/include/dusk/psp/link_fidelity.hpp test/canonical-runtime/host_link_fidelity_test.cpp" ;;
      host.link.root_anchor)
        echo "test/link-playable/host_root_anchor_reference_test.cpp dusklight-main/platforms/psp/src/playable_runtime.cpp scripts/analyze-link-root-anchor.sh" ;;
      host.ui.asset_inventory)
        echo "dusklight-main/src/d/d_meter2.cpp dusklight-main/src/d/d_meter2_draw.cpp tools/dusk_link_loader_probe/src/playable_export.cpp scripts/build-link-playable-assets.sh" ;;
      host.ui.layout_compile)
        echo "tools/dusk_link_loader_probe/src/playable_export.cpp dusklight-main/platforms/psp/src/playable_package.cpp build/assets/link-playable/hud.dpui" ;;
      psp.canonical.build)
        echo "test/dusklight-psp test/room-transition/main.cpp dusklight-main/platforms/psp scripts/build-dusklight-psp-assets.sh" ;;
      psp.canonical.smoke|psp.canonical.replay|psp.canonical.stress|psp.canonical.interactive|psp.canonical.fidelity_review)
        echo "build/psp/dusklight/EBOOT.PBP build/assets/dusklight-psp/data test/link-playable/ppsspp-accelerated.ini test/gu-smoke/ppsspp-software.ini scripts/run-ppsspp-dusklight-psp.sh scripts/lib/ppsspp-host-backend.sh tools/macos/ppsspp-gui-runner scripts/ppsspp-gui-runner-request.sh" ;;
      psp.rendering.release)
        echo ".test-data/ppsspp/captures/render-review build/psp/dusklight/EBOOT.PBP build/assets/dusklight-psp/data/RESOURCE.MANIFEST scripts/package-dusklight-render-review.sh" ;;
      psp.link.root_anchor)
        echo "build/psp/dusklight/EBOOT.PBP scripts/verify-link-root-anchor-checkpoint.sh" ;;
      host.environment.audit)
        echo "dusklight-main/include/d/d_kankyo.h dusklight-main/include/d/d_stage.h dusklight-main/src/d/d_kankyo.cpp tools/dusk_link_loader_probe/src/playable_export.cpp scripts/verify-environment-source-audit.sh" ;;
      host.environment.runtime)
        echo "dusklight-main/platforms/psp/include/dusk/psp/environment_runtime.hpp dusklight-main/platforms/psp/src/environment_runtime.cpp test/room-transition/environment_runtime_host_test.cpp scripts/test-environment-runtime.sh build/assets/room-transition/stages/D_MN10" ;;
      host.shadow.audit)
        echo "dusklight-main/include/d/d_drawlist.h dusklight-main/src/d/d_drawlist.cpp dusklight-main/include/SSystem/SComponent/c_bg_s_shdw_draw.h scripts/verify-shadow-source-audit.sh" ;;
      host.shadow.simple)
        echo "dusklight-main/platforms/psp/include/dusk/psp/shadow_runtime.hpp dusklight-main/platforms/psp/src/shadow_runtime.cpp dusklight-main/platforms/psp/src/room_collision.cpp test/room-transition/shadow_runtime_host_test.cpp scripts/test-shadow-simple.sh build/assets/room-transition/stages/D_MN10" ;;
      psp.shadow.simple)
        echo "build/psp/dusklight/EBOOT.PBP scripts/verify-shadow-simple-checkpoint.sh" ;;
      host.shadow.projected)
        echo "test/room-transition/shadow_projected_budget_host_test.cpp dusklight-main/platforms/psp/include/dusk/psp/shadow_runtime.hpp scripts/test-shadow-projected.sh" ;;
      psp.shadow.projected)
        echo "build/psp/dusklight/EBOOT.PBP scripts/verify-shadow-projected-checkpoint.sh" ;;
      psp.environment.checkpoint)
        echo "build/psp/dusklight/EBOOT.PBP scripts/verify-environment-checkpoint.sh" ;;
      host.link.waits_j3d_reference|host.link.waits_dpan_parity|host.link.idle_foot_slip)
        echo "build/assets/dusklight-psp/data/common test/link-playable dusklight-main/platforms/psp/src/playable_runtime.cpp" ;;
      host.render.color_packing|host.render.normal_pipeline|host.render.material_lighting|host.render.light_space|host.render.gu_state_isolation)
        echo "test/link-playable dusklight-main/platforms/psp/include/dusk/psp build/assets/dusklight-psp/data/common" ;;
      psp.link.idle_review)
        echo "build/psp/dusklight/EBOOT.PBP build/assets/dusklight-psp/data scripts/run-ppsspp-dusklight-psp.sh tools/macos/ppsspp-gui-runner" ;;
      psp.render.diagnostic_matrix)
        echo ".test-data/ppsspp/captures/idle-lighting-review tools/analyze_idle_lighting_frames.cpp tools/psp5650_to_ppm.cpp scripts/package-dusklight-idle-lighting-review.sh" ;;
      psp.render.shadow_state_isolation)
        echo ".test-data/ppsspp/captures/idle-lighting-review/SHADOW_STATE_ISOLATION.OK" ;;
      psp.idle_lighting.checkpoint)
        echo ".test-data/ppsspp/captures/idle-lighting-review scripts/verify-idle-lighting-checkpoint.sh" ;;
      psp.idle_lighting.release)
        echo ".test-data/ppsspp/checkpoints/idle-lighting build/psp/dusklight scripts/verify-idle-lighting-release.sh" ;;
      psp.historical.core_smoke)
        echo "build/psp/core-smoke/EBOOT.PBP scripts/run-ppsspp-core-smoke-gui.sh scripts/ppsspp-gui-runner-request.sh test/gu-smoke/ppsspp-software.ini" ;;
      psp.release.full)
        echo "scripts test dusklight-main/platforms/psp" ;;
    esac
  }
  fingerprint() {
    local node="$1" command input file command_file
    command="$(node_command "$node")"
    command_file="${command%% *}"
    {
      printf 'node=%s\n' "$node"
      printf 'command=%s\n' "$command"
      printf 'graph='
      shasum -a 256 "$PROJECT_ROOT/test/dusklight-psp/test-graph.toml"
      if [ -f "$PROJECT_ROOT/$command_file" ]; then
        shasum -a 256 "$PROJECT_ROOT/$command_file"
      fi
      for input in $(node_inputs "$node"); do
        if [ -f "$PROJECT_ROOT/$input" ]; then
          shasum -a 256 "$PROJECT_ROOT/$input"
        elif [ -d "$PROJECT_ROOT/$input" ]; then
          find "$PROJECT_ROOT/$input" -type f -print |
            LC_ALL=C sort |
            while IFS= read -r file; do
              shasum -a 256 "$file"
            done
        else
          printf 'missing=%s\n' "$input"
        fi
      done
      printf 'pspdev='
      "$PSPDEV/bin/psp-g++" --version 2>/dev/null | head -1 || true
      if [ "$node" = psp.canonical.build ]; then
        printf 'build_commit=%s\n' \
          "$(git -C "$PROJECT_ROOT" rev-parse HEAD)"
      fi
      if [[ "$node" == psp.* && "$node" != psp.canonical.build ]] &&
         [ -f "$PROJECT_ROOT/build/psp/dusklight/EBOOT.PBP" ]; then
        shasum -a 256 "$PROJECT_ROOT/build/psp/dusklight/EBOOT.PBP"
      fi
      if [[ "$node" == psp.* && "$node" != psp.canonical.build ]] &&
         [ -f "$PROJECT_ROOT/build/assets/dusklight-psp/data/RESOURCE.MANIFEST" ]; then
        shasum -a 256 "$PROJECT_ROOT/build/assets/dusklight-psp/data/RESOURCE.MANIFEST"
      fi
      if [[ "$node" == psp.* && "$node" != psp.canonical.build ]] &&
         [ -f "$PROJECT_ROOT/.test-data/ppsspp/home/.config/ppsspp/PSP/SYSTEM/ppsspp.ini" ]; then
        shasum -a 256 \
          "$PROJECT_ROOT/.test-data/ppsspp/home/.config/ppsspp/PSP/SYSTEM/ppsspp.ini"
      fi
      if [[ "$node" == psp.* ]] &&
         [ -f "$PROJECT_ROOT/.tools/ppsspp/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL" ]; then
        printf 'ppsspp='
        shasum -a 256 \
          "$PROJECT_ROOT/.tools/ppsspp/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL"
        printf 'runner='
        shasum -a 256 \
          "$PROJECT_ROOT/tools/macos/ppsspp-gui-runner/runner.py"
        printf '%s\n' \
          "transport=auto" "backend=opengl" "renderer=software"
      fi
    } | shasum -a 256 | awk '{print $1}'
  }
  node_command() {
    case "$1" in
      host.ppsspp.gui_runner)
        echo "scripts/test-ppsspp-gui-runner-host.sh" ;;
      desktop.parity.inventory)
        echo "python3 -B tools/dusk_parity_inventory/dusk_parity_inventory.py --output build/reports/current-psp-parity-scope.json" ;;
      desktop.parity.trace.v3)
        echo "scripts/test-parity-trace-v3.sh" ;;
      desktop.parity.link)
        echo "scripts/run-dusklight-desktop-link-parity.sh --run" ;;
      psp.parity.trace.v3)
        echo "cmake --build build/psp/dusklight" ;;
      psp.parity.link)
        echo "scripts/run-dusklight-link-parity-suite.sh --run-psp" ;;
      parity.coordinates)
        echo "scripts/verify-coordinate-space-contract.sh" ;;
      parity.link.behavior)
        echo "scripts/run-dusklight-link-parity-suite.sh --compare-existing" ;;
      parity.object.pivots)
        echo "scripts/test-object-pivot-audit.sh" ;;
      parity.rooms)
        echo "scripts/test-room-model-collision-parity.sh" ;;
      parity.actors)
        echo "scripts/test-f-sp108-adapter-parity.sh" ;;
      parity.actor.lifecycle)
        echo "scripts/test-original-actor-parity-matrix.sh" ;;
      parity.geyser)
        echo "scripts/test-geyser-parity.sh" ;;
      parity.ui)
        echo "scripts/test-startup-ui-transform-parity.sh" ;;
      parity.camera)
        echo "scripts/test-camera-parity-matrix.sh" ;;
      parity.scene.behavior)
        echo "scripts/test-scene-parity-matrix.sh" ;;
      parity.actor.matrix)
        echo "scripts/test-actor-parity-matrix.sh" ;;
      parity.render)
        echo "scripts/test-visual-parity-review.sh" ;;
      parity.build.identity)
        echo "scripts/test-parity-build-identity.sh" ;;
      host.ppsspp.gui_broker)
        echo "scripts/test-ppsspp-gui-broker.sh" ;;
      host.ppsspp_gui_broker.ready)
        echo "scripts/ensure-ppsspp-gui-broker.sh" ;;
      parity.performance)
        echo "scripts/test-parity-performance-provenance.sh" ;;
      parity.review)
        echo "scripts/stage-global-parity-review.sh" ;;
      psp.ppsspp.gui_runner.selftest)
        # Fresh macOS GUI/OpenGL initialization can consume most of two
        # minutes before the PSP boot signal.  Keep the PSP smoke budget
        # independent from that bounded host startup latency.
        echo "scripts/ppsspp-gui-runner-selftest.sh --run --timeout 300" ;;
      host.link.coordinate_pipeline)
        echo "scripts/test-link-fidelity.sh --target coordinate --no-deps" ;;
      host.link.pivot)
        echo "scripts/test-link-fidelity.sh --target pivot --no-deps" ;;
      host.link.forward_axis)
        echo "scripts/test-link-fidelity.sh --target forward --no-deps" ;;
      host.link.locomotion)
        echo "scripts/test-link-fidelity.sh --target locomotion --no-deps" ;;
      host.link.root_anchor)
        echo "scripts/analyze-link-root-anchor.sh" ;;
      host.ui.asset_inventory)
        echo "scripts/analyze-link-fidelity.sh --section hud --no-deps" ;;
      host.ui.layout_compile)
        echo "build/host/link-playable/dpui_v2_host_test build/assets/link-playable/hud.dpui build/reports/original-hud-atlas.ppm" ;;
      psp.canonical.build)
        echo "scripts/test-dusklight-psp-runtime.sh --target-internal build --no-deps" ;;
      psp.canonical.smoke)
        echo "scripts/run-ppsspp-dusklight-psp.sh --run --mode smoke --presentation game --backend opengl --transport auto --timeout 600" ;;
      psp.link.root_anchor)
        echo "scripts/verify-link-root-anchor-checkpoint.sh" ;;
      host.environment.audit)
        echo "scripts/verify-environment-source-audit.sh" ;;
      host.environment.runtime)
        echo "scripts/test-environment-runtime.sh" ;;
      host.shadow.audit)
        echo "scripts/verify-shadow-source-audit.sh" ;;
      host.shadow.simple)
        echo "scripts/test-shadow-simple.sh" ;;
      psp.shadow.simple)
        echo "scripts/verify-shadow-simple-checkpoint.sh" ;;
      host.shadow.projected)
        echo "scripts/test-shadow-projected.sh" ;;
      psp.shadow.projected)
        echo "scripts/verify-shadow-projected-checkpoint.sh" ;;
      psp.environment.checkpoint)
        echo "scripts/verify-environment-checkpoint.sh" ;;
      host.link.waits_j3d_reference)
        echo "build/host/link-playable/playable_runtime_host_test build/assets/dusklight-psp/data/common/link.dpsk build/assets/dusklight-psp/data/common/link.dptx build/assets/dusklight-psp/data/common/link.dpan build/assets/dusklight-psp/data/common/hud.dpui" ;;
      host.link.waits_dpan_parity)
        echo "build/host/link-playable/link_dpan_pose_semantics_host_test build/assets/dusklight-psp/data/common/link.dpsk build/assets/dusklight-psp/data/common/link.dpan .tmp/waits-dpan.csv" ;;
      host.link.idle_foot_slip)
        echo "build/host/link-playable/link_idle_foot_slip_host_test build/assets/dusklight-psp/data/common/link.dpsk build/assets/dusklight-psp/data/common/link.dptx build/assets/dusklight-psp/data/common/link.dpan build/assets/dusklight-psp/data/common/hud.dpui" ;;
      host.render.color_packing)
        echo "build/host/link-playable/render_color_packing_host_test" ;;
      host.render.normal_pipeline)
        echo "build/host/link-playable/render_normal_pipeline_host_test build/assets/dusklight-psp/data/common/link.dpsk build/assets/dusklight-psp/data/common/link.dptx build/assets/dusklight-psp/data/common/link.dpan build/assets/dusklight-psp/data/common/hud.dpui" ;;
      host.render.material_lighting)
        echo "build/host/link-playable/render_material_lighting_host_test build/assets/dusklight-psp/data/common/link.dptx" ;;
      host.render.light_space)
        echo "build/host/link-playable/render_light_space_host_test" ;;
      host.render.gu_state_isolation)
        echo "build/host/link-playable/render_gu_state_isolation_host_test" ;;
      psp.link.idle_review)
        echo "scripts/run-ppsspp-dusklight-psp.sh --run --mode idle_lighting_review --presentation game --backend opengl --transport auto --timeout 300" ;;
      psp.render.diagnostic_matrix)
        echo "scripts/package-dusklight-idle-lighting-review.sh" ;;
      psp.render.shadow_state_isolation)
        echo "test -f .test-data/ppsspp/captures/idle-lighting-review/SHADOW_STATE_ISOLATION.OK" ;;
      psp.idle_lighting.checkpoint)
        echo "scripts/verify-idle-lighting-checkpoint.sh" ;;
      psp.idle_lighting.release)
        echo "scripts/verify-idle-lighting-release.sh" ;;
      psp.canonical.fidelity_review)
        echo "scripts/capture-dusklight-fidelity-review.sh --no-deps --backend opengl" ;;
      psp.rendering.release)
        echo "scripts/package-dusklight-render-review.sh" ;;
      psp.historical.core_smoke)
        echo "scripts/run-ppsspp-core-smoke-gui.sh" ;;
      psp.canonical.replay)
        echo "scripts/run-ppsspp-dusklight-psp.sh --run --mode replay --presentation game --backend opengl --transport auto --timeout 600" ;;
      psp.canonical.stress)
        echo "scripts/run-ppsspp-dusklight-psp.sh --run --mode long --presentation game --backend opengl --transport auto --timeout 1200" ;;
      psp.canonical.interactive)
        echo "scripts/run-ppsspp-dusklight-psp.sh --run --mode interactive --presentation game --backend opengl --transport auto --timeout 600" ;;
      psp.release.full)
        echo "scripts/test-dusklight-psp-runtime.sh --release-internal --no-deps" ;;
      *) die "test-id inconnu : $1" ;;
    esac
  }
  local node fp result_dir command cache_hit result start finish start_epoch
  local finish_epoch eboot_hash manifest_hash duration
  for node in "${order[@]}"; do
    fp="$(fingerprint "$node")"
    result_dir="$cache/$node/$fp"
    command="$(node_command "$node")"
    cache_hit=false
    if [ "$no_cache" = false ] && [ "$force" = false ] &&
       [ -f "$result_dir/success" ]; then
      cache_hit=true
    fi
    if [ "$explain" = true ]; then
      printf 'TEST_PLAN id=%s fingerprint=%s cache_hit=%s command=%s\n' \
        "$node" "$fp" "$cache_hit" "$command"
      continue
    fi
    case "$executed" in
      *"|$node:$fp|"*)
        die "duplicate_test_invocations=1 id=$node fingerprint=$fp" ;;
    esac
    executed="${executed}${node}:${fp}|"
    start="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    start_epoch="$(date +%s)"
    result=success
    if [ "$cache_hit" = false ]; then
      mkdir -p "$result_dir"
      if ! (cd "$PROJECT_ROOT" && eval "$command") \
          >"$result_dir/stdout.log" 2>"$result_dir/stderr.log"; then
        result=failure
      fi
      if [ "$result" = success ]; then
        printf '%s' "$fp" >"$result_dir/success"
      fi
    fi
    finish="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    finish_epoch="$(date +%s)"
    duration=$((finish_epoch - start_epoch))
    eboot_hash=""
    manifest_hash=""
    if [ -f "$PROJECT_ROOT/build/psp/dusklight/EBOOT.PBP" ]; then
      eboot_hash="$(shasum -a 256 "$PROJECT_ROOT/build/psp/dusklight/EBOOT.PBP" | awk '{print $1}')"
    fi
    if [ -f "$PROJECT_ROOT/build/assets/dusklight-psp/data/RESOURCE.MANIFEST" ]; then
      manifest_hash="$(shasum -a 256 "$PROJECT_ROOT/build/assets/dusklight-psp/data/RESOURCE.MANIFEST" | awk '{print $1}')"
    fi
    printf '{"fingerprint":"%s","result":"%s","started_at":"%s","finished_at":"%s","duration_seconds":%s,"artifacts":[],"logs":["stdout.log","stderr.log"],"eboot_sha256":"%s","package_sha256":"%s","ppsspp_config_sha256":"included_in_fingerprint"}\n' \
      "$fp" "$result" "$start" "$finish" "$duration" "$eboot_hash" \
      "$manifest_hash" >"$result_dir/result.json"
    printf '{"invocation_id":"%s","parent_invocation_id":null,"test_id":"%s","fingerprint":"%s","cache_hit":%s,"started_at":"%s","finished_at":"%s","result":"%s","ppsspp_launch":%s,"command":"%s","reason":"dependency_graph"}\n' \
      "$campaign" "$node" "$fp" "$cache_hit" "$start" "$finish" \
      "$result" "$([[ "$node" == psp.* && "$node" != psp.canonical.build && "$node" != psp.release.full ]] && echo true || echo false)" \
      "$command" >>"$log"
    printf 'TEST_RESULT id=%s fingerprint=%s cache_hit=%s result=%s\n' \
      "$node" "$fp" "$cache_hit" "$result"
    [ "$result" = success ] || {
      tail -40 "$result_dir/stderr.log" >&2 || true
      return 1
    }
  done
  [ "$explain" = true ] ||
    printf 'TEST_GRAPH_OK nodes=%s duplicate_test_invocations=0 nested_orchestrator_invocations=0 identical_ppsspp_launches_repeated=0\n' \
      "${#order[@]}"
}
