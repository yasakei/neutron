# Fmt Module Documentation

The `fmt` module provides dynamic type conversion and type detection utilities for Neutron programs. The older `convert` module has been deprecated and replaced by this enhanced module with an expanded set of functions that perform automatic type detection before conversion.

## Usage

```neutron
use fmt;

// Type conversion functions
var intVal = fmt.to_int(42.7);        // Converts to integer: 42
var strVal = fmt.to_str(42);          // Converts to string: "42"  
var binVal = fmt.to_bin(10);          // Converts to binary: "1010"
var floatVal = fmt.to_float("3.14");  // Converts to float: 3.14

// Type detection
var type = fmt.type(42);              // Returns: "number"
```

## Type Conversion Functions

### `fmt.to_int(value)`
Dynamically converts any supported value type to an integer.

**Parameters:**
- `value` (any): Value to convert to integer

**Returns:** Number representing the integer value

**Type Support:**
- `number`: Truncates decimal part (42.7 → 42)
- `string`: Parses string to integer ("123" → 123)
- `bool`: Converts to 1 (true) or 0 (false)
- `nil`: Converts to 0
- `array`, `object`, `function`, `module`: Throws error

**Example:**
```neutron
var numResult = fmt.to_int(42.7);      // 42
var strResult = fmt.to_int("123");     // 123
var boolResult = fmt.to_int(true);     // 1
var nilResult = fmt.to_int(nil);       // 0
```

**Throws:** Runtime error when attempting to convert unsupported types (arrays, objects, functions, modules)

**Use Cases:**
- Dynamic value processing
- Input validation and sanitization
- Flexible data conversion

---

### `fmt.to_str(value)`
Dynamically converts any supported value type to a string.

**Parameters:**
- `value` (any): Value to convert to string

**Returns:** String representation of the value

**Type Support:**
- `number`: Converts to string representation (42 → "42")
- `string`: Returns unchanged
- `bool`: Converts to "true" or "false"
- `nil`: Converts to "nil"
- `array`, `object`, `function`, `module`: Throws error

**Example:**
```neutron
var numResult = fmt.to_str(42);        // "42"
var strResult = fmt.to_str("hello");   // "hello"
var boolResult = fmt.to_str(true);     // "true"
var nilResult = fmt.to_str(nil);       // "nil"
```

**Throws:** Runtime error when attempting to convert unsupported types (arrays, objects, functions, modules)

**Use Cases:**
- Dynamic string formatting
- Value serialization for display
- Type-safe string operations

---

### `fmt.to_bin(value)`
Dynamically converts any supported value type to binary string representation.

**Parameters:**
- `value` (any): Value to convert to binary

**Returns:** String containing the binary representation (without leading zeros)

**Type Support:**
- `number`: Converts integer part to binary (10 → "1010")
- `string`: First converts string to number, then to binary ("5" → "101")
- `bool`: Converts to "1" (true) or "0" (false)
- `nil`: Converts to "0"
- `array`, `object`, `function`, `module`: Throws error

**Example:**
```neutron
var numResult = fmt.to_bin(10);        // "1010"
var strResult = fmt.to_bin("5");       // "101"
var boolResult = fmt.to_bin(true);     // "1"
var nilResult = fmt.to_bin(nil);       // "0"
```

**Throws:** Runtime error when attempting to convert unsupported types or when string parsing fails

**Use Cases:**
- Binary data representation
- Bit manipulation visualization
- Number system conversions

---

### `fmt.to_float(value)`
Dynamically converts any supported value type to a floating-point number.

**Parameters:**
- `value` (any): Value to convert to float

**Returns:** Number representing the floating-point value

**Type Support:**
- `number`: Returns unchanged (maintains current precision)
- `string`: Parses string to float ("3.14" → 3.14)
- `bool`: Converts to 1.0 (true) or 0.0 (false)
- `nil`: Converts to 0.0
- `array`, `object`, `function`, `module`: Throws error

