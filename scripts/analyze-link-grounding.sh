#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
EXPECTED_ROOT="$(git -C "$ROOT" rev-parse --show-toplevel)"
[ "$ROOT" = "$EXPECTED_ROOT" ] || {
  echo "racine Git inattendue" >&2
  exit 1
}

REPORTS="$ROOT/build/reports"
HOST_BUILD="$ROOT/build/host/link-playable"
ASSETS="$ROOT/build/assets/dusklight-psp/data/common"
mkdir -p "$REPORTS"

cmake -S "$ROOT/test/link-playable" -B "$HOST_BUILD" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$HOST_BUILD" \
  --target link_grounding_reference_host_test \
  link_grounding_parity_host_test playable_runtime_host_test

"$HOST_BUILD/link_grounding_reference_host_test" \
  "$ASSETS/link.dpsk" \
  "$ASSETS/link.dptx" \
  "$ASSETS/link.dpan" \
  "$ASSETS/hud.dpui" \
  "$REPORTS/link-foot-contact-reference.csv"

"$HOST_BUILD/playable_runtime_host_test" \
  "$ASSETS/link.dpsk" \
  "$ASSETS/link.dptx" \
  "$ASSETS/link.dpan" \
  "$ASSETS/hud.dpui"

"$HOST_BUILD/link_grounding_parity_host_test" \
  "$ASSETS/link.dpsk" \
  "$ASSETS/link.dptx" \
  "$ASSETS/link.dpan" \
  "$ASSETS/hud.dpui"

awk -F, '
  NR > 1 {
    key = $1 "/" $3
    if (!(key in seen)) {
      seen[key] = 1
      left_min[key] = $8
      right_min[key] = $9
      root_min[key] = $5
      root_max[key] = $5
    }
    if ($8 < left_min[key]) left_min[key] = $8
    if ($9 < right_min[key]) right_min[key] = $9
    if ($5 < root_min[key]) root_min[key] = $5
    if ($5 > root_max[key]) root_max[key] = $5
    if ($13 > left_pen[key]) left_pen[key] = $13
    if ($14 > right_pen[key]) right_pen[key] = $14
  }
  END {
    for (key in seen) {
      printf "%s left_sole_min=%.6f right_sole_min=%.6f ", \
        key, left_min[key], right_min[key]
      printf "left_penetration_max=%.6f right_penetration_max=%.6f ", \
        left_pen[key], right_pen[key]
      printf "root_y=%.6f:%.6f\n", root_min[key], root_max[key]
    }
  }
' "$REPORTS/link-foot-contact-reference.csv" |
  sort >"$REPORTS/link-collision-comparison.txt"

{
  rg -n -A 95 \
    'void daAlink_c::transAnimeProc' \
    "$ROOT/dusklight-main/src/d/actor/d_a_alink.cpp"
  rg -n -A 75 \
    'int daAlink_c::setFootMatrix' \
    "$ROOT/dusklight-main/src/d/actor/d_a_alink.cpp"
  rg -n -A 135 \
    'void daAlink_c::footBgCheck' \
    "$ROOT/dusklight-main/src/d/actor/d_a_alink.cpp"
} >"$REPORTS/link-source-functions.txt"

{
  echo "LINK_ROOT_ANALYSIS_V1"
  echo "joint_root=0:center"
  echo "joint_pelvis=16:waist"
  echo "joint_left_chain=18:legL1,19:legL2,20:footL,21:toeL"
  echo "joint_right_chain=23:legR1,24:legR2,25:footR,26:toeR"
  echo "source_boot_material=6:al_boots_m"
  echo "dpsk_boot_submesh=15"
  echo "source_root_translation_path=transAnimeProc+jointControll"
  echo "source_grounding_path=footBgCheck+setLegAngle+setFootMatrix"
  echo "psp_current_missing=footBgCheck,setLegAngle,setFootMatrix"
  echo "psp_current_root_xz=cleared"
  echo "psp_current_root_y=retained"
} >"$REPORTS/link-root-analysis.txt"

if [ -n "${DUSKLIGHT_GAME_IMAGE:-}" ]; then
  [ -f "$DUSKLIGHT_GAME_IMAGE" ] || {
    echo "DUSKLIGHT_GAME_IMAGE absent" >&2
    exit 1
  }
  PROBE_BUILD="$ROOT/build/host/link-loader/probe"
  cmake --build "$PROBE_BUILD" --target dusk_link_loader_probe
  DUSKLIGHT_GAME_IMAGE="$DUSKLIGHT_GAME_IMAGE" \
    "$PROBE_BUILD/dusk_link_loader_probe" \
    >"$REPORTS/link-source-probe.log"
  rg \
    '^(LINK_JOINT_METRICS|LINK_BODY_SHAPE_METRICS|LINK_J3D_LOAD_OK)' \
    "$REPORTS/link-source-probe.log" \
    >"$REPORTS/link-source-identity.txt"
else
  echo "LINK_SOURCE_PROBE_SKIPPED reason=DUSKLIGHT_GAME_IMAGE_not_set" \
    >"$REPORTS/link-source-identity.txt"
fi

echo "DUSKLIGHT_LINK_GROUNDING_ANALYSIS_OK outputs=5"
