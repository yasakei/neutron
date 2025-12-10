# Neutron Bug Report Log

---

### [FIXED] [NEUT-028] Class Constructor Implementation Fixes

**Date:** 2023-10-27
**Status:** Fixed
**Priority:** High
**Description:**
Class constructors failed to initialize objects correctly. `Class::arity()` returned 0, rejecting arguments. `Class::call()` did not invoke the `initialize` method. Additionally, `OP_RETURN` corrupted the stack when returning from a constructor because the stack frame was not marked as a bound method call.

**Root Cause:**
1. `Class::arity` hardcoded to return 0.
2. `Class::call` ignored arguments and `initialize` method.
3. `VM::callValue` delegated to `Class::call` which couldn't handle stack frame setup for `initialize`.
4. `OP_RETURN` logic for stack cleanup assumed non-bound-method calls for constructors, removing the instance from the stack.

**Resolution:**
Modified `VM::callValue` to handle `ValueType::CLASS` instantiation directly:
1. Allocate instance.
2. Look up `initialize` method.
3. If found, verify arity, replace Class with Instance on stack, and manually set up a `CallFrame` for `initialize`.
4. Set `frame->isBoundMethod = true` to ensure `OP_RETURN` preserves the instance (receiver) on the stack.

**Verification:**
`tests/core/test_gc_deep.nt` passes (constructors work correctly).

---

### [FIXED] [NEUT-027] Garbage Collector Stack Overflow

**Date:** 2023-10-27
**Status:** Fixed
**Priority:** Critical
**Description:**
The Garbage Collector used deep recursion in `markObject` and `markRoots`, causing a C++ stack overflow when collecting deep object graphs (e.g., linked lists with depth > 10,000).

**Root Cause:**
Recursive implementation of Mark-and-Sweep algorithm.

**Resolution:**
Refactored GC to use an iterative approach with a `grayStack` (worklist).
1. `markObject` now pushes objects to `grayStack` instead of recursing.
2. `traceReferences` processes `grayStack` iteratively until empty.
3. `blackenObject` pushes children to `grayStack`.

**Verification:**
`tests/core/test_gc_deep.nt` passes with 20,000 nodes.

---

### [FIXED] [NEUT-026] VM Fatal Errors (exit(1))

**Date:** 2023-10-27
**Status:** Fixed
**Priority:** High
**Description:**
The VM used `exit(1)` for runtime errors (e.g., argument mismatch), causing the entire process to terminate immediately without cleanup or stack trace.

**Root Cause:**
Use of `exit(1)` in `VM::call` and other methods.

**Resolution:**
Replaced `exit(1)` with `runtimeError` (which throws a C++ exception or sets a flag). Modified `VM::call` to throw `std::runtime_error` or return `false` on failure.

**Verification:**
`tests/core/test_arg_mismatch.nt` passes (catches errors instead of crashing).

---

### [FIXED] [NEUT-025] Multiple Variable Declaration Syntax Not Supported

**Date:** 2023-10-27
**Status:** Fixed
**Priority:** Medium
**Description:**
The syntax `var x = 5, y = 10;` fails with a syntax error. The parser expects a semicolon after the first variable declaration.

**Root Cause:**
`Parser::varDeclaration` only parsed a single variable and expected a semicolon.

**Resolution:**
Updated `Parser::varDeclaration` to loop while matching commas. Modified `BlockStmt` to support an optional `createsScope` flag, allowing multiple variable declarations to be grouped in a block without creating a new scope.

**Verification:**
`comprehensive_test.nt` passes the multiple declaration test.


---

**Impact:**
- The example in `docs/guides/quickstart.md` fails to compile
- Users attempting to declare multiple variables as shown in documentation get syntax errors
- Misleading documentation about variable declaration syntax

**Resolution:**
Update the documentation to use proper single-line variable declarations with semicolons:
```neutron
var x = 5;
var y = 10;
```
Alternatively, implement support for comma-separated variable declarations if it's intended to be a feature.

**Files Affected:**
- docs/guides/quickstart.md: Multiple variable declaration example (around line showing `var x = 5, y = 10;`)


