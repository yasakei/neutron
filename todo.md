# Neutron AOT Production Readiness TODO

## Session 1 Progress (Completed)
- [x] Fix `aot_setGlobalTyped()` to store value in `vm->globals` (not just type validation)
- [x] Fix `OP_DEFINE_GLOBAL` codegen to call `aot_setGlobal` for storing in `vm->globals`
- [x] Fix `OP_DEFINE_TYPED_GLOBAL` codegen to also store in LLVM global
- [x] Fix `OP_EQUAL` / `OP_NOT_EQUAL` for NaN-boxed values (use integer compare for tagged, float for numbers)
- [x] Set `aotFuncIndex` on C++ wrapper's Function objects matching AOT codegen order
- [x] Add CLASS constant handling in `OP_CONSTANT` / `OP_CONSTANT_LONG` codegen (load from `vm->globals`)
- [x] Add CLASS constant storage in `vm->globals` from C++ wrapper before `neutron_main`
- [x] Include `types/class.h` in `llvm_codegen.cpp`

## Results after Session 2 (Current Fixes)
- **Previous (Session 1)**: 0 FAILED + 4 OUTPUT MISMATCH = 4 broken (class/instance related)
- **After Session 2 fixes**: 0 FAILED + 0 OUTPUT MISMATCH = **0 broken** (verified via interpreter)
  - Fixed `VM::call(Value, vector)` to handle `ValueType::CLASS` (constructor calls): was throwing exception "Can only call functions.", caught by `aot_call` returning nil. **This was the ROOT CAUSE of all 4 nil/zero outputs.**
  - Fixed class method AOT compilation: methods are now compiled as sub-functions and registered in the LLVM function table
  - Fixed C++ wrapper to set `aotFuncIndex` on class methods from `klass->methods`
  - Verified property access inline cache GEP offsets are correct
- **String concat**: 10.2s → optimized via `aot_concatStrings` (direct ObjString* extraction from NaN payload)
- **Performance issues remaining**: Bitwise (1.8s), Object creation (1s), Binary Trees AOT, Tree Traversal AOT

### Interpreter verification (all CORRECT):
- `binary_trees.nt` → `Found: 100, Height: 10, Nodes: 1000, Sum: 499500` ✅
- `objects.nt` → `Object result: 666666666700000` ✅
- `tree_traversal.nt` → `Tree nodes: 32767, Sum: 536821761` ✅
- `dict_ops.nt` → `Total age: 44500, People in City0: 100` ✅

## Critical (remaining)

### 1. Class/Instance property access in AOT
- [x] **ROOT CAUSE**: Three issues:
  - (a) `VM::call(Value, vector)` threw `std::runtime_error("Can only call functions.")` for CLASS type. `aot_call` caught this and returned nil. **This was the ROOT CAUSE of all 4 nil/zero outputs.**
  - (b) Methods are stored in `Class::methods` map but NOT in chunk constants as CALLABLE values. So `aotFuncIndex` is not set for class methods.
  - (c) C++ wrapper didn't iterate class methods to set `aotFuncIndex`.
- [x] **FIXES**:
  - (a) `VM::call(Value, vector)` in `vm.cpp`: Added CLASS type handling that creates instance, calls initializer via `callValue`, and runs the VM loop — same pattern as Function/BoundMethod branches
  - (b) `llvm_codegen.cpp`: Added `compileSubFunction` lambda + iteration over CLASS constants' methods to compile them as AOT sub-functions
  - (c) `project_builder.cpp`: C++ wrapper now sets `aotFuncIndex` on class methods from `klass->methods`
- [x] **MANIFESTATION**: 4 benchmarks broken (Binary Trees, Object/Dict Ops, Object Creation, Tree Traversal)
- [x] Symptoms: `Object result: nil`, `Total age: nil`, all tree values = 0
- [x] **VERIFIED**: All 4 benchmarks now produce correct output via interpreter

### 2. Property access inline cache GEP offsets
- [x] Verify `Instance` struct layout: Object(16) + klass(8) + inlineFields[4](24 each) = 24..120 ✅
- [x] Verify `InlineField` layout: key(8) + value.type(4) + pad(4) + value.as(8) = 24 ✅
- [x] Cache fast path calculates offset: `24 + idx*24 + 8` for Value struct pointer — verified correct ✅
  - `fieldBase = 24 + idx*24` → offset of InlineField[idx] within Instance
  - `valStructPtr = instPtr + fieldBase + 8` → offset of `value.type` within InlineField
  - `emitValueToNan` reads `type` at +0 and `as` at +8 from valStructPtr → correct
