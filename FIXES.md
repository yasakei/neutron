---
[NEUT-001] - Not-Equal Operator (!=) Produces Incorrect Results
Status: Fixed
Discoverd on: Wednesday, October 29, 2025 - Linux - Neutron-1.1.1-beta
Description : The `!=` operator produces inverted/incorrect boolean results in comparisons, as it is compiled to the same bytecode as `==` (OP_EQUAL), causing logical errors in program execution.
---
---
[NEUT-002] - BANG_EQUAL Token Not Properly Implemented in VM
Status: Fixed
Discoverd on: Wednesday, October 29, 2025 - Linux - Neutron-1.1.1-beta
Description : The BANG_EQUAL token is recognized by scanner and parsed by the parser, but there's no corresponding bytecode instruction or VM implementation for the '!=' operator in the virtual machine. The VM's OP_EQUAL instruction handles both '==' and '!=' operations incorrectly.
---
---
[NEUT-003] - Potential Stack Overflow in Complex Expression Evaluation
Status: Identified
Discoverd on: Wednesday, October 29, 2025 - Linux - Neutron-1.1.1-beta
Description : Complex nested expressions with multiple operations could potentially cause stack overflow due to the fixed stack size limit of 256 entries without proper bounds checking in all evaluation paths.
---
---
[NEUT-004] - Memory Management Issues with Object Heap
Status: Fixed
Discoverd on: Wednesday, October 29, 2025 - Linux - Neutron-1.1.1-beta
Description : The garbage collector implementation is incomplete with basic marking and sweeping but lacks proper root finding for complex object relationships, which could lead to memory leaks or premature deallocation. Fixed with improved recursive marking of nested objects and automatic GC triggering.
---
---
[NEUT-005] - Exception Handling Implementation Inconsistency
Status: Identified
Discoverd on: Wednesday, October 29, 2025 - Linux - Neutron-1.1.1-beta
Description : Exception handling has been added with TRY/THROW/CATCH opcodes but the implementation has inconsistent frame management and lacks proper exception re-throwing in finally blocks.
---
---
[NEUT-006] - Missing Symbol Operators for Common Operations
Status: Partially Fixed
Discoverd on: Wednesday, October 29, 2025 - Linux - Neutron-1.1.1-beta
Description : C-style operators `&&`, `||`, `++`, `--` are not implemented. The `&&` and `||` symbol operators have been implemented and work the same as `and` and `or` keywords. The `++` and `--` operators require more complex AST changes and remain to be implemented.
---
---
[NEUT-007] - Array Index Out of Bounds Error Handling
Status: Fixed
Discoverd on: Wednesday, October 29, 2025 - Linux - Neutron-1.1.1-beta
Description : Array index access has bounds checking but doesn't provide detailed error information about which array or line caused the error, making debugging difficult for users.
---
---
[NEUT-008] - Module Loading Performance Bottleneck
Status: Identified
Discoverd on: Wednesday, October 29, 2025 - Linux - Neutron-1.1.1-beta
Description : Module loading implementation re-scans and re-parses files each time, leading to performance degradation when modules are loaded multiple times in complex applications.
---
---
[NEUT-009] - Type Safety in Array Operations Not Enforced
Status: Identified
Discoverd on: Wednesday, October 29, 2025 - Linux - Neutron-1.1.1-beta
Description : Array methods like push, slice, indexOf do not enforce type consistency when used with typed variables, potentially allowing type violations.
---
---
[NEUT-010] - String Interpolation with Non-String Values
Status: Identified
Discoverd on: Wednesday, October 29, 2025 - Linux - Neutron-1.1.1-beta
Description : String interpolation using ${...} syntax may not properly handle all value types during expression evaluation, potentially causing runtime errors or unexpected string representations.
---
---
[NEUT-011] - Equality Comparison Performance Optimization
Status: Fixed
Discoverd on: Wednesday, October 29, 2025 - Linux - Neutron-1.1.1-beta
Description : The equality (==) and inequality (!=) operators were performing expensive string conversions for all comparisons. Optimized to perform type-specific comparisons first, only falling back to string comparison for complex types.
---