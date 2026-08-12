#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

LOG="$PROJECT_ROOT/logs/test-orchestration/invocations.jsonl"
SELF_TEST=false
while [ "$#" -gt 0 ]; do
  case "$1" in
    --log)
      shift
      [ "$#" -gt 0 ] || die "--log exige un chemin"
      LOG="$1"
      ;;
    --self-test) SELF_TEST=true ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done

validate_log() {
  local path="$1"
  [ -f "$path" ] || die "journal d'orchestration absent : $path"
  awk '
    function value(name, line, prefix, rest) {
      prefix = "\"" name "\":"
      rest = line
      sub("^.*" prefix, "", rest)
      if (substr(rest, 1, 1) == "\"") {
        sub("^\"", "", rest)
        sub("\".*$", "", rest)
      } else {
        sub("[,}].*$", "", rest)
      }
      return rest
    }
    {
      campaign = value("invocation_id", $0)
      parent = value("parent_invocation_id", $0)
      test_id = value("test_id", $0)
      fp = value("fingerprint", $0)
      cache_hit = value("cache_hit", $0)
      launch = value("ppsspp_launch", $0)
      if (campaign == "" || test_id == "" || fp == "") invalid++
      if (parent != "null") nested++
      key = campaign SUBSEP test_id SUBSEP fp
      if (cache_hit == "false" && real[key]++) duplicate++
      if (launch == "true" && cache_hit == "false" && ppsspp[key]++) repeated++
    }
    END {
      if (invalid || nested || duplicate || repeated) {
        printf "ORCHESTRATION_INVALID invalid=%d nested_orchestrator_invocations=%d duplicate_test_invocations=%d identical_ppsspp_launches_repeated=%d\n",
          invalid, nested, duplicate, repeated > "/dev/stderr"
        exit 1
      }
      printf "ORCHESTRATION_LOG_OK duplicate_test_invocations=0 nested_orchestrator_invocations=0 identical_ppsspp_launches_repeated=0\n"
    }
  ' "$path"
}

if [ "$SELF_TEST" = true ]; then
  tmp_dir="$(assert_project_path ".tmp/orchestration-validator")"
  safe_mkdir .tmp/orchestration-validator
  good="$tmp_dir/good.jsonl"
  duplicate="$tmp_dir/duplicate.jsonl"
  nested="$tmp_dir/nested.jsonl"
  repeated="$tmp_dir/repeated.jsonl"
  printf '%s\n' \
    '{"invocation_id":"c1","parent_invocation_id":null,"test_id":"host.a","fingerprint":"f1","cache_hit":false,"ppsspp_launch":false}' \
    '{"invocation_id":"c1","parent_invocation_id":null,"test_id":"host.a","fingerprint":"f1","cache_hit":true,"ppsspp_launch":false}' >"$good"
  cp -- "$good" "$duplicate"
  printf '%s\n' \
    '{"invocation_id":"c1","parent_invocation_id":null,"test_id":"host.a","fingerprint":"f1","cache_hit":false,"ppsspp_launch":false}' >>"$duplicate"
  printf '%s\n' \
    '{"invocation_id":"c1","parent_invocation_id":"parent","test_id":"host.a","fingerprint":"f1","cache_hit":false,"ppsspp_launch":false}' >"$nested"
  printf '%s\n' \
    '{"invocation_id":"c1","parent_invocation_id":null,"test_id":"psp.a","fingerprint":"f1","cache_hit":false,"ppsspp_launch":true}' \
    '{"invocation_id":"c1","parent_invocation_id":null,"test_id":"psp.a","fingerprint":"f1","cache_hit":false,"ppsspp_launch":true}' >"$repeated"
  validate_log "$good"
  ! validate_log "$duplicate" >/dev/null 2>&1 ||
    die "le doublon réel n'a pas été rejeté"
  ! validate_log "$nested" >/dev/null 2>&1 ||
    die "l'orchestrateur imbriqué n'a pas été rejeté"
  ! validate_log "$repeated" >/dev/null 2>&1 ||
    die "le lancement PPSSPP répété n'a pas été rejeté"
  printf 'ORCHESTRATION_VALIDATOR_SELF_TEST_OK negative_cases=3\n'
else
  validate_log "$LOG"
fi
