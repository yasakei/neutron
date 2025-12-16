#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PY_DIR="$SCRIPT_DIR/python"
AX_DIR="$SCRIPT_DIR/axity"
REPEAT="${REPEAT:-1}"
RESULTS_CSV="$SCRIPT_DIR/results.csv"

AX_EXE="$ROOT_DIR/target/release/axity"
if [[ ! -x "$AX_EXE" ]]; then
  AX_EXE="$ROOT_DIR/target/debug/axity"
fi

PYTHON_CMD="${PYTHON_CMD:-}"
if [[ -z "$PYTHON_CMD" ]]; then
  if command -v python3 >/dev/null 2>&1; then
    PYTHON_CMD="python3"
  elif command -v python >/dev/null 2>&1; then
    PYTHON_CMD="python"
  else
    echo "Python not found (python3/python)"; exit 1
  fi
fi

if [[ ! -f "$RESULTS_CSV" ]]; then
  echo "name,python_avg,python_min,python_max,axity_avg,axity_min,axity_max" > "$RESULTS_CSV"
fi

run() {
  local name="$1"
  echo "============================================"
  echo "$name"
  echo "--- Python ---"
  local times=() start end
  start="$(date +%s%N)"; "$PYTHON_CMD" "$PY_DIR/$name.py"; end="$(date +%s%N)"
  times+=($(awk "BEGIN {printf(\"%.3f\", ($end - $start)/1000000000)}"))
  for ((i=1;i<REPEAT;i++)); do
    start="$(date +%s%N)"; "$PYTHON_CMD" "$PY_DIR/$name.py" > /dev/null 2>&1; end="$(date +%s%N)"
    times+=($(awk "BEGIN {printf(\"%.3f\", ($end - $start)/1000000000)}"))
  done
  python_avg=$(printf "%s\n" "${times[@]}" | awk '{sum+=$1} END{printf "%.3f", sum/NR}')
  python_min=$(printf "%s\n" "${times[@]}" | sort -n | head -n1)
  python_max=$(printf "%s\n" "${times[@]}" | sort -n | tail -n1)
  printf "Time: %.3fs (avg %.3fs, min %.3fs, max %.3fs)\n" "${times[-1]}" "$python_avg" "$python_min" "$python_max"
  echo "--- Axity ---"
  if [[ -x "$AX_EXE" ]]; then
    local atimes=()
    start="$(date +%s%N)"; "$AX_EXE" "$AX_DIR/$name.ax"; end="$(date +%s%N)"
    atimes+=($(awk "BEGIN {printf(\"%.3f\", ($end - $start)/1000000000)}"))
    for ((i=1;i<REPEAT;i++)); do
      start="$(date +%s%N)"; "$AX_EXE" "$AX_DIR/$name.ax" > /dev/null 2>&1; end="$(date +%s%N)"
      atimes+=($(awk "BEGIN {printf(\"%.3f\", ($end - $start)/1000000000)}"))
    done
    ax_avg=$(printf "%s\n" "${atimes[@]}" | awk '{sum+=$1} END{printf "%.3f", sum/NR}')
    ax_min=$(printf "%s\n" "${atimes[@]}" | sort -n | head -n1)
    ax_max=$(printf "%s\n" "${atimes[@]}" | sort -n | tail -n1)
    printf "Time: %.3fs (avg %.3fs, min %.3fs, max %.3fs)\n" "${atimes[-1]}" "$ax_avg" "$ax_min" "$ax_max"
    echo "$name,$python_avg,$python_min,$python_max,$ax_avg,$ax_min,$ax_max" >> "$RESULTS_CSV"
  else
    echo "Skipping Axity for $name (axity binary not found)"
  fi
  echo
}

for n in ackermann dict_ops fibonacci list_ops loops math matrix nested_loops primes recursion sorting string_ops strings; do
  run "$n"
done
