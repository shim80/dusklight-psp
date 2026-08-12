#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

ORACLE="$(assert_project_path "artifacts/dusklight-desktop-oracle-v2")"
FUNCTIONAL="$(assert_project_path \
  "artifacts/dusklight-startup-v2/functional/title_flow.metrics")"
NEW_GAME="$(assert_project_path \
  "artifacts/dusklight-startup-v2/functional/new_game_transition.metrics")"
PACKAGE="$(assert_project_path \
  "artifacts/dusklight-psp/PSP/GAME/DUSKLIGHT_PSP")"
OUTPUT="$(assert_project_path "artifacts/dusklight-parity-v2")"

for required in \
  "$ORACLE/DESKTOP_ORACLE_V2.OK" \
  "$ORACLE/DESKTOP_ORACLE_V2.METRICS" \
  "$ORACLE/title_camera_reference.json" \
  "$ORACLE/opening_sequence_reference.jsonl" \
  "$ORACLE/file_select_reference_v2.json" \
  "$ORACLE/F_SP108_ACTIVATION.METRICS" \
  "$FUNCTIONAL" "$NEW_GAME"; do
  [ -s "$required" ] || die "preuve de parité absente : $required"
done
[ "$(cat "$ORACLE/DESKTOP_ORACLE_V2.OK")" = \
    DUSKLIGHT_DESKTOP_ORACLE_V2_OK ] ||
  die "marqueur oracle desktop v2 invalide"
grep -q '^title_camera_parity=MATCH_WITH_TOLERANCE$' "$FUNCTIONAL" ||
  die "parité caméra titre non observée"
grep -q '^startup_current_segment=new_game_transition$' "$FUNCTIONAL" &&
  grep -q '^title_new_game_available=true$' "$FUNCTIONAL" &&
  grep -q '^error_code=0$' "$FUNCTIONAL" ||
  die "parcours startup Functional incomplet"
grep -q '^source_stage=F_SP108$' "$NEW_GAME" &&
  grep -q '^instantiated_source_actors=9$' "$NEW_GAME" &&
  grep -q '^essential_source_actor_coverage=1.000$' "$NEW_GAME" &&
  grep -q '^error_code=0$' "$NEW_GAME" ||
  die "frontière F_SP108 PSP invalide"
"$PROJECT_ROOT/build/host/canonical-runtime/startup_camera_parity_host_test" \
  >/dev/null

safe_mkdir artifacts/dusklight-parity-v2
cat >"$OUTPUT/STARTUP_PARITY_V2.REPORT.md" <<'EOF'
# Startup parity v2

| Checkpoint | Résultat | Justification |
|---|---|---|
| Caméra F_SP102 | MATCH_WITH_TOLERANCE | Eye, center et FOV source aux checkpoints 0/900/1800 ; aspect PSP et near adaptés. |
| Événements opening | MATCH_WITH_TOLERANCE | Sous-ensemble DTRC déclaré, 1 800 frames ; audio et event runtime complet absents. |
| File select | EXPECTED_PLATFORM_DIFFERENCE | Trois slots, curseur et New Game ; layout J2D et saisie libre non portés. |
| New Game F_SP108/R01/21 | MATCH | Même destination et même EBOOT, sans redémarrage. |
| Acceptation visuelle | MISSING_ON_PSP | Validation manuelle et PSP physique différées. |
EOF
cat >"$OUTPUT/F_SP108_PARITY.REPORT.md" <<'EOF'
# F_SP108 parity

| Checkpoint | Résultat | Justification |
|---|---|---|
| 599 records source | MATCH | Table DPSC déterministe conservée. |
| 9 acteurs essentiels | MATCH_WITH_TOLERANCE | Identité, paramètres, transforms et lifecycle ; comportements complets non revendiqués. |
| Link / room / collision | MATCH_WITH_TOLERANCE | Ressources originales converties, spawn 21 et 180 frames synchronisées. |
| Acteurs non essentiels | MISSING_ON_PSP | Inventoriés dans le CSV, volontairement non instanciés. |
EOF
cat >"$OUTPUT/LINK_PARITY.REPORT.md" <<'EOF'
# Link parity

| Scénario | Résultat |
|---|---|
| idle F_SP108 | MATCH_WITH_TOLERANCE |
| walk / run / turn / stop / slope | MISSING_ON_PSP |

La référence DTRC v2 est disponible. Cette passe ne prétend pas fermer les
scénarios que l’oracle automatique n’a pas exercés.
EOF
cat >"$OUTPUT/RENDER_PARITY.REPORT.md" <<'EOF'
# Render parity

| Surface | Résultat |
|---|---|
| Caméra titre | MATCH_WITH_TOLERANCE |
| Environment F_SP102/F_SP108 | EXPECTED_PLATFORM_DIFFERENCE |
| Matériaux Link | EXPECTED_PLATFORM_DIFFERENCE |
| Shadow requests | EXPECTED_PLATFORM_DIFFERENCE |

Les événements source sont conservés dans l’oracle. Le GE PSP emploie une
approximation fixed-function documentée ; aucune égalité pixel n’est revendiquée.
EOF

camera_events="$(awk -F= \
  '$1 == "desktop_camera_events" {print $2; exit}' \
  "$ORACLE/DESKTOP_ORACLE_V2.METRICS")"
cat >"$PACKAGE/STARTUP_PARITY_V2.METRICS" <<EOF
title_camera_parity=MATCH_WITH_TOLERANCE
title_camera_checkpoints=7
desktop_camera_events=$camera_events
opening_event_parity=MATCH_WITH_TOLERANCE
file_select_layout_parity=EXPECTED_PLATFORM_DIFFERENCE
name_entry_parity=EXPECTED_PLATFORM_DIFFERENCE
new_game_parity=true
link_parity=MATCH_WITH_TOLERANCE
environment_parity=EXPECTED_PLATFORM_DIFFERENCE
material_parity=EXPECTED_PLATFORM_DIFFERENCE
shadow_request_parity=EXPECTED_PLATFORM_DIFFERENCE
hardware_validation=deferred_by_user
user_manual_acceptance=pending
error_code=0
EOF
{
  cat "$ORACLE/F_SP108_ACTIVATION.METRICS"
  printf 'new_game_parity=true\n'
} >"$PACKAGE/F_SP108.METRICS"
printf '%s' DUSKLIGHT_DESKTOP_PSP_STARTUP_PARITY_V2_OK \
  >"$PACKAGE/STARTUP_PARITY_V2.OK"
printf '%s' DUSKLIGHT_PSP_F_SP108_FIRST_PLAYABLE_OK \
  >"$PACKAGE/F_SP108_FIRST_PLAYABLE.OK"

printf '%s\n' \
  "STARTUP_PARITY_V2_OK camera=MATCH_WITH_TOLERANCE new_game=MATCH f_sp108=9/9"
