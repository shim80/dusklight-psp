#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

[ "$#" -eq 2 ] ||
  die "usage : import-link-hardware-benchmark.sh DUSKLIGHT_LINK_DEMO metadata.txt"
INPUT="$(CDPATH= cd -- "$1" 2>/dev/null && pwd -P)" ||
  die "répertoire de résultats absent"
METADATA_DIR="$(CDPATH= cd -- "$(dirname -- "$2")" 2>/dev/null && pwd -P)" ||
  die "répertoire de métadonnées absent"
METADATA="$METADATA_DIR/$(basename -- "$2")"
[ -f "$METADATA" ] && [ ! -L "$METADATA" ] ||
  die "fichier de métadonnées absent ou lié"

EBOOT="$INPUT/EBOOT.PBP"
PACKAGE="$INPUT/data/link.dpmd"
MODE="$INPUT/LINK_DEMO.MODE"
CONFIG="$INPUT/LINK_BENCH.CONFIG"
MARKER="$INPUT/LINK_BENCH.OK"
METRICS="$INPUT/LINK_BENCH.METRICS"
MANIFEST="$(assert_project_path "artifacts/psp-link-benchmark/MANIFEST.sha256")"
PACKAGE_ROOT="$(assert_project_path "artifacts/psp-link-benchmark")"

for path in "$EBOOT" "$PACKAGE" "$MODE" "$CONFIG" "$MARKER" "$METRICS"; do
  [ -f "$path" ] && [ ! -L "$path" ] ||
    die "résultat requis absent ou lié : ${path##*/}"
done
[ -f "$MANIFEST" ] && [ ! -L "$MANIFEST" ] ||
  die "manifeste local du paquet absent"
[ "$(wc -l <"$MANIFEST" | tr -d ' ')" = 5 ] ||
  die "le manifeste ne couvre pas exactement cinq fichiers"

manifest_hash() {
  awk -v path="$1" '$2 == path {print $1; exit}' "$MANIFEST"
}

verify_hash() {
  local file="$1" manifest_path="$2" expected actual
  expected="$(manifest_hash "$manifest_path")"
  [ -n "$expected" ] || die "entrée de manifeste absente : $manifest_path"
  actual="$(shasum -a 256 "$file" | awk '{print $1}')"
  [ "$actual" = "$expected" ] ||
    die "hash divergent : $manifest_path"
}

verify_hash "$EBOOT" PSP/GAME/DUSKLIGHT_LINK_DEMO/EBOOT.PBP
verify_hash "$PACKAGE" PSP/GAME/DUSKLIGHT_LINK_DEMO/data/link.dpmd
verify_hash "$MODE" PSP/GAME/DUSKLIGHT_LINK_DEMO/LINK_DEMO.MODE
verify_hash "$CONFIG" PSP/GAME/DUSKLIGHT_LINK_DEMO/LINK_BENCH.CONFIG
verify_hash "$PACKAGE_ROOT/README.txt" README.txt

[ "$(cat "$MODE")" = benchmark ] ||
  die "LINK_DEMO.MODE n'est pas benchmark"
[ "$(cat "$MARKER")" = DUSKLIGHT_PSP_LINK_BENCH_OK ] &&
  [ "$(wc -c <"$MARKER" | tr -d ' ')" = 27 ] ||
  die "LINK_BENCH.OK invalide"

metric() {
  awk -F= -v key="$1" \
    '$1 == key {print substr($0, length(key) + 2); exit}' "$METRICS"
}

scenario_metric() {
  local scenario="$1" key="$2"
  awk -F= -v scenario="$scenario" -v key="$key" '
    $1 == "scenario_begin" {inside = ($2 == scenario)}
    inside && $1 == key {print substr($0, length(key) + 2); exit}
    $1 == "scenario_end" && $2 == scenario {inside = 0}
  ' "$METRICS"
}

config_value() {
  awk -F= -v key="$1" \
    '$1 == key {print substr($0, length(key) + 2); exit}' "$CONFIG"
}

metadata_value() {
  awk -F= -v key="$1" \
    '$1 == key {print substr($0, length(key) + 2); exit}' "$METADATA"
}

safe_value() {
  [[ "$1" =~ ^[A-Za-z0-9._+() -]{1,80}$ ]]
}

[ "$(config_value target)" = hardware ] ||
  die "le paquet n'est pas déclaré hardware"
RUN_LABEL="$(config_value run_label)"
[[ "$RUN_LABEL" =~ ^[A-Za-z0-9._-]{1,63}$ ]] ||
  die "run_label invalide"
[ "$(metric target_declared)" = hardware ] &&
  [ "$(metric run_label)" = "$RUN_LABEL" ] &&
  [ "$(metric mode)" = benchmark ] ||
  die "identité des métriques incohérente"
[ "$(metric triangle_count)" = 4329 ] &&
  [ "$(metric vertex_count)" = 3543 ] &&
  [ "$(metric index_count)" = 12987 ] &&
  [ "$(metric chunk_count)" = 5 ] &&
  [ "$(metric draw_call_count)" = 5 ] &&
  [ "$(metric warmup_frames)" = 120 ] &&
  [ "$(metric measured_frames)" = 600 ] &&
  [ "$(metric allocation_count_during_frame)" = 0 ] &&
  [ "$(metric guard_regions_valid)" = true ] &&
  [ "$(metric pixel_checks_before_valid)" = true ] &&
  [ "$(metric pixel_checks_after_valid)" = true ] &&
  [ "$(metric synchronization)" = complete ] &&
  [ "$(metric hardware_validation)" = false ] &&
  [ "$(metric error_code)" = 0 ] ||
  die "métriques fonctionnelles invalides"
