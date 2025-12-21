# Neutron Error Handling System

## Overview

Neutron now features a comprehensive, cross-platform error handling system that provides meaningful error messages with context, suggestions, and stack traces to help developers quickly identify and fix issues.

## Features

### 1. **Categorized Error Types**
- `SyntaxError` - Parsing and grammar errors
- `RuntimeError` - Execution errors
- `TypeError` - Type mismatch errors
- `ReferenceError` - Undefined variable/function errors
- `RangeError` - Index out of bounds errors
- `ArgumentError` - Wrong number of arguments
- `DivisionError` - Division or modulo by zero
- `StackError` - Stack overflow/underflow
- `ModuleError` - Module loading errors
- `IOError` - File operation errors
- `LexicalError` - Tokenization errors

### 2. **Rich Error Information**

Each error displays:
- **Error type and category** (color-coded on supported terminals)
- **File name and location** (line and column numbers)
- **The actual source code line** where the error occurred
- **Visual indicator** (^) pointing to the error position
- **Helpful suggestion** for fixing the error
- **Stack trace** showing the call chain (for runtime errors)

### 3. **Cross-Platform Compatibility**

The error handler works seamlessly across:
- **Linux** (with ANSI color support)
- **macOS** (with ANSI color support)
- **Windows** (with ANSI escape sequence support on Windows 10+)

Color output is automatically detected and disabled on terminals that don't support it.

### 4. **Contextual Suggestions**

The error handler provides helpful suggestions based on the error type:

- **Undefined variables**: "Did you forget to declare the variable with 'var'? Or maybe you need to import a module with 'use'?"
- **Missing semicolons**: "Every statement in Neutron must end with a semicolon."
- **Type errors**: "Mathematical operations require numeric values. Use type conversion if needed."
- **Array index errors**: "Array indices must be within bounds (0 to length-1)."
- **Division by zero**: "Check that the divisor is not zero before performing division or modulo operations."
- **Stack overflow**: "This usually indicates infinite recursion. Check your recursive function calls."

## Example Error Output

### Syntax Error

```
SyntaxError in test.nt at line 12, column 15:
  Expect ';' after expression.

  12 | var x = 10 + 5
     |               ^

Suggestion: Every statement in Neutron must end with a semicolon.
```

### Runtime Error with Stack Trace

```
RuntimeError in test.nt at line 25:
  Undefined variable 'count'

  25 | var total = count + 1;
     |             ^

Suggestion: Did you forget to declare the variable with 'var'? Or maybe you need to import a module with 'use'?

Stack trace:
  at calculateTotal (test.nt:25)
  at processData (test.nt:18)
  at main (test.nt:10)
```

### Type Error

```
TypeError in test.nt at line 8:
  Expected numbers but got other type

   8 | var result = "hello" * 2;
     |              ^

Suggestion: Mathematical operations require numeric values. Use type conversion if needed.
```

### Argument Error

```
ArgumentError in test.nt at line 15:
  Function 'greet' expects 2 arguments but got 1

  15 | greet("Alice");
     |       ^

Suggestion: Check the function definition to see how many arguments it expects.
```

### Division Error

```
DivisionError in test.nt at line 20:
  Division by zero is not allowed

  20 | var result = 10 / 0;
     |                  ^

Suggestion: Check that the divisor is not zero before performing division or modulo operations.
```

## Configuration

### Enabling/Disabling Colors

```cpp
ErrorHandler::setColorEnabled(true);  // Enable colors
ErrorHandler::setColorEnabled(false); // Disable colors
```

### Enabling/Disabling Stack Traces

```cpp
ErrorHandler::setStackTraceEnabled(true);  // Show stack traces
ErrorHandler::setStackTraceEnabled(false); // Hide stack traces
```

### Setting Source File Context

```cpp
ErrorHandler::setCurrentFile("myprogram.nt");
ErrorHandler::setSourceLines(sourceCodeLines);
```

## Implementation Details

### Error Handler Class

The `ErrorHandler` class in `include/runtime/error_handler.h` provides static methods for reporting different types of errors:

