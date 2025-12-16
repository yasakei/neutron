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
ORANGE='\033[0;33m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Benchmark directory
BENCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/benchmarks" && pwd)"

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}Python3 is not installed or not in PATH${NC}"
    exit 1
fi

# Check if Cargo is available for Axity
if ! command -v cargo &> /dev/null; then
    echo -e "${RED}Cargo is not installed or not in PATH (needed for Axity benchmarks)${NC}"
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
axity_faster=0
failed_benchmarks=0

# Print header
echo -e "${CYAN}╔═══════════════════════════════════╗${NC}"
echo -e "${CYAN}║  Neutron Benchmark Suite v2.0     ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════╝${NC}"
echo ""
echo -e "${BLUE}Neutron:${NC} ${NEUTRON_BIN}"
echo -e "${BLUE}Python:${NC}  $(python3 --version 2>&1)"
echo -e "${BLUE}Axity:${NC}   $(cd Axity && cargo run --version 2>/dev/null | head -c 25 || echo 'Rust-based language')"
echo ""

# Function to run and time a single benchmark
run_benchmark() {
    local name=$1
    local neutron_file=$2
    local python_file=$3
    local axity_file=$4
    
    total_benchmarks=$((total_benchmarks + 1))
    
    # Run Python version and capture output
    python_start=$(python3 -c 'import time; print(time.time())')
    python_output=$(python3 "$python_file" 2>&1)
    python_exit=$?
    python_end=$(python3 -c 'import time; print(time.time())')

    if [ $python_exit -eq 0 ]; then
        python_time=$(echo "$python_end - $python_start" | bc -l)
        python_success=true
    else
        python_time=0
        python_success=false
    fi

    # Run Neutron version and capture output
    neutron_start=$(python3 -c 'import time; print(time.time())')
    neutron_output=$("$NEUTRON_BIN" "$neutron_file" 2>&1)
    neutron_exit=$?
    neutron_end=$(python3 -c 'import time; print(time.time())')

    if [ $neutron_exit -eq 0 ]; then
        neutron_time=$(echo "$neutron_end - $neutron_start" | bc -l)
        neutron_success=true
    else
        neutron_time=0
        neutron_success=false
    fi

    # Run Axity version and capture output (suppress build warnings)
    axity_start=$(python3 -c 'import time; print(time.time())')
    # Only capture stdout, suppress stderr to avoid build warnings
    axity_output=$(cd Axity && cargo run --quiet -- "$axity_file" 2>/dev/null)
    axity_exit=$?
    axity_end=$(python3 -c 'import time; print(time.time())')

    if [ $axity_exit -eq 0 ]; then
        axity_time=$(echo "$axity_end - $axity_start" | bc -l)
        axity_success=true
    else
        axity_time=0
        axity_success=false
    fi
    
    # Compare outputs for consistency (check if all successful outputs match)
    output_match=true
    if [ "$python_success" = true ] && [ "$neutron_success" = true ] && [ "$axity_success" = true ]; then
        if [ "$python_output" != "$neutron_output" ] || [ "$python_output" != "$axity_output" ]; then
            output_match=false
        fi
    elif [ "$python_success" = true ] && [ "$neutron_success" = true ]; then
        if [ "$python_output" != "$neutron_output" ]; then
            output_match=false
        fi
    elif [ "$python_success" = true ] && [ "$axity_success" = true ]; then
        if [ "$python_output" != "$axity_output" ]; then
            output_match=false
        fi
    elif [ "$neutron_success" = true ] && [ "$axity_success" = true ]; then
        if [ "$neutron_output" != "$axity_output" ]; then
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

    if [ "$axity_success" = true ]; then
        axity_display=$(printf "%.3fs" $axity_time)
    else
        axity_display="${RED}FAILED${NC}"
    fi

    # Compare performance among all three
    if [ "$python_success" = true ] && [ "$neutron_success" = true ] && [ "$axity_success" = true ]; then
        if [ "$output_match" = false ]; then
            failed_benchmarks=$((failed_benchmarks + 1))
            result="${RED}OUTPUT MISMATCH${NC}"
        else
            # Determine the fastest implementation
            if (( $(echo "$neutron_time < $python_time && $neutron_time < $axity_time" | bc -l) )); then
                neutron_faster=$((neutron_faster + 1))
                python_ratio=$(echo "scale=2; $python_time / $neutron_time" | bc -l)
                axity_ratio=$(echo "scale=2; $axity_time / $neutron_time" | bc -l)
                speedup=$(printf "%.2fx" $python_ratio)
                result="${GREEN}Neutron ${speedup} faster than Python${NC}, "
                speedup_axity=$(printf "%.2fx" $axity_ratio)
                result="${result}${GREEN}Axity ${speedup_axity} faster${NC}"
            elif (( $(echo "$axity_time < $python_time && $axity_time < $neutron_time" | bc -l) )); then
                axity_faster=$((axity_faster + 1))
                python_ratio=$(echo "scale=2; $python_time / $axity_time" | bc -l)
                neutron_ratio=$(echo "scale=2; $neutron_time / $axity_time" | bc -l)
                speedup=$(printf "%.2fx" $python_ratio)
                result="${ORANGE}Axity ${speedup} faster than Python${NC}, "
                speedup_neutron=$(printf "%.2fx" $neutron_ratio)
                result="${result}${MAGENTA}Neutron ${speedup_neutron} faster${NC}"
            elif (( $(echo "$python_time < $neutron_time && $python_time < $axity_time" | bc -l) )); then
                python_faster=$((python_faster + 1))
                neutron_ratio=$(echo "scale=2; $neutron_time / $python_time" | bc -l)
                axity_ratio=$(echo "scale=2; $axity_time / $python_time" | bc -l)
                speedup=$(printf "%.2fx" $neutron_ratio)
                result="${YELLOW}Python ${speedup} faster than Neutron${NC}, "
                speedup_axity=$(printf "%.2fx" $axity_ratio)
                result="${result}${ORANGE}Axity ${speedup_axity} faster${NC}"
            else
                # Handle cases where two are equal or close
                if (( $(echo "$neutron_time <= $axity_time && $neutron_time < $python_time" | bc -l) )); then
                    neutron_faster=$((neutron_faster + 1))
                    python_ratio=$(echo "scale=2; $python_time / $neutron_time" | bc -l)
                    speedup=$(printf "%.2fx" $python_ratio)
                    result="${GREEN}Neutron ${speedup} faster than Python${NC}, "
                    axity_ratio=$(echo "scale=2; $axity_time / $neutron_time" | bc -l)
                    speedup_axity=$(printf "%.2fx" $axity_ratio)
                    result="${result}${ORANGE}Axity ${speedup_axity} faster${NC}"
                elif (( $(echo "$axity_time < $neutron_time && $axity_time <= $python_time" | bc -l) )); then
                    axity_faster=$((axity_faster + 1))
                    python_ratio=$(echo "scale=2; $python_time / $axity_time" | bc -l)
                    speedup=$(printf "%.2fx" $python_ratio)
                    result="${ORANGE}Axity ${speedup} faster than Python${NC}, "
                    neutron_ratio=$(echo "scale=2; $neutron_time / $axity_time" | bc -l)
                    speedup_neutron=$(printf "%.2fx" $neutron_ratio)
                    result="${result}${MAGENTA}Neutron ${speedup_neutron} faster${NC}"
                else
                    python_faster=$((python_faster + 1))
                    neutron_ratio=$(echo "scale=2; $neutron_time / $python_time" | bc -l)
                    axity_ratio=$(echo "scale=2; $axity_time / $python_time" | bc -l)
                    speedup=$(printf "%.2fx" $neutron_ratio)
                    result="${YELLOW}Python ${speedup} faster than Neutron${NC}, "
                    speedup_axity=$(printf "%.2fx" $axity_ratio)
                    result="${result}${ORANGE}Axity ${speedup_axity} faster${NC}"
                fi
            fi
        fi
    elif [ "$python_success" = true ] && [ "$neutron_success" = true ]; then
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
    elif [ "$python_success" = true ] && [ "$axity_success" = true ]; then
        if [ "$output_match" = false ]; then
            failed_benchmarks=$((failed_benchmarks + 1))
            result="${RED}OUTPUT MISMATCH${NC}"
        else
            ratio=$(echo "scale=2; $python_time / $axity_time" | bc -l)

            if (( $(echo "$axity_time < $python_time" | bc -l) )); then
                axity_faster=$((axity_faster + 1))
                speedup=$(printf "%.2fx" $ratio)
                result="${ORANGE}Axity ${speedup} faster${NC}"
            else
                python_faster=$((python_faster + 1))
                ratio_inv=$(echo "scale=2; $axity_time / $python_time" | bc -l)
                speedup=$(printf "%.2fx" $ratio_inv)
                result="${YELLOW}Python ${speedup} faster${NC}"
            fi
        fi
    elif [ "$neutron_success" = true ] && [ "$axity_success" = true ]; then
        if [ "$output_match" = false ]; then
            failed_benchmarks=$((failed_benchmarks + 1))
            result="${RED}OUTPUT MISMATCH${NC}"
        else
            ratio=$(echo "scale=2; $neutron_time / $axity_time" | bc -l)

            if (( $(echo "$axity_time < $neutron_time" | bc -l) )); then
                axity_faster=$((axity_faster + 1))
                speedup=$(printf "%.2fx" $ratio)
                result="${ORANGE}Axity ${speedup} faster${NC}"
            else
                neutron_faster=$((neutron_faster + 1))
                ratio_inv=$(echo "scale=2; $axity_time / $neutron_time" | bc -l)
                speedup=$(printf "%.2fx" $ratio_inv)
                result="${GREEN}Neutron ${speedup} faster${NC}"
            fi
        fi
    else
        failed_benchmarks=$((failed_benchmarks + 1))
        result="${RED}BENCHMARK FAILED${NC}"
    fi

    # Print result
    echo -e "  $(printf '%-20s' "$name") ${CYAN}Python:${NC} $(printf '%-10s' "$python_display") ${MAGENTA}Neutron:${NC} $(printf '%-10s' "$neutron_display") ${ORANGE}Axity:${NC} $(printf '%-10s' "$axity_display") $result"

    # Show output details if mismatch
    if [ "$output_match" = false ]; then
        echo -e "    ${RED}Python output:${NC}"
        echo "$python_output" | sed 's/^/      /'
        echo -e "    ${RED}Neutron output:${NC}"
        echo "$neutron_output" | sed 's/^/      /'
        echo -e "    ${RED}Axity output:${NC}"
        echo "$axity_output" | sed 's/^/      /'
    fi
}

