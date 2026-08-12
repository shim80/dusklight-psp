#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

TRACE="$(assert_project_path ".tools/reference/dusklight-desktop/source-trace")"
AURORA="$TRACE/extern/aurora"
DUSKLIGHT_V2_PATCH="$(assert_project_path "reference/desktop/patches/0004-dusklight-reference-trace-v2.patch")"
DUSKLIGHT_V3_PATCH="$(assert_project_path "reference/desktop/patches/0005-dusklight-reference-parity-trace-v3.patch")"
DUSKLIGHT_BEHAVIOR_PATCH="$(assert_project_path "reference/desktop/patches/0007-dusklight-reference-link-behavior-events.patch")"
DUSKLIGHT_POSE_PATCH="$(assert_project_path "reference/desktop/patches/0008-dusklight-reference-pose-checkpoints.patch")"
DUSKLIGHT_RENDER_PATCH="$(assert_project_path "reference/desktop/patches/0009-dusklight-reference-render-state-trace.patch")"
AURORA_PATCH="$(assert_project_path "reference/desktop/patches/0002-aurora-reference-input.patch")"
AURORA_ANALOG_PATCH="$(assert_project_path "reference/desktop/patches/0006-aurora-reference-analog-input.patch")"
EXPECTED_DUSKLIGHT=1bae8a5e6a812217ca33ba533e707ecfa64b1553
EXPECTED_AURORA=81f12f31d23ec822d8bde2031c91e94c470911eb

[ "$(git -C "$TRACE" rev-parse HEAD)" = "$EXPECTED_DUSKLIGHT" ] ||
  die "révision Dusklight trace inattendue"
[ "$(git -C "$AURORA" rev-parse HEAD)" = "$EXPECTED_AURORA" ] ||
  die "révision Aurora trace inattendue"

apply_one() {
  local source="$1"
  local patch="$2"
  local label="$3"
  if git -C "$source" apply --reverse --check "$patch" 2>/dev/null; then
    printf '%s\n' "$label déjà appliqué"
    return
  fi
  git -C "$source" apply --check "$patch"
  git -C "$source" apply "$patch"
  git -C "$source" apply --reverse --check "$patch" ||
    die "instrumentation $label non vérifiable"
  printf '%s\n' "$label appliqué"
}

if git -C "$TRACE" apply --reverse --check "$DUSKLIGHT_BEHAVIOR_PATCH" 2>/dev/null; then
  printf 'Dusklight-DTRC-v2/v3/comportement déjà appliqués\n'
else
  if ! git -C "$TRACE" apply --reverse --check "$DUSKLIGHT_V2_PATCH" 2>/dev/null; then
    [ -z "$(git -C "$TRACE" status --porcelain --untracked-files=all)" ] ||
      die "source Dusklight sale avant instrumentation"
    apply_one "$TRACE" "$DUSKLIGHT_V2_PATCH" Dusklight-DTRC-v2
  fi
  if ! git -C "$TRACE" apply --reverse --check "$DUSKLIGHT_V3_PATCH" 2>/dev/null; then
    apply_one "$TRACE" "$DUSKLIGHT_V3_PATCH" Dusklight-DTRC-v3
  fi
  apply_one "$TRACE" "$DUSKLIGHT_BEHAVIOR_PATCH" Dusklight-comportement
fi
apply_one "$TRACE" "$DUSKLIGHT_POSE_PATCH" Dusklight-pose-checkpoints
apply_one "$TRACE" "$DUSKLIGHT_RENDER_PATCH" Dusklight-render-state

if git -C "$AURORA" apply --reverse --check "$AURORA_ANALOG_PATCH" 2>/dev/null; then
  printf 'Aurora entrée numérique/analogique déjà appliquée\n'
else
  if ! git -C "$AURORA" apply --reverse --check "$AURORA_PATCH" 2>/dev/null; then
    [ -z "$(git -C "$AURORA" status --porcelain --untracked-files=all)" ] ||
      die "source Aurora sale avant instrumentation"
    apply_one "$AURORA" "$AURORA_PATCH" Aurora
  fi
  apply_one "$AURORA" "$AURORA_ANALOG_PATCH" Aurora-analogique
fi

printf 'DUSKLIGHT_DESKTOP_REFERENCE_TRACE_READY\n'