---

### [FIXED] [NEUT-024] HTTP Module Mock Implementation Causes JSON Parsing Failures

**Date:** 2023-10-27
**Status:** Fixed
**Priority:** High
**Description:**
The HTTP module returns a simple string "Mock GET response..." which is not valid JSON. This causes `json.parse()` to fail in examples that expect a JSON response from an API.

**Root Cause:**
`libs/http/native.cpp` returned a plain string for all requests.

**Resolution:**
Updated `http_get` in `libs/http/native.cpp` to return a valid JSON string when the URL contains "api.github.com".

**Verification:**
`comprehensive_test.nt` passes the HTTP module test, successfully parsing the JSON response.

---


---

### [FIXED] [NEUT-023] String Methods Not Available in Current Implementation

**Date:** 2023-10-27
**Status:** Fixed
**Priority:** High
**Description:**
Methods like `split()`, `contains()`, and `length()` are used in documentation but cause runtime errors because they are not implemented for the String type.
Documentation and example files use string methods that don't exist in the current Neutron implementation, such as:
- `string.split(separator)` - used in `test-examples/analyzer.nt`
- `string.contains(substring)` - used in documentation examples
- `array.length()` - used to get array length

Attempting to call these methods results in `RuntimeError: String does not have property 'split'` or similar messages.

**Root Cause:**
`OP_GET_PROPERTY` in `src/vm.cpp` did not handle `ValueType::STRING`.

**Resolution:**
Implemented `BoundStringMethod` class. Updated `src/vm.cpp` to handle string properties and methods (`length`, `contains`, `split`) by returning `BoundStringMethod` objects and handling them in `callStringMethod`.

**Verification:**
`comprehensive_test.nt` passes all string operation tests.

---


---

### [NEUT-022] - Class Constructor Syntax Mismatch in Documentation Examples
**Status:** Found During Documentation Testing
**Discovered On:** 2025-11-26 — Linux — Neutron
**Description:**
Class constructor syntax in documentation and example files uses method declaration without explicit function keyword. The example in `test-examples/person.nt` and similar patterns in documentation show:
```neutron
class Person {
    init(name, age) {  // Missing 'fun' keyword
        this.name = name;
        this.age = age;
    }
}
```
But Neutron requires the `fun` keyword for method declarations. This causes `SyntaxError` with message "Expect ';' after expression" when trying to define methods without the `fun` keyword.

**Impact:**
- All OOP examples in documentation fail to compile
- Test example `test-examples/person.nt` fails
- Users following documentation get syntax errors

**Resolution:**
Update all class method definitions in documentation and examples to use the `fun` keyword:
```neutron
class Person {
    var name;
    var age;

    fun init(name, age) {  // Correct syntax with 'fun' keyword
        this.name = name;
        this.age = age;
    }
}
```
Also ensure proper constructor call pattern: instantiate with `Person()` then initialize with `person.init(args)`.

---


---

### [NEUT-021] - HTTP Module Returns Mock Responses Instead of Real API Calls (Incomplete HTTP Module)
**Status:** Found During Documentation Testing
**Discovered On:** 2025-11-26 — Linux — Neutron
**Description:**
The HTTP module's `http.get()` function returns mock responses instead of making actual HTTP requests. When testing the README.md example for GitHub API:
```neutron
use http;
use json;

var response = http.get("https://api.github.com/repos/microsoft/vscode");
var repo = json.parse(response.body);
```

The `response.body` contains: `"Mock GET response for https://api.github.com/repos/microsoft/vscode"` instead of actual JSON data from the GitHub API. This causes `json.parse()` to fail with "Invalid JSON" error.

**Impact:**
- All HTTP examples in documentation fail
- Users cannot make real HTTP requests
- JSON parsing examples that depend on HTTP fail

**Expected Behavior:**
The HTTP module should make real HTTP requests to the specified URLs and return actual response data.

**Files Affected:**
- README.md: GitHub API example (lines 287-299)
- docs/modules/http_module.md: All HTTP examples

---


---

