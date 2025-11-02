# Neutron Benchmark Suite v2.0

Performance benchmarks comparing Neutron and Python implementations with modern TUI output and output validation.

## Overview

The benchmark suite runs identical algorithms in both Neutron and Python, comparing their execution times with a beautiful, color-coded interface. **Output validation** ensures both languages produce identical results.

## Benchmarks Included (7 total)

### Algorithms (4 benchmarks)
1. **Fibonacci** - Iterative Fibonacci calculation (fibonacci(35))
2. **Prime Numbers** - Generate primes up to 1000
3. **Matrix Operations** - Multiply two 20x20 matrices
4. **Sorting Algorithms** - Bubble sort on 15-element array

### Math Operations (1 benchmark)
5. **Mathematical Functions** - Power, square root, GCD calculations

### String Operations (1 benchmark)
6. **String Manipulation** - String reversal, character counting, concatenation

### Loop Performance (1 benchmark)
7. **Loop Operations** - Various loop types and iterations

## Running Benchmarks

```bash
./run_benchmark.sh
```

### Output Format

The benchmark runner provides:
- 🎨 **Color-coded results** (Green = Neutron faster, Yellow = Python faster, Red = Failed/Mismatch)
- 📊 **Organized categories** (Algorithms, Math, Strings, Loops)
- ⚡ **Speed comparisons** (e.g., "Neutron 155.03x faster")
- ✅ **Output validation** - Ensures both languages produce identical results
- 📈 **Summary statistics** (Win rate, total benchmarks, success rate)
- ⏱️ **Precise timing** (millisecond accuracy)
- 🔍 **Mismatch detection** - Shows differences if outputs don't match

Example output:
```
╔════════════════════════════════╗
║  Neutron Benchmark Suite v2.0 ║
╚════════════════════════════════╝

Neutron: ./neutron
Python:  Python 3.13.7

Benchmarking: Algorithms
  Fibonacci                 Python: 8.059s     Neutron: 0.052s     Neutron 155.03x faster
  Prime Numbers             Python: 0.112s     Neutron: 0.061s     Neutron 1.84x faster
  Matrix Operations         Python: 0.223s     Neutron: 0.048s     Neutron 4.62x faster
  Sorting Algorithms        Python: 0.175s     Neutron: 0.053s     Neutron 3.26x faster

Benchmarking: Math Operations
  Mathematical Functions    Python: 0.107s     Neutron: 0.026s     Neutron 4.05x faster

Benchmarking: String Operations
  String Manipulation       Python: 0.121s     Neutron: 0.066s     Neutron 1.82x faster

Benchmarking: Loop Performance
  Loop Operations           Python: 0.147s     Neutron: 0.175s     Python 1.18x faster

════ BENCHMARK SUMMARY ═══
Total Benchmarks: 7
Neutron Faster:   6
Python Faster:    1

Neutron Win Rate: 85.7%

🎉 All benchmarks completed successfully! 🎉
```

## Performance Characteristics

### Neutron Strengths (85.7% win rate)
- **Recursive/Iterative algorithms** (e.g., Fibonacci) - significantly faster due to optimized call stack
- **Numeric computations** (e.g., Matrix operations, Prime numbers, Sorting) - 2-5x faster
- **Math operations** (e.g., Power, GCD, Square root) - 4x faster  
- **String operations** - Now faster than Python with proper optimizations
- **Low startup overhead** - faster initialization than Python

### Python Strengths
- **Complex nested loops** - Slightly faster due to mature CPython optimizations

## Output Validation

All benchmarks include **output validation** to ensure correctness:
- Both languages must produce identical output
- Mismatches are highlighted in red with detailed diff
- Prevents false performance wins from incorrect implementations
- Example outputs show actual computation results

## Benchmark Files Structure

```
benchmarks/
├── neutron/           # Neutron implementations
│   ├── fibonacci.nt
│   ├── primes.nt
│   ├── matrix.nt
│   ├── strings.nt
│   └── loops.nt
├── python/            # Python implementations
│   ├── fibonacci.py
│   ├── primes.py
│   ├── matrix.py
│   ├── strings.py
│   └── loops.py
├── results/           # Benchmark results (generated)
└── README.md         # This file
```

## Requirements

- **Neutron**: Built binary in project root or `build/` directory
- **Python**: Python 3.7+ with `bc` utility for calculations
- **System**: Linux, macOS, or MSYS2 on Windows

## Adding New Benchmarks

1. **Create Python implementation**:
   ```bash
   # benchmarks/python/my_benchmark.py
   # Your Python code here
   ```

2. **Create Neutron implementation**:
   ```bash
   # benchmarks/neutron/my_benchmark.nt
   # Your Neutron code here
   ```

3. **Add to benchmark runner**:
   Edit `run_benchmark.sh` and add:
   ```bash
   run_benchmark "My Benchmark" \
       "$BENCH_DIR/neutron/my_benchmark.nt" \
       "$BENCH_DIR/python/my_benchmark.py"
   ```

4. **Run and verify**:
   ```bash
   ./run_benchmark.sh
   ```

## Best Practices

- Keep implementations functionally identical
- Avoid I/O operations that would skew results
- Use `> /dev/null 2>&1` in runner to suppress output
- Ensure benchmarks run long enough to be measurable (>10ms)
- Test both languages work correctly before benchmarking

## Notes

- Benchmarks suppress output to avoid I/O overhead affecting results
- Times are measured using high-precision shell timing
- Results may vary based on system load and hardware
- Python JIT warmup not accounted for (cold starts)
- Both languages run in single-threaded mode

## Version History

- **v2.0** (November 2, 2025) - Modern TUI with color-coded output and categories
- **v1.0** (October 2025) - Initial benchmark suite