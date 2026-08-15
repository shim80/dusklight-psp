#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

SOURCE="${DUSKLIGHT_DEMO38_STB:-${1:-}}"
[ -n "$SOURCE" ] ||
  die "DUSKLIGHT_DEMO38_STB (demo38_01.stb extrait légalement) est requis"
SOURCE="$(assert_project_path "$SOURCE")"
[ -f "$SOURCE" ] || die "source demo38_01.stb absente"
[ ! -L "$SOURCE" ] || die "lien symbolique source refusé"

EXPECTED_SOURCE_SHA256="e335d6d44c002dd25881aedd2f053a226be18cdd254d2049e0d78f2aa88b735d"
[ "$(sha256_file "$SOURCE")" = "$EXPECTED_SOURCE_SHA256" ] ||
  die "empreinte demo38_01.stb inattendue (GZ2P01 revision 0 requis)"

OUTPUT="$(assert_project_path build/assets/demo38-camera)"
PASS1="$(assert_project_path .tmp/demo38-camera-pass1)"
PASS2="$(assert_project_path .tmp/demo38-camera-pass2)"
safe_mkdir "$OUTPUT"
safe_mkdir "$PASS1"
safe_mkdir "$PASS2"

python3 "$PROJECT_ROOT/tools/demo38_camera_export.py" \
  "$SOURCE" "$PASS1/title_camera.dpcm"
python3 "$PROJECT_ROOT/tools/demo38_camera_export.py" \
  "$SOURCE" "$PASS2/title_camera.dpcm"
cmp "$PASS1/title_camera.dpcm" "$PASS2/title_camera.dpcm"
cp -- "$PASS1/title_camera.dpcm" "$OUTPUT/title_camera.dpcm"

VALIDATOR_BUILD="$(assert_project_path build/host/demo38-camera-validator)"
safe_mkdir "$VALIDATOR_BUILD"
cmake -S "$PROJECT_ROOT/test/startup-camera-host" \
  -B "$VALIDATOR_BUILD" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$VALIDATOR_BUILD"
"$VALIDATOR_BUILD/startup_camera_host_test" \
  "$OUTPUT/title_camera.dpcm"

{
  printf '%s\n' \
    "format=DUSKLIGHT_DEMO38_CAMERA_V1" \
    "disc_id=GZ2P01" \
    "disc_revision=0" \
    "source_archive=/res/Object/Demo38_01.arc" \
    "source_resource=evt/demo38_01.stb" \
    "source_sha256=$EXPECTED_SOURCE_SHA256" \
    "source_fps=30" \
    "source_frames=2400" \
    "sample_count=2401" \
    "desktop_trace_offset_ticks=2" \
    "deterministic=true" \
    "network_used=false" \
    "title_camera.dpcm_size=$(wc -c <"$OUTPUT/title_camera.dpcm" | tr -d ' ')" \
    "title_camera.dpcm_sha256=$(sha256_file "$OUTPUT/title_camera.dpcm")"
} >"$OUTPUT/DEMO38_CAMERA.MANIFEST"

printf '%s\n' \
  "DEMO38_CAMERA_ASSETS_OK deterministic=true source_frames=2400 samples=2401"
