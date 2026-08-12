#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

variant=vanilla-relwithdebinfo
duration=20
transport=direct_gui
run_id="$(date -u +%Y%m%dT%H%M%SZ)-vanilla-boot"
trace_input=
trace_analog=
trace_scenario=
stage_request=
while [ "$#" -gt 0 ]; do
  case "$1" in
    --variant)
      variant="$2"
      shift 2
      ;;
    --duration)
      duration="$2"
      shift 2
      ;;
    --transport)
      transport="$2"
      shift 2
      ;;
    --run-id)
      run_id="$2"
      shift 2
      ;;
    --trace-input)
      trace_input="$2"
      shift 2
      ;;
    --trace-analog)
      trace_analog="$2"
      shift 2
      ;;
    --trace-scenario)
      trace_scenario="$2"
      shift 2
      ;;
    --stage)
      stage_request="$2"
      shift 2
      ;;
    *)
      die "argument inconnu : $1"
      ;;
  esac
done
case "$variant" in
  vanilla-relwithdebinfo|vanilla-debug|vanilla-debug-asan|trace-relwithdebinfo|trace-debug) ;;
  *) die "variante desktop invalide : $variant" ;;
esac
case "$transport" in
  direct_gui|launchservices_gui) ;;
  *) die "transport GUI invalide : $transport" ;;
esac
case "$duration" in
  ''|*[!0-9]*) die "durée invalide : $duration" ;;
esac
[ "$duration" -ge 1 ] || die "durée invalide : $duration"
case "$run_id" in
  ''|*[!A-Za-z0-9._-]*) die "identifiant d'exécution invalide : $run_id" ;;
esac
case "$trace_input" in
  *[!0-9A-Z_:,]*) die "script d'entrée trace invalide : $trace_input" ;;
esac
case "$trace_analog" in
  *[!0-9,:-]*) die "script analogique trace invalide : $trace_analog" ;;
esac
case "$trace_scenario" in
  *[!A-Za-z0-9._-]*) die "identifiant de scénario trace invalide : $trace_scenario" ;;
esac
case "$stage_request" in
  *[!A-Za-z0-9_,-]*) die "requête de stage invalide : $stage_request" ;;
esac
if [ -n "$trace_input$trace_analog$trace_scenario" ]; then
  case "$variant" in
    trace-*) ;;
    *) die "les options trace sont réservées aux variantes trace" ;;
  esac
fi
[ "$(uname -s)" = Darwin ] || die "le runner desktop GUI exige macOS"

APP="$(assert_project_path "build/reference-desktop/$variant/Dusklight.app")"
EXE="$APP/Contents/MacOS/Dusklight"
DVD="$(assert_project_path "game iso/Legend of Zelda, The - Twilight Princess.iso")"
SESSION="$(assert_project_path ".test-data/dusklight-reference/sessions/$run_id")"
HOME_DIR="$SESSION/home"
STDOUT_LOG="$SESSION/stdout.log"
STDERR_LOG="$SESSION/stderr.log"
RESULT="$SESSION/RESULT"
PREF_DIR="$HOME_DIR/Library/Application Support/TwilitRealm/Dusklight"

[ -x "$EXE" ] || die "binaire desktop absent : $variant"
[ -f "$DVD" ] || die "image locale autorisée absente"
safe_mkdir ".test-data/dusklight-reference/sessions/$run_id/home"
"$SCRIPT_DIR/prepare-dusklight-desktop-input-profile.sh" --home "$HOME_DIR" >/dev/null
: >"$STDOUT_LOG"
: >"$STDERR_LOG"

run_env=(
  "HOME=$HOME_DIR"
  "CFFIXED_USER_HOME=$HOME_DIR"
  "http_proxy=http://127.0.0.1:9"
  "https_proxy=http://127.0.0.1:9"
  "HTTP_PROXY=http://127.0.0.1:9"
  "HTTPS_PROXY=http://127.0.0.1:9"
  "ALL_PROXY=http://127.0.0.1:9"
  "NO_PROXY="
)
if [ -n "$trace_input" ]; then
  run_env+=("DUSKLIGHT_REFERENCE_INPUT=$trace_input")
