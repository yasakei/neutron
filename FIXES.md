### [NEUT-001] - Not-Equal Operator (!=) Produces Incorrect Results
**Status:** Fixed  
**Discovered:** October 29, 2025  

**Description:**  
The `!=` operator produced inverted or incorrect boolean results, as it was compiled
to the same bytecode as `==` (`OP_EQUAL`), causing logical errors in program execution.

---

### [NEUT-002] - BANG_EQUAL Token Not Properly Implemented in VM
**Status:** Fixed  
**Discovered:** October 29, 2025  

**Description:**  
The `BANG_EQUAL` token was recognized by the scanner and parser, but there was no
corresponding bytecode instruction or VM implementation for the `!=` operator.  
The VM’s `OP_EQUAL` instruction incorrectly handled both `==` and `!=`.

---

### [NEUT-003] - Potential Stack Overflow in Complex Expression Evaluation
**Status:** Identified  
**Discovered:** October 29, 2025  

**Description:**  
Complex nested expressions with multiple operations could cause stack overflow due
to the fixed stack size limit of 256 entries and missing bounds checks during evaluation.

---

### [NEUT-004] - Memory Management Issues with Object Heap
**Status:** Fixed  
**Discovered:** October 29, 2025  

**Description:**  
The garbage collector was incomplete — it performed basic marking and sweeping but
lacked proper root discovery for nested object relationships.  
This could lead to memory leaks or premature deallocation.  
Fixed with improved recursive marking and automatic GC triggering.

---

### [NEUT-005] - Exception Handling Implementation Inconsistency
**Status:** Identified  
**Discovered:** October 29, 2025  

**Description:**  
Exception handling (`TRY` / `THROW` / `CATCH`) had inconsistent frame management and
lacked proper exception re-throwing behavior in `finally` blocks.

---

### [NEUT-006] - Missing Symbol Operators for Common Operations
**Status:** Partially Fixed  
**Discovered:** October 29, 2025  

**Description:**  
C-style operators `&&`, `||`, `++`, and `--` were missing.  
The `&&` and `||` operators are now implemented (aliasing `and` / `or`), but `++` and
`--` remain pending due to required AST modifications.

---

### [NEUT-007] - Array Index Out of Bounds Error Handling
**Status:** Fixed  
**Discovered:** October 29, 2025  

**Description:**  
Array index access had bounds checking but provided no detailed error context
(array name or line number).  
Enhanced error messages now include source details to aid debugging.

---

### [NEUT-008] - Module Loading Performance Bottleneck
**Status:** Identified  
**Discovered:** October 29, 2025  

**Description:**  
Module loading re-scanned and re-parsed files on every import, leading to
significant slowdowns when modules were reused across complex projects.

---

### [NEUT-009] - Type Safety in Array Operations Not Enforced
**Status:** Identified  
**Discovered:** October 29, 2025  

**Description:**  
Array methods such as `push`, `slice`, and `indexOf` did not enforce type consistency
when used with typed variables, potentially allowing runtime type violations.

---

### [NEUT-010] - String Interpolation with Non-String Values
**Status:** Identified  
**Discovered:** October 29, 2025  

**Description:**  
String interpolation using `${...}` did not properly handle all value types,
sometimes producing invalid or unintended string representations at runtime.

---

### [NEUT-011] - Equality Comparison Performance Optimization
**Status:** Fixed  
**Discovered:** October 29, 2025  

**Description:**  
Equality (`==`) and inequality (`!=`) comparisons previously performed costly string
conversions for all types.  
Optimized to use type-specific comparisons first, falling back to string comparison
only for complex types.
