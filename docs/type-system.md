# Neutron Type System Documentation

**Version:** 1.0.3-alpha  
**Last Updated:** October 7, 2025

---

## Table of Contents

1. [Overview](#overview)
2. [Type Architecture](#type-architecture)
3. [Core Value Types](#core-value-types)
4. [Type Hierarchy](#type-hierarchy)
5. [Type Conversions](#type-conversions)
6. [Memory Management](#memory-management)
7. [Type Annotations (Optional)](#type-annotations-optional)
8. [Implementation Details](#implementation-details)
9. [Best Practices](#best-practices)

---

## Overview

Neutron uses a **dynamically typed** system with **runtime type checking**. Variables do not have fixed types and can be reassigned to values of different types. The type system is implemented using C++ `std::variant` for efficient storage and type safety at the implementation level.

### Key Characteristics

- **Dynamic Typing**: Variables can hold values of any type
- **Runtime Type Checking**: Type errors are caught during execution
- **Automatic Type Coercion**: Limited implicit conversions (mainly string concatenation)
- **Tagged Union Implementation**: Using C++17 `std::variant` for type-safe storage
- **Garbage Collection**: Automatic memory management for heap-allocated types

---

## Type Architecture

### Value Structure

The core `Value` type is defined in `include/types/value.h`:

```cpp
struct Value {
    ValueType type;          // Type tag
    Literal as;              // Tagged union of all possible value types
    
    std::string toString() const;
};
```

### ValueType Enum

```cpp
enum class ValueType {
    NIL,        // Represents absence of value
    BOOLEAN,    // true or false
    NUMBER,     // 64-bit floating point (double)
    STRING,     // UTF-8 string
    ARRAY,      // Dynamic array of values
    OBJECT,     // JSON-style object or user-defined object
    CALLABLE,   // Function or method
    MODULE,     // Imported module
    CLASS,      // Class definition
    INSTANCE    // Class instance
};
```

### Literal Type

```cpp
using Literal = std::variant<
    std::nullptr_t,  // NIL
    bool,            // BOOLEAN
    double,          // NUMBER
    std::string,     // STRING
    Array*,          // ARRAY (heap-allocated)
    Object*,         // OBJECT (heap-allocated)
    Callable*,       // CALLABLE (heap-allocated)
    Module*,         // MODULE (heap-allocated)
    Class*,          // CLASS (heap-allocated)
    Instance*        // INSTANCE (heap-allocated)
>;
```

---

## Core Value Types

### 1. NIL

**Description**: Represents the absence of a value or an uninitialized variable.

**Neutron Syntax**:
```neutron
var x;              // x is nil
var y = nil;        // explicit nil assignment
```

**Internal Representation**:
- Type: `ValueType::NIL`
- Storage: `std::nullptr_t`
- Memory: Stack-allocated (no heap overhead)

**toString()**: Returns `"nil"`

**Truthiness**: `false`

---

### 2. BOOLEAN

**Description**: True or false value.

**Neutron Syntax**:
```neutron
var isTrue = true;
var isFalse = false;
```

**Internal Representation**:
- Type: `ValueType::BOOLEAN`
- Storage: `bool` (1 byte)
- Memory: Stack-allocated

**toString()**:
- `true` → `"true"`
- `false` → `"false"`

**Truthiness**:
- `true` → `true`
- `false` → `false`

---

### 3. NUMBER

**Description**: 64-bit floating-point number (IEEE 754 double precision).

**Range**:
- Approximately ±1.7 × 10^308
- Precision: ~15-17 decimal digits

**Neutron Syntax**:
```neutron
var integer = 42;
var negative = -10;
var float = 3.14159;
var scientific = 1.23e-4;
```

**Internal Representation**:
- Type: `ValueType::NUMBER`
- Storage: `double` (8 bytes)
- Memory: Stack-allocated

**toString()**: Uses `%.15g` format (removes trailing zeros)
- `42.0` → `"42"`
- `3.14159` → `"3.14159"`

**Truthiness**: `true` for all non-zero numbers, `false` for `0` and `0.0`

**Operations**:
- Arithmetic: `+`, `-`, `*`, `/`, `%` (modulo)
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Unary: `-` (negation)

**Note**: Neutron does not distinguish between integers and floats at the type level. All numbers are stored as `double`.

---

### 4. STRING

**Description**: UTF-8 encoded text string.

**Neutron Syntax**:
```neutron
var name = "Alice";
var message = 'Hello, world!';
var escaped = "Line 1\nLine 2\tTabbed";
```

**Internal Representation**:
- Type: `ValueType::STRING`
- Storage: `std::string`
- Memory: Heap-allocated by std::string (automatic)
- Encoding: UTF-8

**toString()**: Returns the string itself

**Truthiness**: All non-empty strings are `true`, empty string `""` is `false`

**Operations**:
- Concatenation: `+` operator
- Interpolation: Using string interpolation syntax (if supported)
- Comparison: `==`, `!=` (lexicographic)

**Special Characters**:
- `\n` - Newline
- `\t` - Tab
- `\"` - Double quote
- `\'` - Single quote
- `\\` - Backslash

---

### 5. ARRAY

**Description**: Dynamically-sized ordered collection of values (heterogeneous).

**Neutron Syntax**:
```neutron
var empty = [];
var numbers = [1, 2, 3, 4, 5];
var mixed = [42, "hello", true, [1, 2]];  // Can contain different types
```

**Internal Representation**:
- Type: `ValueType::ARRAY`
- Storage: `Array*` (pointer to heap-allocated `Array` object)
- Underlying: `std::vector<Value>`
- Memory: Heap-allocated, garbage collected

**C++ Definition** (`include/types/array.h`):
```cpp
class Array : public Object {
public:
    std::vector<Value> elements;
    
    size_t size() const;
    void push(const Value& value);
    Value pop();
    Value& at(size_t index);
    void set(size_t index, const Value& value);
};
```

**toString()**: Returns string representation like `"[1, 2, 3]"`

**Truthiness**: All arrays are `true` (even empty arrays)

**Operations**:
- Indexing: `arr[0]`, `arr[i]`
- Assignment: `arr[0] = value`
- Methods: `.push()`, `.pop()`, `.slice()`, `.map()`, `.filter()`, `.find()`, `.indexOf()`, `.join()`, `.reverse()`, `.sort()`
- Property: `.length`

> ⚠️ **Common Mistake**: `.length` is a **property**, not a method. Use `arr.length`, NOT `arr.length()`. See [Common Pitfalls Guide](guides/common-pitfalls.md) for more details.

**Example**:
```neutron
var arr = [1, 2, 3];
say(arr.length);        // 3
arr.push(4);            // [1, 2, 3, 4]
var last = arr.pop();   // 4
say(arr[0]);            // 1
```

---

### 6. OBJECT

**Description**: Key-value pairs (like JSON objects). Keys are always strings.

**Neutron Syntax**:
```neutron
var person = {
    "name": "Alice",
    "age": 30,
    "active": true
};

var nested = {
    "user": {
        "id": 123,
        "email": "test@example.com"
    }
};
```

**Internal Representation**:
- Type: `ValueType::OBJECT`
- Storage: `Object*` (pointer to heap-allocated object)
- Can be `JsonObject`, custom class instances, or other object types
- Memory: Heap-allocated, garbage collected

**toString()**: Returns object representation (implementation-dependent)

**Truthiness**: All objects are `true`

**Operations**:
- Property access: `obj["key"]` or `obj.key` (dot notation)
- Property assignment: `obj["key"] = value` or `obj.key = value`

**Note**: Objects created with `{}` literal syntax are `JsonObject` instances.

---

### 7. CALLABLE

**Description**: Functions (user-defined or native).

**Types of Callables**:
1. **User Functions**: Defined with `fun` keyword
2. **Native Functions**: Built-in functions implemented in C++
3. **Lambda Functions**: Anonymous function expressions
4. **Bound Methods**: Instance methods bound to an object

**Neutron Syntax**:
```neutron
// User-defined function
fun add(a, b) {
    return a + b;
}

// Lambda function
var square = fun(x) { return x * x; };

// Native function (built-in)
say("Hello");  // 'say' is a native function
```

**Internal Representation**:
- Type: `ValueType::CALLABLE`
- Storage: `Callable*` (pointer to callable object)
- Interface: `virtual Value call(VM& vm, std::vector<Value> arguments)`
- Memory: Heap-allocated, garbage collected

**C++ Interface** (`include/types/callable.h`):
```cpp
class Callable {
public:
    virtual int arity() = 0;  // Number of parameters (-1 for variadic)
    virtual Value call(VM& vm, std::vector<Value> arguments) = 0;
    virtual std::string toString() = 0;
};
```

**toString()**: Returns `"<fn functionName>"` or `"<native fn>"`

**Truthiness**: All callables are `true`

---

### 8. MODULE

**Description**: Imported module providing functions and values.

**Neutron Syntax**:
```neutron
use math;
use json;
use sys;

say(math.pi);          // Access module members
var result = math.sqrt(16);
```

**Internal Representation**:
- Type: `ValueType::MODULE`
- Storage: `Module*` (pointer to module object)
- Contains: Functions, constants, and other exported values
- Memory: Heap-allocated, garbage collected

**Built-in Modules**:
- `sys`: System operations, file I/O
- `math`: Mathematical functions
- `json`: JSON parsing/serialization
- `http`: HTTP client
- `time`: Time and date operations
- `fmt`: Type conversion utilities

**toString()**: Returns `"<module>"`

**Truthiness**: All modules are `true`

---

### 9. CLASS

**Description**: Class definition (blueprint for creating instances).

**Neutron Syntax**:
```neutron
class Person {
    fun init(name, age) {
        this.name = name;
        this.age = age;
    }
    
    fun greet() {
        say("Hello, I'm " + this.name);
    }
}
```

**Internal Representation**:
- Type: `ValueType::CLASS`
- Storage: `Class*` (pointer to class object)
- Contains: Class name, methods, environment
- Memory: Heap-allocated, garbage collected

**C++ Definition** (`include/types/class.h`):
```cpp
class Class : public Callable {
public:
    std::string name;
    std::shared_ptr<Environment> class_env;
    std::unordered_map<std::string, Value> methods;
};
```

**toString()**: Returns `"<class ClassName>"`

**Truthiness**: All classes are `true`

---

### 10. INSTANCE

**Description**: Instance of a class (object with methods and fields).

**Neutron Syntax**:
```neutron
var alice = Person("Alice", 30);
alice.greet();           // Call method
say(alice.name);         // Access field
alice.age = 31;          // Modify field
```

**Internal Representation**:
- Type: `ValueType::INSTANCE`
- Storage: `Instance*` (pointer to instance object)
- Contains: Reference to class, field storage
- Memory: Heap-allocated, garbage collected

**C++ Definition** (`include/types/instance.h`):
```cpp
class Instance : public Object {
public:
    Class* klass;
    std::unordered_map<std::string, Value> fields;
};
```

**toString()**: Returns `"<ClassName instance>"`

**Truthiness**: All instances are `true`

**Operations**:
- Field access: `instance.fieldName`
- Field assignment: `instance.fieldName = value`
- Method call: `instance.methodName(args)`

---

## Type Hierarchy

```
Value (tagged union)
├── NIL (stack)
├── BOOLEAN (stack)
├── NUMBER (stack)
├── STRING (heap, std::string managed)
└── Heap-Allocated Types (garbage collected)
    ├── ARRAY (Array*)
    │   └── extends Object
    ├── OBJECT (Object*)
    │   ├── JsonObject
    │   ├── BoundArrayMethod
    │   └── Other object types
    ├── CALLABLE (Callable*)
    │   ├── Function (user-defined)
    │   ├── NativeFn (built-in)
    │   ├── BoundMethod
    │   └── Lambda
    ├── MODULE (Module*)
    ├── CLASS (Class*)
    │   └── extends Callable
    └── INSTANCE (Instance*)
        └── extends Object
```

**Base Classes**:
- `Object`: Base for all object types (provides `mark()` for GC)
- `Callable`: Interface for all callable types (functions, methods, classes)

---

## Type Conversions

### Implicit Conversions

#### 1. String Concatenation (Automatic toString())

When using `+` operator with strings, other types are automatically converted:

```neutron
var message = "Value: " + 42;          // "Value: 42"
var greeting = "Count: " + true;       // "Count: true"
var info = "Array: " + [1, 2, 3];      // "Array: [1, 2, 3]"
```

**Rule**: If either operand is a string, the other is converted to string via `toString()`.

#### 2. Boolean Context (Truthiness)

Values are implicitly converted to boolean in conditional contexts:

```neutron
if (value) { ... }
while (condition) { ... }
var result = expr1 and expr2;
```

**Truthiness Rules**:
| Type | Truthy | Falsy |
|------|--------|-------|
| NIL | - | `nil` |
| BOOLEAN | `true` | `false` |
| NUMBER | All except 0 | `0`, `0.0` |
| STRING | Non-empty | `""` |
| ARRAY | All (even empty) | - |
| OBJECT | All | - |
| CALLABLE | All | - |
| MODULE | All | - |
| CLASS | All | - |
| INSTANCE | All | - |

### Explicit Conversions

Neutron provides module functions for explicit type conversion:

```neutron
use fmt;

var str = fmt.to_str(42);      // "42"
var num = fmt.to_float("3.14");  // 3.14
var bool = fmt.to_int(1);        // 1 (the fmt module handles type conversion differently)
```

### Type Checking at Runtime

Check type using comparison or built-in functions (if available):

```neutron
var x = 42;
// Type can change dynamically
x = "hello";
```

---

## Memory Management

### Stack-Allocated Types

**Types**: `NIL`, `BOOLEAN`, `NUMBER`

- Stored directly in the `Value` struct
- No heap allocation
- Automatic cleanup (RAII)
- Copy semantics

### Heap-Allocated Types

**Types**: `STRING` (via std::string), `ARRAY`, `OBJECT`, `CALLABLE`, `MODULE`, `CLASS`, `INSTANCE`

- Stored as pointers in the `Value` struct
- Allocated on the heap
- Managed by garbage collector
- Reference semantics

### Garbage Collection

**Algorithm**: Mark-and-sweep garbage collection

**Process**:
1. **Mark Phase**: Starting from roots (stack, globals), mark all reachable objects
2. **Sweep Phase**: Delete all unmarked objects

**Triggering**: Automatic when heap exceeds threshold (starts at 1024 bytes, doubles after each collection)

**Implementation** (`src/vm.cpp`):
```cpp
void VM::collectGarbage() {
    markRoots();  // Mark reachable objects
    sweep();      // Delete unreachable objects
    nextGC = heap.size() * 2;  // Adjust threshold
}
```

**Mark Method** (in `Object` base class):
```cpp
class Object {
    bool is_marked = false;
    virtual void mark() { is_marked = true; }
};
```

**Note**: Cyclic references are handled correctly by the mark-and-sweep algorithm.

---

## Type Annotations (Optional)

Neutron supports **optional type annotations** for documentation and potential future static analysis, but they are **not enforced at runtime** in the current implementation.

### Syntax

```neutron
var int x = 42;
var string name = "Alice";
var bool isActive = true;
var array numbers = [1, 2, 3];
var object person = {"name": "Bob"};
var any value = "anything";  // Accepts any type
```

### Available Type Keywords

- `int`: Integer number
- `float`: Floating-point number
- `string`: Text string
- `bool`: Boolean value
- `array`: Array type
- `object`: Object type
- `any`: Any type (no restriction)

### Implementation Status

Type annotations are **parsed** but **not enforced** at runtime in v1.0.3-alpha. The tokens exist (`TYPE_INT`, `TYPE_FLOAT`, etc.) and the parser recognizes them, but the VM does not perform runtime type checking based on annotations.

**Future Plans**: May be used for:
- Optional static type checking
- IDE autocomplete and type hints
- Optimization hints for the compiler

---

## Implementation Details

### Value Construction

```cpp
// C++ constructors for creating Value objects
Value::Value()                    // NIL
Value::Value(bool value)          // BOOLEAN
Value::Value(double value)        // NUMBER
Value::Value(const std::string&)  // STRING
Value::Value(Array* array)        // ARRAY
Value::Value(Object* object)      // OBJECT
Value::Value(Callable* callable)  // CALLABLE
Value::Value(Module* module)      // MODULE
Value::Value(Class* klass)        // CLASS
Value::Value(Instance* instance)  // INSTANCE
```

### Type Checking (C++)

```cpp
if (value.type == ValueType::NUMBER) {
    double num = std::get<double>(value.as);
    // Use num...
}
```

### toString() Implementation

Located in `src/types/value.cpp`:

```cpp
std::string Value::toString() const {
    switch (type) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOLEAN: return std::get<bool>(as) ? "true" : "false";
        case ValueType::NUMBER: /* format double with %.15g */
        case ValueType::STRING: return std::get<std::string>(as);
        case ValueType::ARRAY: return std::get<Array*>(as)->toString();
        // ... etc
    }
}
```

### Variant Access Pattern

```cpp
// Safe access using std::get
try {
    double num = std::get<double>(value.as);
} catch (const std::bad_variant_access& e) {
    // Type mismatch
}

// Or use std::holds_alternative for checking
if (std::holds_alternative<double>(value.as)) {
    double num = std::get<double>(value.as);
}
```

---

## Best Practices

### 1. Initialize Variables

```neutron
// Good
var x = 0;
var name = "";

// Avoid (x is nil until assigned)
var x;
```

### 2. Use Consistent Types in Collections

```neutron
// Preferred - homogeneous array
var numbers = [1, 2, 3, 4, 5];

// Works but less maintainable
var mixed = [1, "two", true, [4]];
```

### 3. Check for nil Before Use

```neutron
fun processValue(val) {
    if (val != nil) {
        // Use val safely
        say(val);
    }
}
```

### 4. Use Type Annotations for Clarity

```neutron
// More readable and self-documenting
var int count = 0;
var string username = "alice";
```

### 5. Avoid Excessive Type Changes

```neutron
// Less clear
var x = 42;
x = "hello";  // Type changed - can be confusing
x = true;     // Type changed again

// Better - use different variables
var count = 42;
var message = "hello";
var isActive = true;
```

### 6. Leverage Truthiness Carefully

```neutron
// Good use of truthiness
if (array.length) {
    // Array not empty
}

// More explicit and readable
if (array.length > 0) {
    // Array not empty
}
```

### 7. Handle Garbage Collection Wisely

```neutron
// Objects are GC'd automatically
fun createLargeArray() {
    var large = [];
    // ... populate array
    return large;  // Returned value is reachable, won't be collected
}

// Local objects are collected when unreachable
fun temporary() {
    var temp = [1, 2, 3];  // Collected after function returns
}
```

---

## Performance Considerations

### Type Tag Overhead

- Each `Value` stores a type tag (`ValueType` enum)
- Size: 1 byte for enum + padding
- Minimal overhead for type checking

### Variant Storage

- `std::variant` size is the size of the largest type (8 bytes for pointer or double)
- Total `Value` size: ~16 bytes (8 for variant + 1 for enum + padding)

### Heap Allocation Costs

- Heap types (arrays, objects, etc.) require dynamic allocation
- Garbage collection pause time proportional to heap size
- GC threshold management minimizes collection frequency

### String Operations

- String concatenation creates new string objects
- Repeated concatenations can be inefficient
- Consider accumulating in an array and joining once

```neutron
// Less efficient
var result = "";
for (var i = 0; i < 1000; i = i + 1) {
    result = result + i;  // Creates 1000 string objects
}

// More efficient
var parts = [];
for (var i = 0; i < 1000; i = i + 1) {
    parts.push(i);
}
var result = parts.join("");  // Single concatenation
```

---

## Summary

Neutron's type system provides:

- ✅ **10 core value types** covering all common data structures
- ✅ **Dynamic typing** with runtime type checking
- ✅ **Efficient tagged union** implementation using C++ std::variant
- ✅ **Automatic memory management** via garbage collection
- ✅ **Type safety** at the C++ implementation level
- ✅ **Flexible type conversions** (implicit for strings, explicit via modules)
- ✅ **Optional type annotations** for future static analysis
- ✅ **Zero-cost abstractions** for stack-allocated types
- ✅ **First-class functions** and objects

The type system balances **ease of use** (dynamic typing) with **implementation efficiency** (tagged unions, selective heap allocation) and **safety** (runtime type checking, garbage collection).
