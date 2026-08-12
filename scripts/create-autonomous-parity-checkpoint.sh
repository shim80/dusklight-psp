#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

task_id=
next_action=
visual_pipeline=false
while [ "$#" -gt 0 ]; do
  case "$1" in
    --task)
      task_id="$2"
      shift 2
      ;;
    --next-action)
      next_action="$2"
      shift 2
      ;;
    --visual-pipeline)
      visual_pipeline=true
      shift
      ;;
    *)
      die "argument checkpoint inconnu : $1"
      ;;
  esac
done

case "$task_id" in
  ''|*[!A-Za-z0-9._-]*) die "task_id checkpoint invalide" ;;
esac

timestamp="$(date -u +%Y%m%dT%H%M%SZ)"
if [ "$visual_pipeline" = true ]; then
  relative=".test-data/visual-pipeline-recovery/${timestamp}-${task_id}"
else
  relative=".test-data/autonomous-parity-checkpoints/${timestamp}-${task_id}"
fi
checkpoint="$(assert_project_path "$relative")"
[ ! -e "$checkpoint" ] || die "checkpoint déjà présent : $relative"
mkdir -p "$checkpoint"

git -C "$PROJECT_ROOT" diff --binary --no-ext-diff >"$checkpoint/working-tree.patch"
git -C "$PROJECT_ROOT" ls-files --others --exclude-standard >"$checkpoint/untracked-files.txt"
git -C "$PROJECT_ROOT" status --porcelain=v1 >"$checkpoint/status.txt"
git -C "$PROJECT_ROOT" rev-parse HEAD >"$checkpoint/head.txt"

{
  git -C "$PROJECT_ROOT" diff --name-only --no-ext-diff
  git -C "$PROJECT_ROOT" diff --cached --name-only --no-ext-diff
  git -C "$PROJECT_ROOT" ls-files --others --exclude-standard
} | LC_ALL=C sort -u | while IFS= read -r path; do
  [ -n "$path" ] || continue
  absolute="$(assert_project_path "$path")"
  [ -f "$absolute" ] || continue
  [ ! -L "$absolute" ] || die "lien symbolique refusé dans le checkpoint : $path"
  digest="$(shasum -a 256 "$absolute" | awk '{print $1}')"
  printf '%s  %s\n' "$digest" "$path"
done >"$checkpoint/file-hashes.txt"

queue="$(assert_project_path reference/parity/AUTONOMOUS_WORK_QUEUE.json)"
resume="$(assert_project_path reference/parity/CAUSAL_RESUME_STATE.json)"
[ -f "$queue" ] && cp "$queue" "$checkpoint/work-queue.json" || printf '{}\n' >"$checkpoint/work-queue.json"
[ -f "$resume" ] && cp "$resume" "$checkpoint/causal-resume-state.json" || printf '{}\n' >"$checkpoint/causal-resume-state.json"

request_id=
if [ -f "$resume" ]; then
  request_id="$(jq -r '.last_broker_response.request_id // empty' "$resume")"
fi
response="$(assert_project_path ".test-data/ppsspp-gui-broker/responses/${request_id}.json")"
if [ -n "$request_id" ] && [ -f "$response" ]; then
  cp "$response" "$checkpoint/last-broker-response.json"
else
  printf '{}\n' >"$checkpoint/last-broker-response.json"
fi
if [ "$visual_pipeline" = true ]; then
  cp "$checkpoint/last-broker-response.json" "$checkpoint/last-response.json"
fi

printf '%s\n' "$next_action" >"$checkpoint/next-action.txt"
printf 'AUTONOMOUS_PARITY_CHECKPOINT_OK path=%s task=%s\n' "$relative" "$task_id"