```cpp
// Report a syntax error
ErrorHandler::reportSyntaxError(message, token);

// Report a runtime error with stack trace
ErrorHandler::reportRuntimeError(message, fileName, line, stackTrace);

// Report specific error types
ErrorHandler::typeError(expected, got, fileName, line);
ErrorHandler::referenceError(name, fileName, line);
ErrorHandler::rangeError(message, fileName, line);
ErrorHandler::argumentError(expected, got, functionName, fileName, line);
ErrorHandler::divisionError(fileName, line);
ErrorHandler::stackOverflowError(fileName, line);
ErrorHandler::moduleError(moduleName, reason, fileName, line);
```

### Stack Frame Tracking

The VM now tracks stack frames with file and line information for accurate error reporting:

```cpp
struct CallFrame {
    Function* function;
    uint8_t* ip;
    size_t slot_offset;
    std::string fileName;  // Source file name
    int currentLine;       // Current line number
};
```

### Integration Points

1. **Scanner (Lexer)**: Reports lexical errors with character position
2. **Parser**: Reports syntax errors with token information
3. **Compiler**: Reports semantic errors during compilation
4. **VM**: Reports runtime errors with stack traces

## Best Practices

1. **Always set the current file** when executing code from a file
2. **Provide source lines** to the error handler for better context
3. **Enable colors** for interactive development
4. **Keep stack traces enabled** during development
5. **Catch and handle** NeutronException for recoverable errors in REPL mode

## Migration Guide

### For Existing Code

Replace old error reporting:
```cpp
// Old
std::cerr << "Error: " << message << std::endl;
exit(1);

// New
ErrorHandler::fatal(message, ErrorType::RUNTIME_ERROR);
```

Replace old runtime errors:
```cpp
// Old
runtimeError("Division by zero");

// New
ErrorHandler::divisionError(fileName, line);
```

Update parser errors:
```cpp
// Old
error(token, "Expect ';' after expression.");

// New
ErrorHandler::reportSyntaxError("Expect ';' after expression.", token);
```

## Testing

Test the error handling system with:
```bash
./neutron tests/test_error_handling.nt
```

This test file contains various error scenarios to demonstrate the improved error messages.

## Exception Handling

Neutron now supports structured exception handling with `try`, `catch`, `finally`, and `throw` statements. This allows developers to handle errors gracefully and implement proper error recovery mechanisms.

### Basic Exception Handling

```neutron
try {
    // Code that might throw an exception
    var result = riskyOperation();
    say("Success: " + result);
} catch (error) {
    // Handle the exception
    say("Error caught: " + error);
}
```

### Try-Catch-Finally

```neutron
try {
    // Risky operation
    var file = openFile("data.txt");
    processFile(file);
} catch (error) {
    // Handle specific error
    say("Processing failed: " + error);
} finally {
    // Always execute cleanup
    closeFile(file);  // Ensures resource cleanup regardless of errors
}
```

### Throwing Exceptions

```neutron
fun validateInput(input) {
    if (input == nil) {
        throw "Input cannot be nil";
    }
    if (input.length < 3) {
        throw "Input too short: " + input;
    }
    return true;
}

try {
    validateInput("");
} catch (validationError) {
    say("Validation error: " + validationError);
}
```

### Exception Value Types

Neutron supports throwing any value as an exception:

```neutron
throw "Simple error message";           // String
throw 404;                             // Number
throw true;                            // Boolean  
throw [1, 2, 3];                      // Array
throw {"error": "code", "msg": "desc"}; // Object
```

### Nested Exception Handling

```neutron
try {
    try {
        riskyInnerOperation();
    } catch (innerError) {
        say("Inner error handled: " + innerError);
        // Optionally re-throw or handle differently
    }
    safeOuterOperation();
} catch (outerError) {
    say("Outer error: " + outerError);
} finally {
    cleanup();
}
```

### Exception Flow

When an exception occurs:
1. The runtime searches for the nearest `catch` block in the current scope
2. If found, execution jumps to the `catch` block with the exception value
3. The `finally` block executes regardless of whether an exception occurred
4. If no `catch` block is found, the exception propagates up the call stack
5. Unhandled exceptions cause program termination with a runtime error

## Future Enhancements

- Error recovery in parser for multiple error reporting
- Warning system for potential issues
- Custom error types for user-defined errors
- Error filtering and suppression options
- Integration with IDE language servers for inline errors
- JSON output format for tool integration
