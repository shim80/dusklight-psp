#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
. "$SCRIPT_DIR/lib/common.sh"

[ "$#" -eq 2 ] || die "usage : $0 BASELINE.METRICS CANDIDATE.METRICS"
BASELINE="$(assert_project_path "$1")"
CANDIDATE="$(assert_project_path "$2")"
"$SCRIPT_DIR/validate-dusklight-benchmark-metrics.sh" "$BASELINE" >/dev/null
"$SCRIPT_DIR/validate-dusklight-benchmark-metrics.sh" "$CANDIDATE" >/dev/null

metric() {
  local file="$1" key="$2"
  awk -F= -v key="$key" '$1 == key {print substr($0, length(key) + 2); exit}' \
    "$file"
}

[ "$(metric "$BASELINE" scene)" = "$(metric "$CANDIDATE" scene)" ] ||
  die "les scènes diffèrent"
[ "$(metric "$BASELINE" profile)" = "$(metric "$CANDIDATE" profile)" ] ||
  die "les profils diffèrent"

awk -v scene="$(metric "$BASELINE" scene)" \
    -v before_fps="$(metric "$BASELINE" fps_average)" \
    -v after_fps="$(metric "$CANDIDATE" fps_average)" \
    -v before_frame="$(metric "$BASELINE" frame_time_us_average)" \
    -v after_frame="$(metric "$CANDIDATE" frame_time_us_average)" \
    -v before_mem="$(metric "$BASELINE" memory_peak_bytes)" \
    -v after_mem="$(metric "$CANDIDATE" memory_peak_bytes)" \
    -v before_edram="$(metric "$BASELINE" edram_peak_bytes)" \
    -v after_edram="$(metric "$CANDIDATE" edram_peak_bytes)" '
  BEGIN {
    printf "BENCHMARK_COMPARISON scene=%s\n", scene
    printf "fps_before=%.3f\nfps_after=%.3f\nfps_delta=%+.3f\n",
      before_fps, after_fps, after_fps - before_fps
    printf "frame_time_us_before=%d\nframe_time_us_after=%d\nframe_time_us_delta=%+d\n",
      before_frame, after_frame, after_frame - before_frame
    printf "memory_peak_before=%d\nmemory_peak_after=%d\nmemory_delta=%+d\n",
      before_mem, after_mem, after_mem - before_mem
    printf "edram_peak_before=%d\nedram_peak_after=%d\nedram_delta=%+d\n",
      before_edram, after_edram, after_edram - before_edram
  }'