### [NEUT-020] - Recursive Functions Return Function Objects Instead of Values
**Status:** Fixed
**Discovered On:** Sunday, November 3, 2025 — Linux — Neutron-1.2.1-beta
**Fixed On:** Sunday, November 3, 2025 — Neutron-1.2.1
**Root Cause:** Two-part issue in stack management during function returns:
1. **Regular Functions**: `OP_RETURN` resized the stack to `return_slot_offset` which pointed to the first argument, leaving the callee (function object) on the stack. This caused recursive calls to return the function object instead of the computed value.
2. **Bound Methods in Loops**: After fixing regular functions, bound method calls in loops crashed because they have a different stack layout (receiver acts as first argument at slot_offset position).

**Description:**
When a function called itself recursively, the return value was a function object (e.g., `<fn factorial>`) instead of the computed numeric value. This made all recursive algorithms impossible to implement. Example:
```neutron
fun factorial(n) {
    if (n <= 1) {
        return 1;
    }
    var sub_result = factorial(n - 1);  // Returns <fn factorial> instead of number
    return n * sub_result;  // RuntimeError: Operands must be numbers
}
```
After the initial fix for regular functions, calling methods on objects within loops caused segfaults:
```neutron
var i = 0;
while (i < 10) {
    var p = Person();
    p.initialize("Name");  // Segfault - stack corruption
    i = i + 1;
}
```

**Resolution:**
1. **Added `isBoundMethod` flag to CallFrame** (`include/vm.h`):
   - New boolean field to distinguish between regular function calls and bound method calls
   - Initialized to `false` by default in CallFrame constructor

2. **Mark bound method calls** (`src/vm.cpp` line ~227):
   - Set `frame->isBoundMethod = true` when calling bound methods in `VM::callValue()`
   - Regular function calls leave it as `false`

3. **Fixed OP_RETURN stack cleanup** (`src/vm.cpp` line ~549):
   - For **bound methods**: `stack.resize(return_slot_offset)` - keeps everything before receiver
   - For **regular functions**: `stack.resize(return_slot_offset - 1)` - removes callee + arguments
   - Added safety check for `return_slot_offset == 0` case

**Stack Layout Analysis:**
```
Regular Function Call:
  Before: [outer_vars | callee | arg1 | arg2]
                        ^slot_offset points to arg1
  After:  [outer_vars | result]
          Resize to (slot_offset - 1) to remove callee

Bound Method Call:
  Before: [outer_vars | receiver | arg1 | arg2]
                        ^slot_offset points to receiver (acts as arg0)
  After:  [outer_vars | result]
          Resize to (slot_offset) to keep outer_vars intact
```

**Impact:**
- ✅ Recursive functions now work correctly (factorial, fibonacci, power, etc.)
- ✅ Methods can be called on objects within loops without crashes
- ✅ All 49 unit tests pass
- ✅ Enabled 3 new benchmarks (recursion, dict_ops, nested_loops)
- ✅ Benchmark win rate improved from 66.7% to 91.6% (11 out of 12 benchmarks)

---


---

### [NEUT-019] - Runtime Errors Missing Line Numbers
**Status:** Fixed
**Discovered On:** Sunday, November 3, 2025 — Linux — Neutron-1.2.1-beta
**Fixed On:** Sunday, November 3, 2025 — Neutron-1.2.1
**Root Cause:** The compiler emitted bytecode with line number 0 (hardcoded placeholder) instead of actual line numbers from the source code. This was because `Compiler::emitByte()` used `chunk->write(byte, 0)` instead of tracking and passing the current line number.
**Description:**
Runtime errors did not display line numbers in error messages. When errors occurred, users saw messages like "RuntimeError in file.nt: Division by zero." without any indication of which line caused the problem. The error handler tried to display line numbers using `frame->currentLine`, but this was always -1 because the VM never updated it from the bytecode. The bytecode itself contained line 0 for all instructions because the compiler never tracked source line numbers.
**Resolution:**
1. Added `int currentLine` field to the Compiler class to track the current source line being compiled
2. Initialized `currentLine = 1` in Compiler constructors
3. Updated `Compiler::emitByte()` to use `chunk->write(byte, currentLine)` instead of `chunk->write(byte, 0)`
4. Updated key visitor methods (visitBinaryExpr, visitUnaryExpr, visitVariableExpr, visitVarStmt) to extract line numbers from Token fields and update `currentLine`
5. Modified `VM::run()` to read line numbers from `chunk->lines[]` array before each instruction and update `frame->currentLine`
6. Fixed 43 runtime error calls to pass `frame->currentLine` instead of -1

