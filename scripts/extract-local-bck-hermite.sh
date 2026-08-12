#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

[ -n "${DUSKLIGHT_GAME_IMAGE:-}" ] ||
  die "DUSKLIGHT_GAME_IMAGE est requis"
image="$(assert_project_path "$DUSKLIGHT_GAME_IMAGE")"
[ -f "$image" ] || die "image locale absente"
[ ! -L "$image" ] || die "lien symbolique image refusé"

raw="$(assert_project_path ".test-data/bck-hermite/source-curves.json")"
summary="$(assert_project_path "build/reports/parity/bck-hermite-summary.json")"
safe_mkdir .test-data/bck-hermite
safe_mkdir build/reports/parity
safe_mkdir logs

"$SCRIPT_DIR/build-link-loader-probe.sh"
env DUSKLIGHT_GAME_IMAGE="$image" \
  DUSKLIGHT_BCK_CURVE_OUTPUT="$raw" \
  "$PROJECT_ROOT/build/host/link-loader/probe/dusk_link_loader_probe" \
  >"$PROJECT_ROOT/logs/bck-hermite-extraction.log" 2>&1

python3 -B - "$raw" "$summary" "$image" <<'PY'
import hashlib
import json
import pathlib
import sys

raw = pathlib.Path(sys.argv[1])
summary = pathlib.Path(sys.argv[2])
image = pathlib.Path(sys.argv[3])
data = json.loads(raw.read_text())
if data.get("schema") != "dusklight.bck.ank1.curves.v1":
    raise SystemExit("schéma BCK inattendu")
expected = ["waits.bck", "walks.bck", "dashs.bck", "stepl.bck"]
if [clip.get("name") for clip in data.get("clips", [])] != expected:
    raise SystemExit("identité des clips BCK inattendue")

clips = []
for clip in data["clips"]:
    tracks = clip["tracks"]
    hermite = [
        track for track in tracks
        if track["curve"]["interpolation"] == "hermite"
    ]
    split = [
        track for track in hermite
        if track["curve"]["tangent_type"] == 1
    ]
    clips.append({
        "name": clip["name"],
        "resource_id": clip["resource_id"],
        "duration": clip["duration"],
        "loop_mode": clip["loop_mode"],
        "rotation_decimal_shift": clip["rotation_decimal_shift"],
        "joint_count": clip["joint_count"],
        "track_count": len(tracks),
        "key_count": sum(track["curve"]["key_count"] for track in tracks),
        "hermite_track_count": len(hermite),
        "split_tangent_track_count": len(split),
    })

image_hash = hashlib.sha256()
with image.open("rb") as stream:
    while chunk := stream.read(4 * 1024 * 1024):
        image_hash.update(chunk)
result = {
    "schema": "dusklight.bck.hermite.summary.v1",
    "source_identity": {
        "disc_id": "GZ2P01",
        "disc_revision": 0,
        "image_size": image.stat().st_size,
        "image_sha256": image_hash.hexdigest(),
    },
    "curve_dump_sha256": hashlib.sha256(raw.read_bytes()).hexdigest(),
    "clip_count": len(clips),
    "track_count": sum(clip["track_count"] for clip in clips),
    "hermite_track_count": sum(
        clip["hermite_track_count"] for clip in clips
    ),
    "clips": clips,
}
summary.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
PY

python3 -B -m json.tool "$raw" >/dev/null
python3 -B -m json.tool "$summary" >/dev/null
printf 'LOCAL_BCK_HERMITE_EXTRACTION_OK raw=%s summary=%s\n' \
  "${raw#"$PROJECT_ROOT/"}" "${summary#"$PROJECT_ROOT/"}"
