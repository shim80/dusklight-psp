#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"

[ "$#" -eq 1 ] || die "usage: $0 TRACE_JSONL"
TRACE="$(assert_project_path "$1")"
[ -f "$TRACE" ] || die "trace opaque absente"

python3 - "$TRACE" <<'PY'
import collections
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
frames = set()
counts = collections.Counter()
depth_writes = set()
submissions = 0
for line in path.read_text(encoding="utf-8").splitlines():
    event = json.loads(line)
    if event.get("event_type") != "render_submission":
        continue
    payload = event["payload"]
    submissions += 1
    frames.add(event["frame"])
    counts[(payload["source"], payload["bucket"])] += 1
    depth_writes.add(payload["depth_write"])
    forbidden = (
        payload["bucket"] != "opaque" or
        payload["alpha_test"] or payload["blending"] or
        payload["fog"] or
        payload["source"] in {"ui", "startup_ui"}
    )
    if forbidden:
        raise SystemExit(f"forbidden opaque submission: {payload}")

if len(frames) != 4 or submissions == 0:
    raise SystemExit("opaque trace coverage incomplete")
if counts[("room", "opaque")] == 0 or counts[("link", "opaque")] == 0:
    raise SystemExit("opaque room/link coverage incomplete")
if depth_writes != {False, True}:
    raise SystemExit("source depth-write policies were flattened")
print(
    "V3_OPAQUE_TRACE_OK "
    f"frames={len(frames)} submissions={submissions} "
    f"room={counts[('room', 'opaque')]} link={counts[('link', 'opaque')]} "
    "forbidden=0 depth_write_states=2")
PY
