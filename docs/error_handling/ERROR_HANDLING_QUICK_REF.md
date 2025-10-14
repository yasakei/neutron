# Neutron Error Handling System - Quick Reference

## New in Neutron: Comprehensive Error Reporting

Neutron now features a professional-grade error handling system that provides:

✅ **Clear, colorful error messages** (cross-platform)
✅ **Exact source code location** with line and column numbers
✅ **Visual error indicators** pointing to the problem
✅ **Helpful suggestions** for fixing errors
✅ **Full stack traces** for runtime errors
✅ **11 categorized error types** for specific issues

## Quick Example

### Before:
```
Runtime error: Undefined variable 'x'.
```

### Now:
```
ReferenceError in program.nt at line 15:
  Undefined variable 'x'

  15 | var total = x + 10;
     |             ^

Suggestion: Did you forget to declare the variable with 'var'? 
            Or maybe you need to import a module with 'use'?

Stack trace:
  at calculateTotal (program.nt:15)
  at main (program.nt:8)
```

## Error Types

| Type | Description | Example |
|------|-------------|---------|
| `SyntaxError` | Parse/grammar errors | Missing semicolon, unmatched braces |
| `RuntimeError` | Execution errors | General runtime failures |
| `TypeError` | Type mismatches | String * Number |
| `ReferenceError` | Undefined references | Undefined variable/function |
| `RangeError` | Index out of bounds | Array index errors |
| `ArgumentError` | Wrong argument count | Function called with wrong args |
| `DivisionError` | Division by zero | `10 / 0` |
| `StackError` | Stack overflow | Infinite recursion |
| `ModuleError` | Module loading issues | Module not found |
| `IOError` | File operations | Cannot open file |
| `LexicalError` | Tokenization errors | Invalid characters |

## Building with Error Handling

The error handling system is automatically included when you build Neutron:

```bash
# Build Neutron
cmake -B build
cmake --build build

# Test error handling
./build/neutron tests/test_error_handling.nt
```

## Documentation

See [ERROR_HANDLING.md](ERROR_HANDLING.md) for complete documentation including:
- Configuration options
- API reference
- Integration guide
- Best practices
- Migration guide

## Platform Support

- ✅ Linux (with ANSI colors)
- ✅ macOS (with ANSI colors)
- ✅ Windows 10+ (with ANSI escape sequences)

Colors are automatically disabled on terminals that don't support them.

## Testing

Try the error handling test suite:

```bash
# This will demonstrate various error scenarios
./build/neutron tests/test_error_handling.nt
```

The test file includes examples of:
- Syntax errors
- Undefined variables
- Type errors
- Division by zero
- Array bounds errors
- Argument count errors
- Stack overflow detection

## API Usage

### In C++ Code

```cpp
#include "runtime/error_handler.h"

// Report a runtime error
ErrorHandler::reportRuntimeError("Something went wrong", fileName, line);

// Report a type error
ErrorHandler::typeError("number", "string", fileName, line);

// Report with stack trace
std::vector<StackFrame> trace;
trace.emplace_back("myFunction", "file.nt", 10);
ErrorHandler::reportRuntimeError("Error message", fileName, line, trace);
```

### Configuration

```cpp
// Enable/disable features
ErrorHandler::setColorEnabled(true);
ErrorHandler::setStackTraceEnabled(true);

// Set context for error reporting
ErrorHandler::setCurrentFile("myprogram.nt");
ErrorHandler::setSourceLines(sourceCodeLines);
```

## Benefits

1. **Faster Debugging**: Quickly locate and understand errors
2. **Better Learning**: Helpful suggestions guide new users
3. **Professional Output**: Clean, formatted error messages
4. **Cross-Platform**: Works consistently across all platforms
5. **Stack Traces**: Understand the call chain when errors occur
6. **Type Safety**: Catch type errors early with clear messages

## Examples

### Syntax Error
```
SyntaxError in test.nt at line 5:
  Expect ';' after expression

   5 | var x = 10
     |           ^

Suggestion: Every statement in Neutron must end with a semicolon.
```

### Argument Error
```
ArgumentError in test.nt at line 12:
  Function 'add' expects 2 arguments but got 1

  12 | var result = add(5);
     |                  ^

Suggestion: Check the function definition to see how many arguments it expects.
```

### Division Error
```
DivisionError in test.nt at line 8:
  Division by zero is not allowed

   8 | var result = count / (total - total);
     |                     ^

Suggestion: Check that the divisor is not zero before performing division.
```

---

**Note**: This error handling system is designed with cross-platform compatibility in mind and follows best practices for developer-friendly error reporting.
