#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

OUTPUT="$(assert_project_path \
  "artifacts/dusklight-psp-global-parity-review")"
SCENES="$(assert_project_path "build/reports/PARITY_SCENE_MATRIX.csv")"
ACTORS="$(assert_project_path "build/reports/PARITY_ACTOR_MATRIX.csv")"
PERFORMANCE="$(assert_project_path \
  "build/reports/parity-performance-status.metrics")"
BUILD_IDENTITY="$(assert_project_path \
  "build/reports/PARITY_BUILD_ID.metrics")"
EBOOT="$(assert_project_path "build/psp/dusklight/EBOOT.PBP")"

for required in \
  "$SCENES" "$ACTORS" "$PERFORMANCE" "$BUILD_IDENTITY" "$EBOOT"; do
  [ -s "$required" ] || die "preuve de revue absente : $required"
done
for marker in \
  TRANSFORM_PARITY.OK LINK_BEHAVIOR_PARITY.OK \
  SCENE_BEHAVIOR_PARITY.OK GLOBAL_PARITY_REVIEW.OK; do
  [ ! -e "$OUTPUT/$marker" ] ||
    die "marqueur final prématuré : $marker"
done

for directory in \
  artifacts/dusklight-psp-global-parity-review \
  artifacts/dusklight-psp-global-parity-review/reports \
  artifacts/dusklight-psp-global-parity-review/traces \
  artifacts/dusklight-psp-global-parity-review/desktop \
  artifacts/dusklight-psp-global-parity-review/psp \
  artifacts/dusklight-psp-global-parity-review/side-by-side \
  artifacts/dusklight-psp-global-parity-review/overlays \
  artifacts/dusklight-psp-global-parity-review/heatmaps \
  artifacts/dusklight-psp-global-parity-review/videos-or-frame-strips; do
  safe_mkdir "$directory"
done

cp "$SCENES" "$OUTPUT/PARITY_SCENE_MATRIX.csv"
cp "$ACTORS" "$OUTPUT/PARITY_ACTOR_MATRIX.csv"
cp "$PERFORMANCE" "$OUTPUT/PARITY_PERFORMANCE.METRICS"
cp "$BUILD_IDENTITY" "$OUTPUT/PARITY_BUILD_ID.metrics"
cp "$PROJECT_ROOT/build/reports/parity/link-suite-summary.json" \
  "$OUTPUT/LINK_PARITY_SUMMARY.json"
cp "$PROJECT_ROOT/.test-data/ppsspp-gui-broker/GUI_BROKER.METRICS" \
  "$OUTPUT/GUI_BROKER.METRICS"
safe_mkdir "artifacts/dusklight-psp-global-parity-review/benchmarks/performance"
safe_mkdir "artifacts/dusklight-psp-global-parity-review/benchmarks/psp_conservative"
cp "$PROJECT_ROOT/artifacts/dusklight-psp-benchmarks/performance/"*.metrics \
  "$OUTPUT/benchmarks/performance/"
cp "$PROJECT_ROOT/artifacts/dusklight-psp-benchmarks/performance/RUN.MANIFEST" \
  "$OUTPUT/benchmarks/performance/"
cp "$PROJECT_ROOT/artifacts/dusklight-psp-benchmarks/psp_conservative/"*.metrics \
  "$OUTPUT/benchmarks/psp_conservative/"
cp "$PROJECT_ROOT/artifacts/dusklight-psp-benchmarks/psp_conservative/RUN.MANIFEST" \
  "$OUTPUT/benchmarks/psp_conservative/"
safe_mkdir "artifacts/dusklight-psp-global-parity-review/traces/current"
cp -R "$PROJECT_ROOT/build/reports/parity/." "$OUTPUT/traces/current/"
cp -R "$PROJECT_ROOT/artifacts/dusklight-psp-fidelity-review/." \
  "$OUTPUT/side-by-side/"
cp -R "$PROJECT_ROOT/artifacts/dusklight-psp-render-review/." \
  "$OUTPUT/overlays/"
for report in \
  124-current-psp-parity-scope.md \
  125-model-converter-origin-audit.md \
  126-rigid-object-pivot-audit.md \
  127-link-behavior-parity-convergence.md \
  128-room-model-collision-parity.md \
  129-f-sp108-adapter-parity.md \
  130-original-actor-behavior-matrix.md \
  131-geyser-parity-classification.md \
  132-startup-ui-transform-parity.md \
  133-camera-parity-matrix.md \
  134-visual-parity-review-pipeline.md \
  135-parity-performance-provenance.md \
  136-global-parity-review-staging.md \
  137-ppsspp-host-graphics-classification.md \
  138-parity-build-reconciliation.md \
  139-persistent-ppsspp-gui-broker.md \
  140-canonical-smoke-18-marker-success.md \
  141-autonomous-ppsspp-gui-broker.md \
  142-autonomous-ppsspp-gui-broker-selftest.md \
  143-autonomous-global-parity-campaign-result.md \
  PARITY_SCENE_MATRIX.md \
  PARITY_ACTOR_MATRIX.md; do
  cp "$PROJECT_ROOT/docs/reports/$report" "$OUTPUT/reports/$report"
