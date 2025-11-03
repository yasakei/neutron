# Neutron Benchmark Suite v2.0

Performance benchmarks comparing Neutron and Python implementations with modern TUI output and output validation.

## Overview

The benchmark suite runs identical algorithms in both Neutron and Python, comparing their execution times with a beautiful, color-coded interface. **Output validation** ensures both languages produce identical results.

## Benchmarks Included (12 total)

### Algorithms (3 benchmarks)
1. **Fibonacci** - Iterative Fibonacci calculation (fibonacci(35))
2. **Prime Numbers** - Generate primes up to 1000
3. **Matrix Operations** - Multiply two 20x20 matrices

### Recursion & Function Calls (1 benchmark)
4. **Deep Recursion** - Ackermann function with deep recursion

### Data Structures (3 benchmarks)
5. **Sorting Algorithms** - Bubble sort on 15-element array
6. **Array/List Operations** - List creation, indexing, iteration, and transformations
7. **Object/Dictionary Operations** - Dictionary creation, access, modification, and iteration

### Math Operations (1 benchmark)
8. **Mathematical Functions** - Power, square root, GCD calculations

### String Operations (2 benchmarks)
9. **String Manipulation** - String reversal, character counting, concatenation
10. **Advanced String Operations** - Complex string processing including splitting, joining, and pattern matching

### Loop Performance (2 benchmarks)
11. **Loop Operations** - Various loop types and iterations
12. **Nested Loops** - Multiple nested loop combinations and complex iterations

## Running Benchmarks

```bash
./run_benchmark.sh
```

### Output Format

The benchmark runner provides:
- 🎨 **Color-coded results** (Green = Neutron faster, Yellow = Python faster, Red = Failed/Mismatch)
- 📊 **Organized categories** (Algorithms, Recursion & Function Calls, Data Structures, Math Operations, String Operations, Loop Performance)
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

Benchmarking: Recursion & Function Calls
  Deep Recursion            Python: 0.234s     Neutron: 0.087s     Neutron 2.69x faster

Benchmarking: Data Structures
  Sorting Algorithms        Python: 0.175s     Neutron: 0.053s     Neutron 3.26x faster
  Array/List Operations     Python: 0.089s     Neutron: 0.045s     Neutron 1.98x faster
  Object/Dictionary Operations  Python: 0.102s  Neutron: 0.051s     Neutron 2.00x faster

Benchmarking: Math Operations
  Mathematical Functions    Python: 0.107s     Neutron: 0.026s     Neutron 4.05x faster

Benchmarking: String Operations
  String Manipulation       Python: 0.121s     Neutron: 0.066s     Neutron 1.82x faster
  Advanced String Operations Python: 0.098s    Neutron: 0.054s     Neutron 1.81x faster

Benchmarking: Loop Performance
  Loop Operations           Python: 0.147s     Neutron: 0.175s     Python 1.18x faster
  Nested Loops              Python: 0.156s     Neutron: 0.189s     Python 1.21x faster

════ BENCHMARK SUMMARY ═══
Total Benchmarks: 12
Neutron Faster:   10
Python Faster:    2

Neutron Win Rate: 83.3%

🎉 All benchmarks completed successfully! 🎉
```

## Performance Characteristics

### Neutron Strengths (83.3% win rate)
- **Recursive/Iterative algorithms** (e.g., Fibonacci, Deep Recursion) - significantly faster due to optimized call stack
- **Numeric computations** (e.g., Matrix operations, Prime numbers, Sorting) - 2-5x faster
- **Math operations** (e.g., Power, GCD, Square root) - 4x faster  
- **String operations** - Now faster than Python with proper optimizations
- **Data structures** (e.g., List and Dictionary operations) - 2x faster on average
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
│   ├── ackermann.nt
│   ├── sorting.nt
│   ├── list_ops.nt
│   ├── dict_ops.nt
│   ├── math.nt
│   ├── strings.nt
│   ├── string_ops.nt
│   ├── loops.nt
│   └── nested_loops.nt
├── python/            # Python implementations
│   ├── fibonacci.py
│   ├── primes.py
│   ├── matrix.py
│   ├── ackermann.py
│   ├── sorting.py
│   ├── list_ops.py
│   ├── dict_ops.py
│   ├── math.py
│   ├── strings.py
│   ├── string_ops.py
│   ├── loops.py
│   └── nested_loops.py
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