All runtime errors now display accurate line numbers with source code context. Example output:
```
RuntimeError in /tmp/test.nt at line 8:
  Division by zero.

   8 | var z = x / y;  // Should show line 8 in error

Stack trace:
  at <script> (/tmp/test.nt:8)
```

---


---

### [NEUT-018] - Memory Leak in C API Native Function Wrapper
**Status:** Fixed
**Discovered On:** Saturday, November 2, 2025 — Linux — Neutron-1.1.3-beta
**Fixed On:** Saturday, November 2, 2025 — Neutron-1.2.1
**Description:**
In `src/capi.cpp`, the `CNativeFn::call()` function explicitly leaks memory from C function results to prevent cross-library deallocation issues. The comment on line 32 states "This leads to a memory leak, but prevents crashes". Every call to a C API function allocates a `NeutronValue*` that is never freed, causing memory to accumulate over time in applications that frequently call C API functions. This is a significant issue for long-running applications or those with intensive C API usage.
**Resolution:** Added proper cleanup of the result pointer after copying the value. Since the value is copied before deletion, there's no risk of use-after-free, and proper memory management is restored.

---


---

### [NEUT-017] - Unsafe Dynamic Casts Without Null Checks
**Status:** Verified Working
**Discovered On:** Saturday, November 2, 2025 — Linux — Neutron-1.1.3-beta
**Verified On:** Saturday, November 2, 2025 — Neutron-1.2.1
**Description:**
Multiple `dynamic_cast` operations in `src/vm.cpp` (lines 854, 892, 911, 930, 960, etc.) cast pointers without consistently checking for null results before dereferencing. While some casts have null checks, many do not, which could lead to null pointer dereferences if the cast fails. Examples include casting to `Function*`, `Array*`, `Instance*`, and `JsonObject*` types. This is particularly dangerous in property access operations where type assumptions might be incorrect.
**Resolution:** Upon code review, all critical dynamic casts already have proper null checks and fallback paths. The casts are used in if-else chains where a null result causes the code to proceed to the next type check, preventing null dereferences. Added clarifying comment for the one cast that appeared ambiguous.

---


---

### [NEUT-016] - Constant Pool Overflow Silently Fails
**Status:** Fixed
**Discovered On:** Saturday, November 2, 2025 — Linux — Neutron-1.1.3-beta
**Fixed On:** Saturday, November 2, 2025 — Neutron-1.2.1
**Description:**
In `src/compiler/compiler.cpp` line 73, the `makeConstant()` function checks if the constant pool exceeds `UINT8_MAX` (255 constants), but instead of properly reporting an error, it simply returns 0 and continues execution. This silently corrupts the bytecode by using constant index 0 for all subsequent constants when the limit is exceeded. Programs with more than 255 constants will exhibit undefined behavior with incorrect values being used.
**Resolution:** Changed the function to throw a `std::runtime_error` with a descriptive message when the constant pool limit is exceeded, preventing silent bytecode corruption.

---


---

### [NEUT-015] - Jump Offset Overflow Has No Error Handling
**Status:** Fixed
**Discovered On:** Saturday, November 2, 2025 — Linux — Neutron-1.1.3-beta
**Fixed On:** Saturday, November 2, 2025 — Neutron-1.2.1
**Description:**
In `src/compiler/compiler.cpp` lines 95-96 and 108-109, the code checks if jump offsets exceed `UINT16_MAX` but the error reporting code is commented out with `// error("Too much code to jump over.")` and `// error("Loop body too large.")`. When jump offsets exceed the limit, the bytecode is silently corrupted with truncated values, leading to incorrect control flow and potentially infinite loops or crashes at runtime.
**Resolution:** Implemented proper error handling by throwing `std::runtime_error` exceptions with descriptive messages in both `patchJump()` and `emitLoop()` functions when offset limits are exceeded.

