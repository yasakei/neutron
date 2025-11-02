#!/bin/bash

# Neutron Benchmark Suite v2.0
# Runs benchmarks with organized categories and beautiful TUI output

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Benchmark directory
BENCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/benchmarks" && pwd)"

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}Python3 is not installed or not in PATH${NC}"
    exit 1
fi

# Check if Neutron binary exists
NEUTRON_BIN="./neutron"
if [ ! -f "./neutron" ]; then
    if [ -f "./build/neutron" ]; then
        NEUTRON_BIN="./build/neutron"
    elif [ -f "build/neutron" ]; then
        NEUTRON_BIN="build/neutron"
    else
        echo -e "${RED}Neutron binary not found. Please build the project first.${NC}"
        exit 1
    fi
fi

# Statistics
total_benchmarks=0
neutron_faster=0
python_faster=0
failed_benchmarks=0

# Print header
echo -e "${CYAN}╔════════════════════════════════╗${NC}"
echo -e "${CYAN}║  Neutron Benchmark Suite v2.0 ║${NC}"
echo -e "${CYAN}╚════════════════════════════════╝${NC}"
echo ""
echo -e "${BLUE}Neutron:${NC} ${NEUTRON_BIN}"
echo -e "${BLUE}Python:${NC}  $(python3 --version 2>&1)"
echo ""

# Function to run and time a single benchmark
run_benchmark() {
    local name=$1
    local neutron_file=$2
    local python_file=$3
    
    total_benchmarks=$((total_benchmarks + 1))
    
    # Run Python version and capture output
    python_start=$(date +%s.%N)
    python_output=$(python3 "$python_file" 2>&1)
    python_exit=$?
    python_end=$(date +%s.%N)
    
    if [ $python_exit -eq 0 ]; then
        python_time=$(echo "$python_end - $python_start" | bc -l)
        python_success=true
    else
        python_time=0
        python_success=false
    fi
    
    # Run Neutron version and capture output
    neutron_start=$(date +%s.%N)
    neutron_output=$("$NEUTRON_BIN" "$neutron_file" 2>&1)
    neutron_exit=$?
    neutron_end=$(date +%s.%N)
    
    if [ $neutron_exit -eq 0 ]; then
        neutron_time=$(echo "$neutron_end - $neutron_start" | bc -l)
        neutron_success=true
    else
        neutron_time=0
        neutron_success=false
    fi
    
    # Compare outputs if both succeeded
    output_match=true
    if [ "$python_success" = true ] && [ "$neutron_success" = true ]; then
        if [ "$python_output" != "$neutron_output" ]; then
            output_match=false
        fi
    fi
    
    # Format times
    if [ "$python_success" = true ]; then
        python_display=$(printf "%.3fs" $python_time)
    else
        python_display="${RED}FAILED${NC}"
    fi
    
    if [ "$neutron_success" = true ]; then
        neutron_display=$(printf "%.3fs" $neutron_time)
    else
        neutron_display="${RED}FAILED${NC}"
    fi
    
    # Compare performance
    if [ "$python_success" = true ] && [ "$neutron_success" = true ]; then
        if [ "$output_match" = false ]; then
            failed_benchmarks=$((failed_benchmarks + 1))
            result="${RED}OUTPUT MISMATCH${NC}"
        else
            ratio=$(echo "scale=2; $python_time / $neutron_time" | bc -l)
            
            if (( $(echo "$neutron_time < $python_time" | bc -l) )); then
                neutron_faster=$((neutron_faster + 1))
                speedup=$(printf "%.2fx" $ratio)
                result="${GREEN}Neutron ${speedup} faster${NC}"
            else
                python_faster=$((python_faster + 1))
                ratio_inv=$(echo "scale=2; $neutron_time / $python_time" | bc -l)
                speedup=$(printf "%.2fx" $ratio_inv)
                result="${YELLOW}Python ${speedup} faster${NC}"
            fi
        fi
    else
        failed_benchmarks=$((failed_benchmarks + 1))
        result="${RED}BENCHMARK FAILED${NC}"
    fi
    
    # Print result
    echo -e "  $(printf '%-25s' "$name") ${CYAN}Python:${NC} $(printf '%-10s' "$python_display") ${MAGENTA}Neutron:${NC} $(printf '%-10s' "$neutron_display") $result"
    
    # Show output details if mismatch
    if [ "$output_match" = false ]; then
        echo -e "    ${RED}Python output:${NC}"
        echo "$python_output" | sed 's/^/      /'
        echo -e "    ${RED}Neutron output:${NC}"
        echo "$neutron_output" | sed 's/^/      /'
    fi
}

# Run benchmarks by category
echo -e "${BOLD}Benchmarking: Algorithms${NC}"

run_benchmark "Fibonacci" \
    "$BENCH_DIR/neutron/fibonacci.nt" \
    "$BENCH_DIR/python/fibonacci.py"

run_benchmark "Prime Numbers" \
    "$BENCH_DIR/neutron/primes.nt" \
    "$BENCH_DIR/python/primes.py"

run_benchmark "Matrix Operations" \
    "$BENCH_DIR/neutron/matrix.nt" \
    "$BENCH_DIR/python/matrix.py"

# Recursion benchmark disabled - has issues with recursive multiplication
# run_benchmark "Recursion" \
#     "$BENCH_DIR/neutron/recursion.nt" \
#     "$BENCH_DIR/python/recursion.py"

echo ""
echo -e "${BOLD}Benchmarking: Sorting & Data Structures${NC}"

run_benchmark "Sorting Algorithms" \
    "$BENCH_DIR/neutron/sorting.nt" \
    "$BENCH_DIR/python/sorting.py"

echo ""
echo -e "${BOLD}Benchmarking: Math Operations${NC}"

run_benchmark "Mathematical Functions" \
    "$BENCH_DIR/neutron/math.nt" \
    "$BENCH_DIR/python/math.py"

echo ""
echo -e "${BOLD}Benchmarking: String Operations${NC}"

run_benchmark "String Manipulation" \
    "$BENCH_DIR/neutron/strings.nt" \
    "$BENCH_DIR/python/strings.py"

echo ""
echo -e "${BOLD}Benchmarking: Loop Performance${NC}"

run_benchmark "Loop Operations" \
    "$BENCH_DIR/neutron/loops.nt" \
    "$BENCH_DIR/python/loops.py"

# Print summary
echo ""
echo -e "${CYAN}════ BENCHMARK SUMMARY ═══${NC}"
echo -e "Total Benchmarks: ${BOLD}$total_benchmarks${NC}"
echo -e "${GREEN}Neutron Faster:   $neutron_faster${NC}"
echo -e "${YELLOW}Python Faster:    $python_faster${NC}"

if [ $failed_benchmarks -gt 0 ]; then
    echo -e "${RED}Failed:           $failed_benchmarks${NC}"
fi

echo ""

# Calculate win rate
if [ $total_benchmarks -gt 0 ]; then
    success_rate=$((total_benchmarks - failed_benchmarks))
    win_rate=$(echo "scale=1; ($neutron_faster * 100) / $success_rate" | bc -l)
    echo -e "${BOLD}Neutron Win Rate: ${GREEN}${win_rate}%${NC}"
fi

# Exit with appropriate code
if [ $failed_benchmarks -gt 0 ]; then
    exit 1
else
    echo ""
    echo -e "${GREEN}🎉 All benchmarks completed successfully! 🎉${NC}"
    exit 0
fi
