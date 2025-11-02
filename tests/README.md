# Neutron Test Suite v2.0

## Overview
The Neutron test suite is now organized into logical categories for better maintainability and easier issue identification.

## Directory Structure

```
tests/
├── fixes/          # Tests for specific bug fixes (NEUT-001 to NEUT-018)
├── core/           # Core language features (variables, types, comments, etc.)
├── operators/      # Operator tests (equality, modulo, etc.)
├── control-flow/   # Control flow tests (if/else, loops, match, etc.)
├── functions/      # Function and lambda tests
├── classes/        # Class and OOP tests
└── modules/        # Module system tests (arrays, async, http, math, sys, time)
```

## Running Tests

### Run all tests:
```bash
./run_tests.sh
```

### Run with verbose output:
```bash
./run_tests.sh -v
# or
./run_tests.sh --verbose
```

## Test Naming Convention

- **Bug fix tests**: `neut-XXX.nt` (e.g., `neut-001.nt`, `neut-012.nt`)
- **Feature tests**: `test_<feature>.nt` (e.g., `test_arrays_module.nt`)

## Test Categories

### 1. Fixes (`tests/fixes/`)
Tests for specific bug fixes documented in FIXES.md:
- `neut-001.nt` - Not-Equal Operator
- `neut-011.nt` - Equality Comparison
- `neut-012.nt` - Recursive Function Calls
- `neut-013.nt` - Constant Pool Bounds Checking
- `neut-014.nt` - Type Annotations
- `neut-015.nt` - Jump Offset Handling
- `neut-016.nt` - Constant Pool Handling
- `neut-017.nt` - Safe Type Casting
- `neut-018.nt` - C API Memory Management

### 2. Core (`tests/core/`)
Core language functionality tests:
- Variables and data types
- Comments
- String interpolation
- Type annotations
- Error handling and exceptions
- Command line arguments
- Cross-platform compatibility

### 3. Operators (`tests/operators/`)
Operator functionality tests:
- Equality and inequality
- Arithmetic operators
- Modulo operator
- Comparison operators

### 4. Control Flow (`tests/control-flow/`)
Control flow statement tests:
- If/else statements
- Elif chains
- While loops
- For loops
- Break and continue
- Match expressions

### 5. Functions (`tests/functions/`)
Function-related tests:
- Function declarations
- Lambda expressions
- Closures
- Recursion

### 6. Classes (`tests/classes/`)
Object-oriented programming tests:
- Class definitions
- Methods and properties
- Instance creation
- Object literals

### 7. Modules (`tests/modules/`)
Built-in module tests:
- Arrays module
- Async module
- HTTP module
- Math module
- Sys module
- Time module

## Adding New Tests

1. **Choose the appropriate directory** based on what you're testing
2. **Name your test file** following the convention:
   - Bug fixes: `neut-XXX.nt`
   - Features: `test_<feature_name>.nt`
3. **Write clear test output** using `say()` statements with ✓ or ✗ prefixes
4. **Run the test suite** to ensure it passes

## Example Test Structure

```neutron
say("=== Testing Feature Name ===");

// Test case 1
if (condition) {
    say("✓ Test case 1 passed");
} else {
    say("✗ Test case 1 failed");
}

// Test case 2
if (another_condition) {
    say("✓ Test case 2 passed");
} else {
    say("✗ Test case 2 failed");
}

say("=== All tests completed ===");
```

## Benefits of the New Structure

1. **Easy Navigation**: Find tests by category instead of searching through a flat list
2. **Better Organization**: Related tests are grouped together
3. **Clearer Output**: Test runner shows results by category
4. **Easier Maintenance**: Adding new tests is straightforward
5. **Quick Issue Identification**: Failed tests are clearly categorized

## Test Runner Features

- ✅ Color-coded output (green for pass, red for fail)
- ✅ Per-directory summaries
- ✅ Overall test summary
- ✅ List of failed tests at the end
- ✅ Verbose mode for detailed output
- ✅ Exit code 0 for success, 1 for failures

## Current Status

**Total Tests**: 40
- Fixes: 9 tests
- Core: 11 tests
- Operators: 3 tests
- Control Flow: 7 tests
- Functions: 2 tests
- Classes: 2 tests
- Modules: 6 tests

All tests currently passing! ✅