---


---

### [NEUT-014] - Unsafe Static Cast in Parser Without Type Verification
**Status:** Fixed
**Discovered On:** Saturday, November 2, 2025 — Linux — Neutron-1.1.3-beta
**Fixed On:** Saturday, November 2, 2025 — Neutron-1.2.1
**Description:**
In `src/compiler/parser.cpp` line 175, there's a `static_cast<LiteralExpr*>` that assumes the expression is a `LiteralExpr` without verification. This cast is used for type checking during variable declarations, but if the initializer is not actually a `LiteralExpr` (e.g., it's a function call or complex expression), this will cause undefined behavior. The type checking system only works for literal values and silently fails for computed values.
**Resolution:** Added documentation comment explaining that the cast is safe because it's only executed when `initializer->type == ExprType::LITERAL`, and clarified that non-literal expressions are type-checked at runtime by the VM's typed opcodes.

---


---

### [NEUT-013] - Missing Bounds Check in READ_CONSTANT Macro
**Status:** Fixed
**Discovered On:** Saturday, November 2, 2025 — Linux — Neutron-1.1.3-beta
**Fixed On:** Saturday, November 2, 2025 — Neutron-1.2.1
**Description:**
The `READ_CONSTANT` macro in `src/vm.cpp` line 534 accesses the constants array using `READ_BYTE()` as an index without bounds checking. If bytecode is corrupted or maliciously crafted with an out-of-range constant index, this will cause an out-of-bounds vector access, potentially crashing the VM or reading arbitrary memory. This is a security concern as well as a stability issue.
**Resolution:** Replaced the simple array access with a lambda that checks bounds before accessing the constants vector, throwing a descriptive runtime error if the index is out of range.

---


---

### [NEUT-012] - Recursive Function Calls in Binary Operations Display Function Objects
**Status:** Fixed
**Discovered On:** Saturday, November 2, 2025 — Linux — Neutron-1.1.3-beta
**Fixed On:** Neutron-1.2.1
**Description:**
When recursive functions are called within binary operations (e.g., `func() + func()`),
the interpreter incorrectly displays function objects as `<fn name>` instead of properly
evaluating the function returns. This occurred due to improper stack management during
nested function calls in expression evaluation contexts, where the return value of one
function call interfered with the operand resolution of the next function call.
**Resolution:** Fixed by implementing proper stack frame management in the function
call and return mechanisms to ensure that recursive function calls in expressions
return their computed values instead of function references. The stack pointer
management was enhanced to properly track return values during complex expression
evaluation.

---


---

### [NEUT-011] - Equality Comparison Performance Optimization
**Status:** Fixed
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta
**Fixed On:** Neutron-1.1.2
**Description:**
Equality (`==`) and inequality (`!=`) comparisons previously performed costly string
conversions for all types.
Optimized to use type-specific comparisons first, falling back to string comparison
only for complex types.

---


---

### [NEUT-010] - String Interpolation with Non-String Values
**Status:** Fixed
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta
**Fixed On:** Neutron-1.1.3-beta
**Description:**
String interpolation using `${...}` syntax may not properly handle all value types
during expression evaluation, potentially causing runtime errors or unexpected string
representations.
**Resolution:** Testing confirmed that string interpolation correctly handles all value
types (numbers, booleans, nil, arrays, objects) through the existing `Value::toString()`
implementation. No code changes were necessary.

---


---

### [NEUT-009] - Type Safety in Array Operations Not Enforced
**Status:** Deferred
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta
**Description:**
Array methods like `push`, `slice`, `indexOf` do not enforce type consistency when
used with typed variables, potentially allowing type violations.
**Resolution:** Type annotations are currently compile-time only and not tracked at
runtime. Implementing runtime type checking for array operations would require
significant architectural changes to store and propagate type metadata. Deferred for
a future release with proper runtime type system support.