# Run benchmarks by category
echo -e "${BOLD}Benchmarking: Algorithms${NC}"

run_benchmark "Fibonacci" \
    "$BENCH_DIR/neutron/fibonacci.nt" \
    "$BENCH_DIR/python/fibonacci.py" \
    "$BENCH_DIR/axity/fibonacci.ax"

run_benchmark "Prime Numbers" \
    "$BENCH_DIR/neutron/primes.nt" \
    "$BENCH_DIR/python/primes.py" \
    "$BENCH_DIR/axity/primes.ax"

run_benchmark "Matrix Operations" \
    "$BENCH_DIR/neutron/matrix.nt" \
    "$BENCH_DIR/python/matrix.py" \
    "$BENCH_DIR/axity/matrix.ax"

echo ""
echo -e "${BOLD}Benchmarking: Recursion & Function Calls${NC}"

run_benchmark "Deep Recursion" \
    "$BENCH_DIR/neutron/ackermann.nt" \
    "$BENCH_DIR/python/ackermann.py" \
    "$BENCH_DIR/axity/ackermann.ax"

echo ""
echo -e "${BOLD}Benchmarking: Data Structures${NC}"

run_benchmark "Sorting Algorithms" \
    "$BENCH_DIR/neutron/sorting.nt" \
    "$BENCH_DIR/python/sorting.py" \
    "$BENCH_DIR/axity/sorting.ax"

