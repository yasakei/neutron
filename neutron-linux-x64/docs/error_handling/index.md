# Error Handling Documentation

This directory contains comprehensive documentation for Neutron's error handling system.

## Files

### 📘 [ERROR_HANDLING.md](ERROR_HANDLING.md)
Complete technical documentation including:
- Error types and categories
- Configuration options
- API reference
- Implementation details
- Integration guide
- Best practices
- Migration guide

### 🚀 [ERROR_HANDLING_QUICK_REF.md](ERROR_HANDLING_QUICK_REF.md)
Quick reference guide with:
- Overview of the error handling system
- Error type table
- Quick examples
- Building instructions
- Platform support

### ✨ [ERROR_HANDLING_FEATURE.md](ERROR_HANDLING_FEATURE.md)
Feature announcement with:
- Before/after comparisons
- Key features
- Quick examples
- Testing instructions

### 🔧 [IMPLEMENTATION_ERROR_HANDLING.md](IMPLEMENTATION_ERROR_HANDLING.md)
Implementation summary including:
- Technical details
- Files created and modified
- Build status
- Testing results
- Future enhancements

## Quick Start

To see the error handling system in action:

```bash
# Test with an undefined variable
echo 'say(unknownVar);' > test.nt
./build/neutron test.nt

# Test with a syntax error
echo 'var x = 10' > test.nt
./build/neutron test.nt
```

## Error Types

The system provides 11 categorized error types:

| Type | Description |
|------|-------------|
| `SyntaxError` | Parse/grammar errors |
| `RuntimeError` | Execution errors |
| `TypeError` | Type mismatches |
| `ReferenceError` | Undefined references |
| `RangeError` | Index out of bounds |
| `ArgumentError` | Wrong argument count |
| `DivisionError` | Division by zero |
| `StackError` | Stack overflow |
| `ModuleError` | Module loading issues |
| `IOError` | File operations |
| `LexicalError` | Tokenization errors |

## Example Output

```
ReferenceError in program.nt at line 15:
  Undefined variable 'count'

  15 | var total = count + 1;
     |             ^

Suggestion: Did you forget to declare the variable with 'var'? 
            Or maybe you need to import a module with 'use'?
```

## Testing

Run the comprehensive test suite:

```bash
./build/neutron tests/test_error_handling.nt
```

This test file demonstrates various error scenarios including syntax errors, undefined variables, type errors, division by zero, and more.
