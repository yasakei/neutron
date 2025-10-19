# Error Handling - Current Limitations

## Status: Partially Implemented

The error handling system described in `docs/error_handling/` has been implemented, but there are some current limitations in how error context is passed from the VM to the error handler.

## What Works ✅

- **Error categorization**: All error types (SyntaxError, RuntimeError, TypeError, etc.) are properly categorized
- **Color output**: Errors display with ANSI colors on supported terminals
- **Helpful suggestions**: Context-aware suggestions are provided for each error type
- **Cross-platform**: Works on Linux, macOS, and Windows

## Current Limitations ⚠️

###  1. Missing Source Line Context in Runtime Errors

**Issue**: Runtime errors currently do NOT show:
- The actual source code line where the error occurred
- The visual indicator (^) pointing to the error position
- Line numbers in some cases

**Example - Current Output:**
```
RuntimeError in test.nt:
  Undefined variable 'missing'.
```

**Expected Output (from docs):**
```
RuntimeError in test.nt at line 3:
  Undefined variable 'missing'

   3 | var x = missing;
     |         ^

Suggestion: Did you forget to declare the variable with 'var'?
```

**Root Cause**: The VM's runtime error calls don't consistently pass line numbers and context to the error handler. Many `runtimeError()` calls use a legacy signature that doesn't include the VM pointer or current line number.

### 2. Stack Traces Not Always Shown

Stack traces are built correctly but may not display line numbers accurately for all runtime errors due to missing context.

## What Needs To Be Fixed

To fully implement the documented error handling:

1. **Update VM error calls**: Change all `runtimeError(message)` calls to `runtimeError(this, message, currentLine)`
2. **Pass source lines**: Ensure `ErrorHandler::setSourceLines()` is called before execution
3. **Track line numbers**: VM needs to consistently track and update `frames.back().currentLine` during execution

##Files That Need Updates

- `src/vm.cpp`: ~50+ `runtimeError()` calls need context parameters
- VM execution loop: Line tracking for all bytecode operations

## Workaround for Users

The error messages themselves are accurate and helpful, they just lack visual context. To debug:

1. Note the error message and type
2. Check the file name in the error output
3. Look for the mentioned variable/function/operation in your code
4. Use the suggestion provided

## For Contributors

If you want to help fix this:

1. Start with high-priority errors (undefined variables, type errors)
2. Update the signature: `runtimeError(this, message, frames.empty() ? -1 : frames.back().currentLine)`
3. Ensure `frames.back().currentLine` is updated in the VM execution loop
4. Test with `tests/test_error_handling.nt`

## Syntax Errors Work Correctly ✅

Note that **syntax errors** (parser/scanner errors) DO show source lines and visual indicators correctly, because the parser has direct access to token positions.

**Example:**
```
SyntaxError in test.nt at line 3:
  Expect ';' after variable declaration.

Suggestion: Every statement in Neutron must end with a semicolon.
```

---

**Last Updated**: 2025-10-19  
**Status**: Tracked in GitHub Issues (if applicable)
