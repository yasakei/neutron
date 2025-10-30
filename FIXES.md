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
**Status:** Identified  
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta  
**Description:**  
String interpolation using `${...}` syntax may not properly handle all value types 
during expression evaluation, potentially causing runtime errors or unexpected string 
representations.

---

### [NEUT-009] - Type Safety in Array Operations Not Enforced
**Status:** Identified  
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta  
**Description:**  
Array methods like `push`, `slice`, `indexOf` do not enforce type consistency when 
used with typed variables, potentially allowing type violations.

---

### [NEUT-008] - Module Loading Performance Bottleneck
**Status:** Identified  
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta  
**Description:**  
Module loading implementation re-scans and re-parses files each time, leading to 
performance degradation when modules are loaded multiple times in complex applications.

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
**Status:** Partially Fixed  
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta  
**Description:**  
C-style operators `&&`, `||`, `++`, `--` are not implemented.  
The `&&` and `||` symbol operators have been implemented and work the same as `and` 
and `or` keywords.  
The `++` and `--` operators require more complex AST changes and remain to be implemented.

---

### [NEUT-005] - Exception Handling Implementation Inconsistency
**Status:** Identified  
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta  
**Description:**  
Exception handling has been added with TRY/THROW/CATCH opcodes, but the implementation 
has inconsistent frame management and lacks proper exception re-throwing in finally blocks.

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
**Status:** Identified  
**Discovered On:** Wednesday, October 29, 2025 — Linux — Neutron-1.1.1-beta  
**Description:**  
Complex nested expressions with multiple operations could potentially cause 
stack overflow due to the fixed stack size limit of 256 entries without proper 
bounds checking in all evaluation paths.

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