fi
if [ -n "$trace_analog" ]; then
  run_env+=("DUSKLIGHT_REFERENCE_ANALOG=$trace_analog")
fi
if [ -n "$trace_scenario" ]; then
  run_env+=(
    "DUSKLIGHT_REFERENCE_SCENARIO=$trace_scenario"
    "DUSKLIGHT_PARITY_TRACE=1"
  )
fi

if [ "$transport" = direct_gui ]; then
  command=("$EXE" --dvd "$DVD")
  [ -z "$stage_request" ] || command+=(--stage "$stage_request")
  env "${run_env[@]}" "${command[@]}" >"$STDOUT_LOG" 2>"$STDERR_LOG" &
  launch_pid=$!
  app_pid=$launch_pid
else
  open_args=(-W -n "$APP" --stdout "$STDOUT_LOG" --stderr "$STDERR_LOG")
  for assignment in "${run_env[@]}"; do
    open_args+=(--env "$assignment")
  done
  open_args+=(--args --dvd "$DVD")
  [ -z "$stage_request" ] || open_args+=(--stage "$stage_request")
  /usr/bin/open "${open_args[@]}" &
  launch_pid=$!

  app_pid=
  for _ in $(jot 100); do
    app_pid="$(
      /usr/bin/pgrep -f 'Dusklight.app/Contents/MacOS/Dusklight' |
        tail -n 1 || true
    )"
    [ -n "$app_pid" ] && break
    if ! kill -0 "$launch_pid" 2>/dev/null; then
      break
    fi
    sleep 0.1
  done
fi

if [ -z "$app_pid" ]; then
  set +e
  wait "$launch_pid"
  launch_status=$?
  set -e
  {
    printf 'classification=DESKTOP_PROCESS_NOT_OBSERVED\n'
    printf 'transport=%s\n' "$transport"
    printf 'launch_status=%s\n' "$launch_status"
  } >"$RESULT"
  die "processus Dusklight non observé (voir ${SESSION#"$PROJECT_ROOT"/})"
fi

elapsed=0
while kill -0 "$app_pid" 2>/dev/null && [ "$elapsed" -lt "$duration" ]; do
  sleep 1
  elapsed=$((elapsed + 1))
done

terminated_by_runner=false
if kill -0 "$app_pid" 2>/dev/null; then
  terminated_by_runner=true
  kill -TERM "$app_pid"
fi
set +e
wait "$launch_pid"
launch_status=$?
set -e

expected_config="$PREF_DIR/config.json"
if ! grep -Fq "Loading config from '$expected_config'" "$STDOUT_LOG"; then
  {
    printf 'classification=DESKTOP_PROFILE_ISOLATION_FAILED\n'
    printf 'transport=%s\n' "$transport"
    printf 'launch_status=%s\n' "$launch_status"
    printf 'expected_config=%s\n' "${expected_config#"$PROJECT_ROOT"/}"
  } >"$RESULT"
  die "isolation du profil desktop non prouvée par le journal"
fi

{
  printf 'classification=DESKTOP_PROCESS_OBSERVED\n'
  printf 'variant=%s\n' "$variant"
  printf 'transport=%s\n' "$transport"
  printf 'app_pid=%s\n' "$app_pid"
  printf 'observed_seconds=%s\n' "$elapsed"
  printf 'terminated_by_runner=%s\n' "$terminated_by_runner"
  printf 'launch_status=%s\n' "$launch_status"
  printf 'profile=%s\n' "${HOME_DIR#"$PROJECT_ROOT"/}"
  printf 'dvd=game iso/Legend of Zelda, The - Twilight Princess.iso\n'
  printf 'trace_input=%s\n' "$trace_input"
  printf 'trace_analog=%s\n' "$trace_analog"
  printf 'trace_scenario=%s\n' "$trace_scenario"
  printf 'stage_request=%s\n' "$stage_request"
} >"$RESULT"

printf 'DUSKLIGHT_DESKTOP_GUI_RUN_COMPLETE run_id=%s transport=%s observed_seconds=%s terminated_by_runner=%s launch_status=%s\n' \
  "$run_id" "$transport" "$elapsed" "$terminated_by_runner" "$launch_status"
