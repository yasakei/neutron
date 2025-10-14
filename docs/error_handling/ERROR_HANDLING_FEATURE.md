# ✨ New Feature: Enhanced Error Handling System

Neutron now includes a comprehensive error handling system with meaningful error messages!

## What's New

### Before
```
Runtime error: Undefined variable 'x'.
```

### Now
```
RuntimeError in program.nt at line 15:
  Undefined variable 'x'

  15 | var total = x + 10;
     |             ^

Suggestion: Did you forget to declare the variable with 'var'? 
            Or maybe you need to import a module with 'use'?
```

## Features

✅ **11 Error Types**: SyntaxError, RuntimeError, TypeError, ReferenceError, RangeError, ArgumentError, DivisionError, StackError, ModuleError, IOError, LexicalError

✅ **Visual Indicators**: Points to exact error location with `^` marker

✅ **Helpful Suggestions**: Context-aware tips for fixing errors

✅ **Cross-Platform**: Works on Linux, macOS, and Windows with color support

✅ **File Context**: Shows the actual source code line where the error occurred

## Quick Examples

### Syntax Error
```
SyntaxError in test.nt at line 5:
  Expect ';' after expression

   5 | var x = 10
     |           ^

Suggestion: Every statement in Neutron must end with a semicolon.
```

### Division by Zero
```
RuntimeError in test.nt:
  Division by zero.

Suggestion: Check that the divisor is not zero before performing division.
```

### Type Error
```
RuntimeError in test.nt:
  Operands must be numbers.

Suggestion: Mathematical operations require numeric values.
```

## Documentation

- Full guide: [ERROR_HANDLING.md](ERROR_HANDLING.md)
- Quick reference: [ERROR_HANDLING_QUICK_REF.md](ERROR_HANDLING_QUICK_REF.md)

## Testing

Try it out:
```bash
# Create a test file with an error
echo 'say(undefinedVariable);' > test.nt

# Run it and see the improved error message
./build/neutron test.nt
```

You'll see a clear, helpful error message pointing to the exact problem!

---

**This feature makes Neutron more developer-friendly and easier to debug!** 🎉
