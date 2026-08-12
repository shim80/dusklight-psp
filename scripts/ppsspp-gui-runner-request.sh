#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

ACTION=queue
REQUEST_ID=""
EBOOT=""
GAME_ID=""
MODE=smoke
PRESENTATION=game
BACKEND=opengl
RENDERER=software
TIMEOUT=120
CONFIG=""
declare -a MARKERS=()
declare -a PACKAGES=()

usage() {
  cat <<'EOF'
Usage:
  ppsspp-gui-runner-request.sh [--queue|--run]
    --request-id ID --eboot CHEMIN --game-id ID
    --config CHEMIN [--mode MODE] [--presentation game|debug|opaque_only]
    [--backend auto|opengl|vulkan] [--renderer software|hardware]
    [--timeout SECONDES]
    --marker NOM=CONTENU [--marker ...]
    [--package SOURCE=DESTINATION_RELATIVE_AU_DOSSIER_JEU ...]
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --queue) ACTION=queue ;;
    --run) ACTION=run ;;
    --request-id) shift; REQUEST_ID="${1:-}" ;;
    --eboot) shift; EBOOT="${1:-}" ;;
    --game-id) shift; GAME_ID="${1:-}" ;;
    --mode) shift; MODE="${1:-}" ;;
    --presentation) shift; PRESENTATION="${1:-}" ;;
    --backend) shift; BACKEND="${1:-}" ;;
    --renderer) shift; RENDERER="${1:-}" ;;
    --timeout) shift; TIMEOUT="${1:-}" ;;
    --config) shift; CONFIG="${1:-}" ;;
    --marker) shift; MARKERS+=("${1:-}") ;;
    --package) shift; PACKAGES+=("${1:-}") ;;
    -h|--help) usage; exit 0 ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done

[[ "$REQUEST_ID" =~ ^[A-Za-z0-9][A-Za-z0-9._-]{0,95}$ ]] ||
  die "request-id invalide"
[[ "$GAME_ID" =~ ^[A-Za-z0-9_][A-Za-z0-9_.-]{0,63}$ ]] ||
  die "game-id invalide"
[[ "$TIMEOUT" =~ ^[1-9][0-9]*$ ]] && [ "$TIMEOUT" -le 1800 ] ||
  die "timeout invalide"
[ -n "$EBOOT" ] && [ -n "$CONFIG" ] || die "EBOOT et config requis"
EBOOT="$(assert_project_path "$EBOOT")"
CONFIG="$(assert_project_path "$CONFIG")"
[ -f "$EBOOT" ] || die "EBOOT absent"
[ -f "$CONFIG" ] || die "config absente"

PPSSPP="$(assert_project_path \
  ".tools/ppsspp/PPSSPPSDL.app/Contents/MacOS/PPSSPPSDL")"
REQUEST_ROOT="$(assert_project_path \
  ".test-data/ppsspp/gui-runner/requests/$REQUEST_ID")"
SESSION_ROOT="$(assert_project_path \
  ".test-data/ppsspp/gui-runner/sessions/$REQUEST_ID")"
MEMSTICK="$(assert_project_path \
  ".test-data/ppsspp/gui-runner/sessions/$REQUEST_ID/home/.config/ppsspp/PSP")"
GAME_DIR="$MEMSTICK/GAME/$GAME_ID"
LOG_ROOT="$(assert_project_path "logs/ppsspp-gui-runner/$REQUEST_ID")"
REQUEST="$REQUEST_ROOT/request.json"
RESULT="$REQUEST_ROOT/response.json"
STDOUT="$LOG_ROOT/stdout.log"
STDERR="$LOG_ROOT/stderr.log"
BOOT_SIGNAL="$REQUEST_ROOT/BOOT.OBSERVED"

safe_mkdir ".test-data/ppsspp/gui-runner/requests/$REQUEST_ID"
safe_mkdir ".test-data/ppsspp/gui-runner/sessions/$REQUEST_ID/home/.config/ppsspp/PSP/GAME/$GAME_ID"
safe_mkdir ".test-data/ppsspp/gui-runner/sessions/$REQUEST_ID/xdg-cache"
safe_mkdir ".test-data/ppsspp/gui-runner/sessions/$REQUEST_ID/tmp"
safe_mkdir "logs/ppsspp-gui-runner/$REQUEST_ID"

MARKER_FILE="$REQUEST_ROOT/markers.list"
PACKAGE_FILE="$REQUEST_ROOT/packages.list"
: >"$MARKER_FILE"
: >"$PACKAGE_FILE"
if [ "${#MARKERS[@]}" -gt 0 ]; then
  printf '%s\n' "${MARKERS[@]}" >"$MARKER_FILE"
