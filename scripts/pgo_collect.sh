#!/usr/bin/env bash
set -euo pipefail

# PGO collection script for Neutron
# Usage:  scripts/pgo_collect.sh [benchmark ...]
#
# Default: runs the full test suite as the profile workload.
# To run specific benchmarks:
#   scripts/pgo_collect.sh benchmarks/neutron/mandelbrot.nt benchmarks/neutron/nbody.nt

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${BUILD_DIR:-${REPO_DIR}/build-pgo}"
PROFILE_DIR="${BUILD_DIR}/pgo-profiles"
PROFILE_DATA="${REPO_DIR}/pgo.profdata"

echo "=== Phase 1: Build instrumented binary ==="
cmake -S "${REPO_DIR}" -B "${BUILD_DIR}" -DNEUTRON_PGO=GENERATE -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || echo 4)"

echo ""
echo "=== Phase 2: Run workload to collect profiles ==="
export LLVM_PROFILE_FILE="${PROFILE_DIR}/default_%p.profraw"

if [ $# -gt 0 ]; then
    # Run specific benchmarks
    for bench in "$@"; do
        echo "  Running: ${bench}"
        "${BUILD_DIR}/neutron" "${bench}" >/dev/null 2>&1 || true
    done
else
    # Run the full test suite as profile workload
    echo "  Running full test suite..."
    python3 "${REPO_DIR}/tests/run_tests.py" 2>&1 | tail -5
fi

echo ""
echo "=== Phase 3: Merge profiles ==="
LLVM_PROFCMD="${LLVM_PROFCMD:-$(command -v llvm-profdata 2>/dev/null || echo /usr/bin/llvm-profdata)}"

if ls "${PROFILE_DIR}"/default_*.profraw 1>/dev/null 2>&1; then
    "${LLVM_PROFCMD}" merge -output="${PROFILE_DATA}" "${PROFILE_DIR}"/default_*.profraw
    echo "  Profile data written to: ${PROFILE_DATA}"
    echo "  Size: $(wc -c < "${PROFILE_DATA}") bytes"
    echo ""
    echo "=== Done ==="
    echo "Next step: cmake -B build-release -DNEUTRON_PGO=USE=${PROFILE_DATA} && cmake --build build-release"
else
    echo "  WARNING: No .profraw files found in ${PROFILE_DIR}"
    echo "  The instrumented binary may not have been executed."
    exit 1
fi