run_benchmark "Array/List Operations" \
    "$BENCH_DIR/neutron/list_ops.nt" \
    "$BENCH_DIR/python/list_ops.py" \
    "$BENCH_DIR/axity/list_ops.ax"

run_benchmark "Object/Dictionary Operations" \
    "$BENCH_DIR/neutron/dict_ops.nt" \
    "$BENCH_DIR/python/dict_ops.py" \
    "$BENCH_DIR/axity/dict_ops.ax"

echo ""
echo -e "${BOLD}Benchmarking: Math Operations${NC}"

run_benchmark "Mathematical Functions" \
    "$BENCH_DIR/neutron/math.nt" \
    "$BENCH_DIR/python/math.py" \
    "$BENCH_DIR/axity/math.ax"

echo ""
echo -e "${BOLD}Benchmarking: String Operations${NC}"

run_benchmark "String Manipulation" \
    "$BENCH_DIR/neutron/strings.nt" \
    "$BENCH_DIR/python/strings.py" \
    "$BENCH_DIR/axity/strings.ax"

run_benchmark "Advanced String Operations" \
    "$BENCH_DIR/neutron/string_ops.nt" \
    "$BENCH_DIR/python/string_ops.py" \
    "$BENCH_DIR/axity/string_ops.ax"

echo ""
echo -e "${BOLD}Benchmarking: Loop Performance${NC}"