**Example:**
```neutron
var numResult = fmt.to_float(42);      // 42.0
var strResult = fmt.to_float("3.14");  // 3.14
var boolResult = fmt.to_float(true);   // 1.0
var nilResult = fmt.to_float(nil);     // 0.0
```

**Throws:** Runtime error when attempting to convert unsupported types or when string parsing fails

**Use Cases:**
- Precise decimal arithmetic
- Scientific calculations
- Financial computations requiring decimal precision

---

## Type Detection Function

### `fmt.type(value)`
Returns the runtime type of a value as a string.

**Parameters:**
- `value` (any): Value to check type of

**Returns:** String representing the type ("nil", "bool", "number", "string", "function", "object", "array", "module")

**Example:**
```neutron
var nilType = fmt.type(nil);      // "nil"
var boolType = fmt.type(true);    // "bool"
var numType = fmt.type(42);       // "number"
var strType = fmt.type("hello");  // "string"
var funType = fmt.type(say);      // "function"
```

**Use Cases:**
- Dynamic behavior based on type
- Debugging and logging
- Runtime type validation

## Error Handling

The fmt module functions throw descriptive errors when attempting to convert unsupported types:

```neutron
use fmt;

try {
    var result = fmt.to_int([1, 2, 3]);  // Throws: Cannot convert array to integer
} catch (error) {
    say("Conversion failed: " + error);
}
```

## Migration from Convert Module

The fmt module replaces the older convert module with the following changes:

- `convert.int(string)` → `fmt.to_int(value)` (dynamic conversion) - REMOVED: convert module was deprecated
- `convert.str(number)` → `fmt.to_str(value)` (dynamic conversion) - REMOVED: convert module was deprecated
- `convert.bin_to_int()` and `convert.int_to_bin()` → `fmt.to_bin(value)` (dynamic conversion) - REMOVED: convert module was deprecated
- Added `fmt.to_float()` for decimal number conversion
- Removed character manipulation functions (use sys module for direct string operations)

## Common Usage Patterns

### Flexible Data Processing
```neutron
use fmt;

fun processInput(input) {
    var inputType = fmt.type(input);
    say("Processing " + inputType + " value: " + fmt.to_str(input));
    
    // Convert to number if possible
    try {
        var num = fmt.to_int(input);
        say("As integer: " + num);
    } catch (error) {
        say("Cannot convert to integer: " + error);
    }
    
    // Convert to string for display
    try {
        var str = fmt.to_str(input);
        say("As string: " + str);
    } catch (error) {
        say("Cannot convert to string: " + error);
    }
}

processInput(42);         // Process a number
processInput("123");      // Process a string that looks like a number
processInput(true);       // Process a boolean
```

### Type-Safe Calculations
```neutron
use fmt;

fun safeCalculate(str1, str2, operation) {
    try {
        var num1 = fmt.to_float(str1);
        var num2 = fmt.to_float(str2);
        
        var result;
        if (operation == "add") {
            result = num1 + num2;
        } else if (operation == "multiply") {
            result = num1 * num2;
        } else {
            say("Unknown operation: " + operation);
            return nil;
        }
        
        return fmt.to_str(result);  // Return as string
    } catch (error) {
        say("Calculation failed: " + error);
        return nil;
    }
}

var result = safeCalculate("3.14", "2", "multiply");
say("Result: " + result);  // Output: Result: 6.28
```

### Data Validation
```neutron
use fmt;

fun validateAndConvert(value, expectedType) {
    var actualType = fmt.type(value);
    
    if (actualType == expectedType) {
        say("Value is already " + expectedType);
        return value;
    }
    
    try {
        if (expectedType == "number") {
            var converted = fmt.to_float(value);
            say("Converted " + actualType + " to number: " + converted);
            return converted;
        } else if (expectedType == "string") {
            var converted = fmt.to_str(value);
            say("Converted " + actualType + " to string: " + converted);
            return converted;
        }
        // Add more conversion cases as needed
    } catch (error) {
        say("Cannot convert " + actualType + " to " + expectedType + ": " + error);
        return nil;
    }
}

validateAndConvert("123", "number");    // Converts string to number
validateAndConvert(456, "string");      // Converts number to string
```