#!/usr/bin/env bash
set -euo pipefail

# Run MP4 host workload scenarios in an ordered ladder and print the output marker.
#
# Usage:
#   tools/run_mp4_workload_ladder.sh
#   tools/run_mp4_workload_ladder.sh --from 0 --to 10
#
# The per-scenario marker is the first 8 bytes of the dumped output bin.

repo_root="$(cd "$(dirname "$0")/.." && pwd)"

FROM=0
TO=999999

while [[ $# -gt 0 ]]; do
  case "$1" in
    --from)
      FROM=${2:?missing value}
      shift 2
      ;;
    --to)
      TO=${2:?missing value}
      shift 2
      ;;
    *)
      echo "usage: $0 [--from N] [--to N]" >&2
      exit 2
      ;;
  esac
done

infer_out_rel() {
  local scenario_src="$1"
  awk '
    /gc_scenario_out_path[[:space:]]*[(]/ { in_fn=1 }
    in_fn && /return[[:space:]]*"/ {
      match($0, /return[[:space:]]*"[^"]+"/)
      if (RSTART > 0) {
        s=substr($0, RSTART, RLENGTH)
        sub(/^return[[:space:]]*"/, "", s)
        sub(/"$/, "", s)
        print s
        exit 0
      }
    }
    in_fn && /}/ { in_fn=0 }
  ' "$scenario_src" 2>/dev/null | head -n 1
}

show_marker() {
  local scenario_src="$1"
  local scenario_dir
  scenario_dir="$(cd "$(dirname "$scenario_src")" && pwd)"
  local out_rel
  out_rel="$(infer_out_rel "$scenario_src")"
  if [[ -z "$out_rel" ]]; then
    echo "(no out_path)" >&2
    return 1
  fi
  local out_abs
  out_abs="$(cd "$scenario_dir" && cd "$(dirname "$out_rel")" && pwd)/$(basename "$out_rel")"
  if [[ ! -f "$out_abs" ]]; then
    echo "(no output bin at $out_abs)" >&2
    return 1
  fi

  # Print first two 32-bit words (big-endian marker + DEADBEEF or similar).
  xxd -g 4 -l 8 "$out_abs" | awk '{print $2" "$3"  ("FILENAME")"}' FILENAME="$out_abs"
}

is_mtx_step() {
  local scenario_base="$1"
  case "$scenario_base" in
    mp4_mainloop_one_iter_001_scenario|mp4_mainloop_one_iter_tick_001_scenario|mp4_mainloop_two_iter_001_scenario|mp4_mainloop_two_iter_tick_001_scenario|mp4_mainloop_ten_iter_tick_001_scenario|mp4_mainloop_hundred_iter_tick_001_scenario|mp4_wipe_frame_still_mtx_001_scenario)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

scenarios=(
  "$repo_root/tests/workload/mp4/mp4_husysinit_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_huprcinit_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_hupadinit_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_gwinit_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_pfinit_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_husprinit_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_hu3dinit_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_hudatainit_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_huperfinit_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_init_to_viwait_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_mainloop_one_iter_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_mainloop_one_iter_tick_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_mainloop_two_iter_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_mainloop_two_iter_tick_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_mainloop_ten_iter_tick_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_mainloop_hundred_iter_tick_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_wipe_frame_still_mtx_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_process_scheduler_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_process_sleep_001_scenario.c"
  "$repo_root/tests/workload/mp4/mp4_process_vsleep_001_scenario.c"
)

idx=0
for s in "${scenarios[@]}"; do
  if (( idx < FROM || idx > TO )); then
    idx=$((idx+1))
    continue
  fi

  rel="${s#"$repo_root/"}"
  echo "[$idx] RUN $rel" >&2

  mkdir -p "$repo_root/tests/build/workload_logs"
  scenario_base="$(basename "$s" .c)"
  log="$repo_root/tests/build/workload_logs/${scenario_base}.log"

  run_env=()
  if is_mtx_step "$scenario_base"; then
    run_env+=(GC_HOST_WORKLOAD_MTX=1)
  fi

  if env "${run_env[@]+${run_env[@]}}" "$repo_root/tools/run_host_scenario.sh" "$s" >/dev/null 2>"$log"; then
    echo -n "[$idx] OK  $rel  marker: "
    show_marker "$s" || true
    if [[ -s "$log" ]]; then
      nlines="$(wc -l <"$log" | tr -d ' ')"
      echo "[$idx] note: build warnings logged: $log ($nlines lines)" >&2
    fi
  else
    rc=$?
    echo "[$idx] log: $log" >&2
    if [[ -s "$log" ]]; then
      tail -n 60 "$log" >&2 || true
    fi
    echo "[$idx] FAIL rc=$rc  $rel" >&2
    exit "$rc"
  fi

  idx=$((idx+1))
done

echo "DONE" >&2