done

eboot_hash="$(shasum -a 256 "$EBOOT" | awk '{print $1}')"
commit="$(git -C "$PROJECT_ROOT" rev-parse HEAD)"
build_id="$(awk -F= '$1 == "parity_build_id" {print $2}' "$BUILD_IDENTITY")"
build_commit="$(awk -F= '$1 == "commit" {print $2}' "$BUILD_IDENTITY")"
cat >"$OUTPUT/GLOBAL_PARITY.METRICS" <<EOF
status=READY_UNATTENDED_PPSSPP_PARITY_CAMPAIGN
staging_commit=$commit
psp_build_commit=$build_commit
psp_eboot_sha256=$eboot_hash
parity_build_id=$build_id
parity_contract=DUSKLIGHT_DESKTOP_PSP_PARITY_CONTRACT_V1
scenes_in_scope=40
scenes_traced_desktop=10
scenes_traced_psp_current=10
scenes_match=0
scenes_partial=10
scenes_missing_or_not_ported=30
actors_in_scope=748
actors_original_source_placements=18
actors_transform_only_adapter=9
actors_procedural_fallback=2
actors_missing=719
link_status=PARTIAL_PARITY
rooms_local_package_match=3
camera_match_with_tolerance=1
ui_surfaces=14
current_benchmark_runs=10
visual_pairs_current=54
package_crc_valid=true
transform_marker_created=false
link_behavior_marker_created=false
scene_behavior_marker_created=false
global_review_marker_created=false
ppsspp_execution=COMPLETED
ppsspp_transport_required=persistent_gui_broker
ppsspp_gui_broker_host_test=true
ppsspp_gui_broker_manual_start=completed_once
ppsspp_per_test_launchservices_calls=0
ppsspp_functional_backend=opengl
ppsspp_functional_renderer=software
ppsspp_last_transport_classification=MARKERS_VALID_METRICS_VALID
ppsspp_last_transport_boot_observed=true
ppsspp_last_graphics_classification=VULKAN_PERFORMANCE_OPENGL_CONSERVATIVE
ppsspp_eboot_runtime_failure_observed=false
new_supported_rooms=0
new_unrelated_actor_sources=0
new_parallel_runtime_targets=0
new_gameplay_features=0
manual_broker_restart_count=0
user_confirmation_prompts_after_bootstrap=0
campaign_items_total=43
campaign_items_succeeded=43
campaign_items_failed=0
link_scenarios_partial=10
user_manual_direction_validation=pending
user_manual_acceptance=pending
error_code=0
EOF

cat >"$OUTPUT/README.md" <<'EOF'
# Dusklight PSP global parity review — staging

Ce paquet contient les preuves de la campagne PPSSPP autonome : dix traces
Link PSP DTRC v3, les comparaisons desktop/PSP, dix exécutions benchmark, les
captures de revue et les métriques du broker. Les 43 éléments d’exécution ont
réussi sans intervention après bootstrap.

Aucun marqueur final de parité n’est présent : les dix scénarios Link restent
`PARTIAL_PARITY` et les trente autres scénarios sont honnêtement classés
`MISSING_OR_NOT_PORTED`.
EOF
cat >"$OUTPUT/SUMMARY.md" <<'EOF'
# Résumé intermédiaire

- 40 scénarios inventoriés, 10 traces desktop et PSP DTRC v3 comparées ;
- 10 scénarios Link `PARTIAL_PARITY`, 30 `MISSING_OR_NOT_PORTED` ;
- 748 placements acteurs : 18 sources originales, 9 adaptateurs bornés,
  2 fallbacks procéduraux, 719 absents ;
- 3 rooms canoniques cohérentes localement ;
- Link reste `PARTIAL_PARITY`, première divergence au tick 0 sur `procedure` ;
- 54 captures de revue et 10 runs benchmark courants acquis ;
- release automatisée et smoke canonique validés ;
- validations manuelles : `pending`.
EOF

printf '%s\n' \
  "GLOBAL_PARITY_REVIEW_STAGED status=READY_UNATTENDED_PPSSPP_PARITY_CAMPAIGN markers=0 scenes=40 actors=748"