[ "$(metric package_crc_expected)" = \
  "$(metric package_crc_actual)" ] ||
  die "CRC DPMD divergent"
[ "$(metric dpmd_sha256_expected)" = \
  b15eb6a5a8e077a888462eb7574282ce8cf730ff5baea7a747863edc76e18fd8 ] ||
  die "SHA-256 DPMD attendu divergent"
for scenario in culling_off culling_on; do
  [ "$(scenario_metric "$scenario" scenario_name)" = "$scenario" ] &&
    [ "$(scenario_metric "$scenario" sample_count)" = 600 ] ||
    die "scénario incomplet : $scenario"
done
[ "$(scenario_metric culling_off culling_enabled)" = false ] &&
  [ "$(scenario_metric culling_on culling_enabled)" = true ] ||
  die "états de culling inversés"

[ "$(metadata_value physical_psp_confirmed)" = yes ] ||
  die "l'attestation physical_psp_confirmed=yes est requise"
for field in \
  psp_model firmware custom_firmware cfw_cpu_setting \
  memory_stick power_source reported_cpu_clock_mhz \
  reported_bus_clock_mhz visual_stability visible_pieces \
  flicker corruption crash_or_hang \
  interactive_controls_after_benchmark; do
  value="$(metadata_value "$field")"
  safe_value "$value" || die "métadonnée absente ou non sûre : $field"
done

IMPORT_ROOT="$(assert_project_path ".test-data/link-benchmark-imports")"
safe_mkdir .test-data/link-benchmark-imports
RUN_DIR="$IMPORT_ROOT/${RUN_LABEL}-$(timestamp_utc)"
case "$RUN_DIR/" in
  "$PROJECT_ROOT/"*) ;;
  *) die "destination d'import hors dépôt" ;;
esac
mkdir -p -- "$RUN_DIR/raw"
cp -- "$MARKER" "$RUN_DIR/raw/LINK_BENCH.OK"
cp -- "$METRICS" "$RUN_DIR/raw/LINK_BENCH.METRICS"
cp -- "$CONFIG" "$RUN_DIR/raw/LINK_BENCH.CONFIG"
cp -- "$METADATA" "$RUN_DIR/raw/hardware-metadata.txt"
cp -- "$MANIFEST" "$RUN_DIR/raw/MANIFEST.sha256"

EBOOT_HASH="$(shasum -a 256 "$EBOOT" | awk '{print $1}')"
DPMD_HASH="$(shasum -a 256 "$PACKAGE" | awk '{print $1}')"
OFF_P95="$(scenario_metric culling_off frame_total_us_p95)"
ON_P95="$(scenario_metric culling_on frame_total_us_p95)"
CLASSIFICATION=LINK_HARDWARE_BASELINE_VALIDATED
if [ "$(metadata_value crash_or_hang)" != none ]; then
  CLASSIFICATION=LINK_HARDWARE_CRASH_OR_HANG
elif [ "$(metadata_value corruption)" != none ] ||
     [ "$(metadata_value flicker)" != none ] ||
     [ "$(metadata_value visible_pieces)" != all_five ]; then
  CLASSIFICATION=LINK_HARDWARE_RENDER_DIVERGENCE
elif [ "$OFF_P95" -gt 33333 ] || [ "$ON_P95" -gt 33333 ]; then
  CLASSIFICATION=LINK_HARDWARE_FUNCTIONAL_BUT_SLOW
fi

{
  printf 'hardware_validation=confirmed\n'
  printf 'classification=%s\n' "$CLASSIFICATION"
  printf 'run_label=%s\n' "$RUN_LABEL"
  printf 'psp_model=%s\n' "$(metadata_value psp_model)"
  printf 'firmware=%s\n' "$(metadata_value firmware)"
  printf 'custom_firmware=%s\n' "$(metadata_value custom_firmware)"
  printf 'cfw_cpu_setting=%s\n' "$(metadata_value cfw_cpu_setting)"
  printf 'reported_cpu_clock_mhz=%s\n' \
    "$(metadata_value reported_cpu_clock_mhz)"
  printf 'reported_bus_clock_mhz=%s\n' \
    "$(metadata_value reported_bus_clock_mhz)"
  printf 'eboot_sha256=%s\n' "$EBOOT_HASH"
  printf 'dpmd_sha256=%s\n' "$DPMD_HASH"
  printf 'culling_off_frame_total_us_p95=%s\n' "$OFF_P95"
  printf 'culling_on_frame_total_us_p95=%s\n' "$ON_P95"
  printf 'visual_stability=%s\n' "$(metadata_value visual_stability)"
  printf 'visible_pieces=%s\n' "$(metadata_value visible_pieces)"
  printf 'flicker=%s\n' "$(metadata_value flicker)"
  printf 'corruption=%s\n' "$(metadata_value corruption)"
  printf 'crash_or_hang=%s\n' "$(metadata_value crash_or_hang)"
} >"$RUN_DIR/summary.txt"

printf 'LINK_HARDWARE_IMPORT_OK classification=%s summary=%s\n' \
  "$CLASSIFICATION" \
  "${RUN_DIR#"$PROJECT_ROOT"/}/summary.txt"