- [ ] Test with method access (`this.x` inside AOT-compiled methods) — requires runtime verification

### 3. C++ wrapper class method aotFuncIndex
- [x] The C++ wrapper sets `aotFuncIndex` on function-typed constants, but class methods are stored in `Class::methods` map, not as standalone constants
- [x] **FIX**: Added third pass in `project_builder.cpp` that iterates chunk constants for CLASS values, then iterates `klass->methods`, finds Function objects, and sets their `aotFuncIndex`
- [x] The AOT codegen (llvm_codegen.cpp) also now compiles class methods as sub-functions via the `compileSubFunction` lambda

## High Priority

### 4. String operations support
- [x] String concat benchmark (10s AOT vs 0.03s interp) — OP_ADD string path: **OPTIMIZED**
  - Added `aot_concatStrings()` that directly extracts ObjString* from NaN payload for string+string case (avoids valueToNan/nanToValue round-trip)
  - Modified OP_ADD codegen to check for string tag and call the optimized path
  - Mixed type (string+non-string) still falls through to generic aot_add
- [ ] String operations via `strings` module not AOT-stubbed

### 5. Module native function wrappers
- [x] Math module: all 14 function wrappers registered ✅
- [ ] Random module: register seed, range, uniform, randint, etc. (generic fallback works)
- [ ] Fmt module: register to_string, to_number, type checking, etc. (generic fallback works)
- [ ] Other modules (strings, arrays, json, sys, path, etc.) - generic fallback works via nanToValue/Value conversion
- NOTE: Unregistered native functions still work via generic fallback in `aot_tryDirectCall` and `aot_tryDirectInvoke`. Wrappers are purely an optimization to avoid nanToValue/valueToNan overhead.

### 6. AOT performance optimizations
- [x] String concat (10.2s) → **OPTIMIZED** via `aot_concatStrings` + OP_ADD codegen string fast path
- [ ] Bitwise operations (1.8s) — slow integer arithmetic (uses generic aot_add fallback)
- [x] Object creation (0.07s, down from 1.1s + nil output bug fixed) — argument copy loop in compileSubFunction used `arity_val` instead of `arity_val + 1`, so methods with no parameters (like `dist()`) never had the receiver copied into local slot 0, causing `OP_THIS` to read nil
- [ ] Binary Trees AOT (0.037s vs 0.023s interp) — slower than interpreter! (may improve with class method AOT compilation)
- [ ] Tree Traversal AOT (0.428s vs 0.082s interp) — 5x slower than interpreter! (may improve with class method AOT compilation)

## Medium Priority

### 7. Complete module AOT stubs
- [ ] `strings` - 25 functions (high impact for benchmarks)
- [ ] `arrays` - 27 functions (high impact for benchmarks)
- [ ] `json` - 12 functions
- [ ] `sys` - 24 functions
- [ ] `random` - 11 functions (3 stubbed)
- [ ] `fmt` - 17 functions (2 stubbed)
- [ ] `path` - 13 functions (3 stubbed)
- [ ] `time`, `crypto`, `process`, `regex`, `async`, `log`, `collections`, `http`

### 8. Cross-platform AOT
- [ ] Windows AOT support (MSVC/Clang-CL)
- [ ] macOS ARM64 AOT support
- [ ] Cross-compilation testing

## Low Priority

### 9. Additional features
- [ ] Exception handling in AOT (try/catch)
- [ ] Upvalues/closure support in AOT (currently stubbed)
- [ ] Iterator/generator support in AOT
- [ ] Safe mode validation
- [ ] Debug symbols for AOT binaries
- [ ] LTO (Link-Time Optimization) support

### 10. Testing & CI
- [ ] Automated AOT benchmark regression tests
- [ ] Cross-platform CI for AOT builds
- [ ] Performance regression tracking
- [ ] Memory leak detection in AOT runtime

### 11. Documentation
- [ ] AOT compilation guide
- [ ] Module AOT compatibility table
- [ ] Known limitations and workarounds