run_benchmark "Loop Operations" \
    "$BENCH_DIR/neutron/loops.nt" \
    "$BENCH_DIR/python/loops.py" \
    "$BENCH_DIR/axity/loops.ax"

run_benchmark "Nested Loops" \
    "$BENCH_DIR/neutron/nested_loops.nt" \
    "$BENCH_DIR/python/nested_loops.py" \
    "$BENCH_DIR/axity/nested_loops.ax"

# Print summary
echo ""
echo -e "${CYAN}════ BENCHMARK SUMMARY ═══${NC}"
echo -e "Total Benchmarks: ${BOLD}$total_benchmarks${NC}"
echo -e "${GREEN}Neutron Faster:   $neutron_faster${NC}"
echo -e "${YELLOW}Python Faster:    $python_faster${NC}"
echo -e "${ORANGE}Axity Faster:     $axity_faster${NC}"

if [ $failed_benchmarks -gt 0 ]; then
    echo -e "${RED}Failed:           $failed_benchmarks${NC}"
fi

echo ""

# Calculate win rates for each language
if [ $total_benchmarks -gt 0 ]; then
    success_rate=$((total_benchmarks - failed_benchmarks))
    if [ $success_rate -gt 0 ]; then
        neutron_win_rate=$(echo "scale=1; ($neutron_faster * 100) / $success_rate" | bc -l)
        python_win_rate=$(echo "scale=1; ($python_faster * 100) / $success_rate" | bc -l)
        axity_win_rate=$(echo "scale=1; ($axity_faster * 100) / $success_rate" | bc -l)
        echo -e "${BOLD}Win Rates:${NC}"
        echo -e "  ${GREEN}Neutron: ${neutron_win_rate}%${NC}"
        echo -e "  ${YELLOW}Python:  ${python_win_rate}%${NC}"
        echo -e "  ${ORANGE}Axity:   ${axity_win_rate}%${NC}"
    fi
fi

# Exit with appropriate code
if [ $failed_benchmarks -gt 0 ]; then
    exit 1
else
    echo ""
    echo -e "${GREEN}🎉 All benchmarks completed successfully! 🎉${NC}"
    exit 0
fi