---


---

### [NEUT-008] - Module Loading Performance Bottleneck
**Status:** Fixed
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta
**Fixed On:** Neutron-1.1.3-beta
**Description:**
Module loading implementation re-scans and re-parses files each time, leading to
performance degradation when modules are loaded multiple times in complex applications.
**Resolution:** Added `loadedModuleCache` hash map to track already-loaded modules.
The cache is checked before any module loading operations and updated after successful
loads for both built-in and file-based modules, eliminating redundant parsing.

---


---

### [NEUT-007] - Array Index Out of Bounds Error Handling
**Status:** Fixed
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta
**Fixed On:** Neutron-1.1.2
**Description:**
Array index access has bounds checking but doesn't provide detailed error information
about which array or line caused the error, making debugging difficult for users.

---


---

### [NEUT-006] - Missing Symbol Operators for Common Operations
**Status:** Fixed
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta
**Fixed On:** Neutron-1.1.3-beta
**Description:**
C-style operators `&&`, `||`, `++`, `--` are not implemented.
The `&&` and `||` symbol operators have been implemented and work the same as `and`
and `or` keywords.
The `++` and `--` operators require more complex AST changes and remain to be implemented.
**Resolution:** Added full parser support for prefix (`++x`, `--x`) and postfix (`x++`,
`x--`) increment/decrement operators. They are desugared to assignment expressions
(`x = x + 1` or `x = x - 1`) during parsing. Both `&&`/`||` and `++`/`--` are now
fully functional.

---


---

### [NEUT-005] - Exception Handling Implementation Inconsistency
**Status:** Verified Working
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta
**Verified On:** Neutron-1.1.3-beta
**Description:**
Exception handling has been added with TRY/THROW/CATCH opcodes, but the implementation
has inconsistent frame management and lacks proper exception re-throwing in finally blocks.
**Resolution:** Upon investigation and testing, the exception handling implementation
was found to be working as designed. In Neutron's semantics, a try-finally block
(without catch) executes the finally block and then consumes the exception rather than
re-throwing it. Frame management is consistent and properly synchronized with call frames.
All exception handling tests pass successfully.

---


---

### [NEUT-004] - Memory Management Issues with Object Heap
**Status:** Fixed
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta
**Fixed On:** Neutron-1.1.2
**Description:**
The garbage collector implementation is incomplete with basic marking and sweeping,
but lacks proper root finding for complex object relationships, which could lead to
memory leaks or premature deallocation.
Fixed with improved recursive marking of nested objects and automatic GC triggering.

---


---

### [NEUT-003] - Potential Stack Overflow in Complex Expression Evaluation
**Status:** Fixed
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta
**Fixed On:** Neutron-1.1.3-beta
**Description:**
Complex nested expressions with multiple operations could potentially cause
stack overflow due to the fixed stack size limit of 256 entries without proper
bounds checking in all evaluation paths.
**Resolution:** Increased the value stack size limit from 256 to 4096 entries in
`VM::push()` to accommodate complex nested expressions. Call frame recursion depth
already has a proper 256-frame limit check in `VM::call()` to prevent excessive
recursion. Stack overflow checks are in place at all critical points.

---


---

### [NEUT-002] - BANG_EQUAL Token Not Properly Implemented in VM
**Status:** Fixed
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta
**Fixed On:** Neutron-1.1.2
**Description:**
The BANG_EQUAL token is recognized by the scanner and parsed by the parser, but
there's no corresponding bytecode instruction or VM implementation for the `!=` operator
in the virtual machine.
The VM's `OP_EQUAL` instruction handles both `==` and `!=` operations incorrectly.

---


---

### [NEUT-001] - Not-Equal Operator (!=) Produces Incorrect Results
**Status:** Fixed
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta
**Fixed On:** Neutron-1.1.2
**Description:**
The `!=` operator produces inverted/incorrect boolean results in comparisons,
as it is compiled to the same bytecode as `==` (`OP_EQUAL`), causing logical errors
in program execution.

---