fi
if [ "${#PACKAGES[@]}" -gt 0 ]; then
  printf '%s\n' "${PACKAGES[@]}" >"$PACKAGE_FILE"
fi

/usr/bin/python3 - \
  "$PROJECT_ROOT" "$REQUEST_ID" "$PPSSPP" "$EBOOT" "$MEMSTICK" \
  "$GAME_DIR" "$MODE" "$PRESENTATION" "$BACKEND" "$RENDERER" "$TIMEOUT" \
  "$CONFIG" "$RESULT" "$STDOUT" "$STDERR" "$BOOT_SIGNAL" \
  "$SESSION_ROOT" "$MARKER_FILE" "$PACKAGE_FILE" "$REQUEST" <<'PY'
import hashlib
import json
import pathlib
import sys

(
    root, request_id, ppsspp, eboot, memstick, game_dir, mode,
    presentation, backend, renderer, timeout, config, result, stdout,
    stderr, boot, session_root, marker_file, package_file, output,
) = sys.argv[1:]

def file_hash(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()

def path_hash(path):
    source = pathlib.Path(path)
    if source.is_file():
        return file_hash(source)
    digest = hashlib.sha256()
    for item in sorted(
        (candidate for candidate in source.rglob("*") if candidate.is_file()),
        key=lambda candidate: candidate.relative_to(source).as_posix(),
    ):
        relative = item.relative_to(source).as_posix().encode()
        digest.update(len(relative).to_bytes(8, "big"))
        digest.update(relative)
        digest.update(bytes.fromhex(file_hash(item)))
    return digest.hexdigest()

markers = []
contents = {}
for line in pathlib.Path(marker_file).read_text().splitlines():
    if not line:
        continue
    name, separator, content = line.partition("=")
    if not separator or "/" in name or name in {".", ".."}:
        raise SystemExit("marker invalide")
    content = content.replace("\\n", "\n")
    path = str(pathlib.Path(game_dir) / name)
    markers.append(path)
    contents[path] = content

packages = []
for line in pathlib.Path(package_file).read_text().splitlines():
    if not line:
        continue
    source, separator, destination = line.partition("=")
    if not separator or destination.startswith("/") or ".." in pathlib.Path(destination).parts:
        raise SystemExit("package invalide")
    source_path = pathlib.Path(source)
    if not source_path.is_absolute():
        source_path = pathlib.Path(root) / source_path
    packages.append({
        "path": str(source_path),
        "sha256": path_hash(source_path),
        "destination": str(pathlib.Path(game_dir) / destination),
    })

session = pathlib.Path(session_root)
payload = {
    "request_version": 1,
    "request_id": request_id,
    "repository_root": root,
    "ppsspp_executable": ppsspp,
    "ppsspp_sha256": file_hash(ppsspp),
    "eboot_path": eboot,
    "eboot_sha256": file_hash(eboot),
    "memstick_root": memstick,
    "mode": mode,
    "presentation_profile": presentation,
    "graphics_backend": backend,
    "psp_renderer": renderer,
    "timeout_seconds": int(timeout),
    "expected_markers": markers,
    "expected_marker_contents": contents,
    "config_source": config,
    "config_sha256": file_hash(config),
    "environment_overrides": {
        "HOME": str(session / "home"),
        "XDG_CONFIG_HOME": str(session / "home" / ".config"),
        "XDG_CACHE_HOME": str(session / "xdg-cache"),
        "TMPDIR": str(session / "tmp"),
    },
    "result_path": result,
    "stdout_path": stdout,
    "stderr_path": stderr,
    "boot_signal_path": boot,
    "packages": packages,
}
pathlib.Path(output).write_text(json.dumps(payload, sort_keys=True, indent=2) + "\n")
PY

rm -f -- "$MARKER_FILE" "$PACKAGE_FILE" "$RESULT" "$BOOT_SIGNAL"
printf 'PPSSPP_GUI_REQUEST_QUEUED request_id=%s request=%s\n' \
  "$REQUEST_ID" "${REQUEST#"$PROJECT_ROOT"/}"

if [ "$ACTION" = queue ]; then
  printf 'classification=PENDING_GUI_EXECUTION\n'
  exit 0
fi

set +e
"$SCRIPT_DIR/submit-ppsspp-gui-request.sh" \
  --prepared-request "$REQUEST" --wait
BROKER_STATUS=$?
set -e
[ "$BROKER_STATUS" -eq 0 ]
