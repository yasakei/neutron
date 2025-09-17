# Neutron Language Benchmarks

This directory contains benchmark scripts designed to test the performance characteristics of the Neutron programming language.

## Benchmark Descriptions

### 1. Arithmetic Operations (`arithmetic.nt`)
Tests the speed of basic arithmetic operations (addition, multiplication) by performing thousands of calculations.

### 2. String Operations (`string.nt`)
Tests the speed of string concatenation and character access operations.

### 3. Variable Assignments (`variables.nt`)
Tests the speed of variable creation and assignment operations.

### 4. Function Calls (`functions.nt`)
Tests the speed of function definitions and calls.

### 5. Conditional Statements (`conditionals.nt`)
Tests the speed of if-else conditional evaluations, including nested conditions.

### 6. Time Module (`time.nt`)
Tests the performance of the time module functions:
- `time.now()` - Gets current timestamp
- `time.format()` - Formats timestamps
- `time.sleep()` - Sleeps for specified milliseconds

## Running Benchmarks

To run all benchmarks sequentially, execute:
```
./neutron benchmark/run_all.nt
```

To run individual benchmarks:
```
./neutron benchmark/arithmetic.nt
./neutron benchmark/string.nt
./neutron benchmark/variables.nt
./neutron benchmark/functions.nt
./neutron benchmark/conditionals.nt
./neutron benchmark/time.nt
```

## Interpreting Results

Since there's no built-in timing function in Neutron, benchmarks use `time.now()` to measure execution time. The output shows:

1. Duration for executing a batch of operations
2. Average time per operation
3. Comparison between different approaches when applicable

## Benchmark Categories

Each benchmark focuses on a specific aspect of the language:

- **Arithmetic**: Mathematical operations
- **String**: String manipulation and processing
- **Variables**: Variable assignment and access
- **Functions**: Function call overhead
- **Conditionals**: Control flow performance
- **Time**: Time-related functions performance

The benchmarks help identify performance bottlenecks and compare the efficiency of different approaches in Neutron.