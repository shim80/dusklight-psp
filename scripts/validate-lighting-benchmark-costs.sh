#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

[ "$#" -gt 0 ] ||
  die "usage: validate-lighting-benchmark-costs.sh BENCHMARK.METRICS [...]"

field() {
  local file="$1"
  local key="$2"
  awk -F= -v key="$key" '$1 == key {print substr($0, length(key) + 2)}' \
    "$file"
}

for candidate in "$@"; do
  metrics="$(assert_project_path "$candidate")"
  [ -f "$metrics" ] || die "métriques lighting absentes : $candidate"
  [ "$(field "$metrics" benchmark_contract)" = \
      DUSKLIGHT_PSP_RENDERING_CONTRACT_V1 ] ||
    die "contrat benchmark lighting invalide : $candidate"
  [ "$(field "$metrics" benchmark_version)" = 1 ] ||
    die "version benchmark lighting invalide : $candidate"
  mode="$(field "$metrics" mode)"
  case "$mode" in
    benchmark_boot_intro|benchmark_title|benchmark_link_idle|benchmark_room_stress|benchmark_transition) ;;
    *) die "mode benchmark lighting invalide : $candidate" ;;
  esac
  profile="$(field "$metrics" profile)"
  case "$profile" in
    functional|performance|psp_conservative) ;;
    *) die "profil benchmark lighting invalide : $candidate" ;;
  esac
  frames="$(field "$metrics" measured_frames)"
  average="$(field "$metrics" lighting_cost_us_average)"
  p95="$(field "$metrics" lighting_cost_us_p95)"
  commit="$(field "$metrics" build_commit)"
  for number in "$frames" "$average" "$p95"; do
    case "$number" in
      ''|*[!0-9]*)
        die "coûts benchmark lighting non numériques : $candidate" ;;
    esac
  done
  case "$commit" in
    ''|*[!0-9a-f]*)
      die "commit benchmark lighting invalide : $candidate" ;;
  esac
  [ "${#commit}" -eq 40 ] ||
    die "longueur commit benchmark lighting invalide : $candidate"
  [ "$frames" -gt 0 ] || die "benchmark lighting vide : $candidate"
  [ "$p95" -ge "$average" ] ||
    die "p95 lighting inférieur à la moyenne : $candidate"
  if [ "$mode" = benchmark_link_idle ] ||
     [ "$mode" = benchmark_room_stress ]; then
    [ "$average" -gt 0 ] ||
      die "coût lighting nul dans une scène éclairée : $candidate"
  fi
  [ "$(field "$metrics" validation_target)" = PPSSPP ] ||
    die "cible benchmark lighting inattendue : $candidate"
  [ "$(field "$metrics" real_psp_validation_required)" = true ] ||
    die "limite matériel réel absente : $candidate"
  [ "$(field "$metrics" diagnostic_only)" = true ] ||
    die "métrique benchmark non diagnostique : $candidate"
  [ "$(field "$metrics" error_code)" = 0 ] ||
    die "benchmark lighting en erreur : $candidate"
  printf 'LIGHTING_BENCHMARK_COST_METRICS_OK mode=%s profile=%s frames=%s average_us=%s p95_us=%s build_commit=%s hardware=PPSSPP_ONLY\n' \
    "$mode" "$profile" "$frames" "$average" "$p95" "$commit"
done
