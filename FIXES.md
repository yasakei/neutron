# Neutron Bug Report Log

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

### [NEUT-007] - Array Index Out of Bounds Error Handling
**Status:** Fixed  
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta  
**Fixed On:** Neutron-1.1.2  
**Description:**  
Array index access has bounds checking but doesn't provide detailed error information 
about which array or line caused the error, making debugging difficult for users.

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

### [NEUT-005] - Exception Handling Implementation Inconsistency
**Status:** Fixed  
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta  
**Fixed On:** Neutron-1.1.3-beta  
**Description:**  
Exception handling has been added with TRY/THROW/CATCH opcodes, but the implementation 
has inconsistent frame management and lacks proper exception re-throwing in finally blocks.  
**Resolution:** Fixed `OP_END_TRY` bytecode handler to properly re-throw pending 
exceptions after finally blocks execute. When an exception occurs in a try-finally 
block (without catch), the exception is now stored, the finally block executes, and 
then the exception is re-thrown to propagate up the call stack, maintaining proper 
exception semantics.

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

### [NEUT-001] - Not-Equal Operator (!=) Produces Incorrect Results
**Status:** Fixed  
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta  
**Fixed On:** Neutron-1.1.2  
**Description:**  
The `!=` operator produces inverted/incorrect boolean results in comparisons, 
as it is compiled to the same bytecode as `==` (`OP_EQUAL`), causing logical errors 
in program execution.
