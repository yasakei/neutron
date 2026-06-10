# LLVM AOT Backend Migration

## Goal
Replace C++ string codegen (`src/aot/aot_compiler.cpp`) with direct LLVM IR emission for reliable cross-platform AOT with Rust-grade performance.

---

> **Status**: Phases 0–5 complete ✅. Core stack/control-flow/function ops working. All 128 interpreter + 10 AOT tests pass. Remaining correctness items and Phase 6 (static module linking) below.

---

## Phases 0–5: Complete ✅

All foundational work is done:
- **0** — LLVM CMake detection, CI install, `llvm_codegen.h/.cpp` scaffold
- **1** — All core stack ops, arithmetic, comparisons, stack management
- **2** — All control flow (jumps/loops/fused), globals, functions/calls/closures, arrays/objects, extended opcodes
- **3** — Old C++ string codegen deleted, `project_builder.cpp` uses `LlvmCodegen`
- **4** — O2 optimization pipeline, int type specialization, inline caching, codegen tuning, PGO
- **5** — All 20 LLVM backends initialized, `--target` flag wired for ARM64/X64/X86 cross-compilation

---

## Remaining AOT Fixes (correctness)

### `OP_ADD` with string operands
- Currently emits `fadd` on NaN-boxed i64 values, producing garbage when one operand is a string.
- **Fix**: Add `aot_add` runtime helper that checks tag bits → calls `Value::operator+` for string concatenation, else `fadd` for numbers.
- Test: `"hello " + "world"`, `"result=" + 42`, etc. in AOT `.aot.nt` tests.

### String constants in `@constants` array
- `createConstants()` stores nil for strings (heap pointers invalid at compile time).
- OP_CONSTANT for string literals pushes nil — currently `aot_internString` is only used for `say()` arguments.
- **Fix**: Either (a) add `aot_getConstant` runtime helper for string constants, or (b) emit `aot_internString` calls for all string-typed OP_CONSTANT loads.

### `OP_TRY` / `OP_THROW` / `OP_END_TRY`
- Currently exits via `fprintf`+`exit(1)` on throw.
- **Fix**: Add `setjmp`/`longjmp`-based unwinding in AOT runtime, or proper LLVM `landingpad` EH. `OP_TRY` pushes a jmp_buf, `OP_THROW` longjmps back to the nearest catch point.

### `OP_FOR_IN` edge cases
- Runtime helpers work for basic cases but may leak iteration objects on early exit.
- **Fix**: Ensure cleanup always runs (close iterable/keys on break/throw).

### Validation opcodes (`OP_VALIDATE_SAFE_*`, `OP_TYPE_GUARD`, typed set ops)
- Currently prints error + `exit(1)` on type mismatch.
- **Fix**: Should throw a proper VM error (via `setjmp`/`longjmp` or by setting VM error state).

### Property inline caching — full LLVM IR inlining
- Fast path still calls `aot_getPropertyCached` runtime helper (function call overhead).
- **Fix**: Emit cached-path field access directly in LLVM IR (GEP + klass check + inline load).

### Miscellaneous
- `OP_THIS` currently loads from merged stack+locals slot 0 — verify edge cases with nested closures.
- `OP_BREAK`/`OP_CONTINUE`/`OP_LOGICAL_AND`/`OP_LOGICAL_OR` — compiler inlines these into `OP_JUMP`/`OP_LOOP`, but safety-net handlers should be verified.
- `OP_ADD_LOCAL_CONST` uses `fadd` — int-specialized variant needed for consistency.

---

## Phase 6: Static Module Linking for AOT

### 6.1 — Module interface audit [✅ DONE]
- Listed all 16 built-in modules, their source files, and every exported function signature (~210 total).
- Identified 4 modules with AOT stubs (`math` 11, `random` 3, `fmt` 2, `path` 3) and 12 without.
- All modules use `Value(VM& vm, std::vector<Value> args)` calling convention.
- Full audit saved in `docs/aot_module_audit.md`.

### 6.2 — Compile C++ modules to LLVM bitcode
- Add CMake option to build each module `.cpp` as a bitcode file (`.bc`):
  - `-flto -emit-llvm -c` for each module source
  - Or `-save-temps` during normal build and collect `.bc` files
- Install `.bc` files alongside the Neutron runtime library.
- Create a helper `getModuleBitcode(name)` that returns the precompiled bitcode module by name.

### 6.3 — Emit direct `extern "C"` calls in LLVM IR
- Replace `aot_invoke` runtime dispatch with direct function declarations for known module functions:
  ```llvm
  declare i64 @math_sqrt(ptr %vm_ctx, i64 %x)
  ```
- Map each module call opcode to the correct bitcode symbol.
- Handle argument marshalling and return values.

### 6.4 — Thin wrapper generation
- Generate thin C wrappers per exported function that:
  - Unpack tagged values from AOT calling convention
  - Call the real module implementation
  - Pack the result back into a tagged value

### 6.5 — LTO linking pipeline
- Replace system linker call with `lld` or `gcc -flto`:
  - Neutron LLVM IR (`.o`) + module bitcode (`.bc`) + runtime (`.a`)
  - Full LTO across all components
- Ensure all module symbols are resolved at link time (no `dlopen`/`dlsym`).

### 6.6 — Module dependency resolution
- `project_builder.cpp` `nonAotModules` set → remove entries as they get AOT support.
- For each module, also link its transitive dependencies.
- Verify no circular dependencies in module graph.

### 6.7 — Test all 128 tests via AOT
- Run `tests/run_tests.py --aot` with full module support.
- Module tests (`modules/` dir) should produce identical output to interpreter.
- Benchmark suites should show performance improvement from direct calls + LTO.

---

## Key Decisions

1. NaN-boxing (`i64`) used from the start — no `Value` struct in LLVM IR.
2. **Emit `.o` files, not assembler** — LLVM's `TargetMachine::addPassesToEmitFile()` with `CGFT_ObjectFile`.
3. **Link with system linker** — no need for `lld` until LTO in Phase 6.
4. **Two-pass codegen** (first pass: collect blocks & function signatures, second pass: emit IR).
5. **No dependency on `proton/`** — LLVM replaces both QBE and the C++ string codegen.

---

## Learnings from `proton` branch (what to avoid)

| Issue | Proton approach | LLVM approach |
|-------|----------------|---------------|
| `init_optab()` fragile on MSVC | QBE required init | LLVM C API always safe |
| `filluse()` / `ssacheck()` crashes | QBE SSA validator | LLVM `verifyModule()` is robust |
| GAS→MASM converter | 570-line kludge | LLVM emits proper COFF directly |
| `tmpfile()` fails on Windows | Temp file hack | LLVM uses in-memory buffers |
| No optimization = slow output | QBE baseline | LLVM O3 pipeline built-in |
| Cross-compilation hacks | Manual `sed` on asm | LLVM `Triple` system |
