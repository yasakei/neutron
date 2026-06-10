# LLVM AOT Backend Migration

## Goal
Replace C++ string codegen (`src/aot/aot_compiler.cpp`) with direct LLVM IR emission for reliable cross-platform AOT with Rust-grade performance.

---

> **Status**: Phases 0–3 complete ✅. Phase 4.1–4.5 complete ✅. Phase 5 complete ✅. Remaining: Phase 6 (future optimizations).

---

## Phase 0: Dependencies & Build Infrastructure [✅ DONE]

### 0.1 — Add LLVM CMake detection
- Add to `CMakeLists.txt`: `find_package(LLVM REQUIRED CONFIG)`, `llvm_map_components_to_libnames()` for IRReader, Target, TargetInfo, etc.
- Add `src/aot/llvm_codegen.cpp` + `.h` to `NEUTRON_COMPILER_SOURCES`
- Link `${LLVM_AVAILABLE_LIBS}` to `neutron_runtime` and `neutron_shared`

### 0.2 — CI: install LLVM on all platforms
- **Linux (Arch)**: `pacman -S llvm` (replaces `pacman -S gcc`? Keep both for now)
- **macOS**: `brew install llvm` (note: `llvm-config` not in PATH by default on Apple Silicon)
- **Windows**: LLVM via vcpkg or fetch prebuilt from GitHub Releases
- Verify with `llvm-config --version` in CI output

### 0.3 — Create `src/aot/llvm_codegen.h`
- Class `LlvmCodegen` with same public API as `AotCompiler`:
  - Constructor taking `const Chunk*`
  - `setDebugMode(bool)`, `setTargetPlatform(TargetPlatform)`
  - `generateModule(const std::string& functionName)` → emits `.o` or `.obj` file
  - `getSourceMap()` (same interface)
- Fields: `LLVMContext`, `Module`, `IRBuilder<>`, function map, basic block map
- Header stays minimal — LLVM includes go in `.cpp` only

### 0.4 — Create `src/aot/llvm_codegen.cpp`
- Initialize LLVM: `LLVMInitializeNativeTarget()`, `LLVMInitializeNativeAsmPrinter()`
- For now: just a stub that emits a `ret void` function and writes an object file
- Verify it compiles and links on Linux (`cmake --build`)

---

## Phase 1: Core Stack Operations [✅ DONE]

### 1.1 — Value type as LLVM struct
- ✅ Use `i64` with NaN-boxing (deferred to Phase 4, but implemented early)

### 1.2 — Constant/load/store opcodes
- ✅ `OP_CONSTANT`, `OP_CONSTANT_LONG`, `OP_NIL`, `OP_TRUE`, `OP_FALSE`
- ✅ `OP_GET_LOCAL`, `OP_SET_LOCAL`
- ✅ Stack: `alloca [512 x i64]` array + `i32` SP (merged stack+locals buffer)

### 1.3 — Arithmetic & comparison opcodes
- ✅ `OP_ADD`, `OP_SUBTRACT`, `OP_MULTIPLY`, `OP_DIVIDE`, `OP_MODULO`, `OP_NEGATE`
- ✅ `OP_EQUAL`, `OP_GREATER`, `OP_LESS`, `OP_NOT_EQUAL`, `OP_NOT`
- ✅ Integer-specialized variants: `OP_ADD_INT`, `OP_SUB_INT`, etc.
- ✅ Bitwise: `OP_BITWISE_AND`, `OP_BITWISE_OR`, `OP_BITWISE_XOR`, `OP_BITWISE_NOT`, `OP_LEFT_SHIFT`, `OP_RIGHT_SHIFT`

### 1.4 — Stack management
- ✅ `OP_POP`, `OP_DUP`
- ✅ `OP_SAY` — calls `aot_printValue` runtime

### 1.5 — Bump `aot_compiler.cpp` to delegate or co-exist
- ✅ Old codegen deleted, `project_builder.cpp` uses `LlvmCodegen`

---

## Phase 2: Control Flow & Functions [✅ DONE]

### 2.1 — Jumps & branches
- ✅ `OP_JUMP`, `OP_JUMP_IF_FALSE`, `OP_LOOP`
- ✅ Two-pass: first collect all block targets, then emit
- ✅ Fused ops: `OP_LESS_JUMP`, `OP_GREATER_JUMP`, `OP_EQUAL_JUMP`
- ✅ `OP_LOOP_IF_LESS_LOCAL`, `OP_INCREMENT_LOCAL`, `OP_DECREMENT_LOCAL`, integer variants

### 2.2 — Globals
- ✅ `OP_GET_GLOBAL`, `OP_SET_GLOBAL`, `OP_DEFINE_GLOBAL`
- ✅ LLVM module-level `@global_<name>` globals
- ✅ `OP_GET_GLOBAL_FAST`, `OP_SET_GLOBAL_FAST`
- ✅ `OP_DEFINE_TYPED_GLOBAL`, `OP_SET_GLOBAL_TYPED`, `OP_SET_LOCAL_TYPED`
- ✅ `OP_INCREMENT_GLOBAL`

### 2.3 — Functions & calls
- ✅ `OP_CALL`, `OP_CALL_FAST`, `OP_TAIL_CALL` — calls `aot_invoke` runtime
- ✅ `OP_RETURN`
- ✅ `OP_CLOSURE`, `OP_GET_UPVALUE`, `OP_SET_UPVALUE`, `OP_CLOSE_UPVALUE`

### 2.4 — Arrays & objects
- ✅ `OP_ARRAY` — calls `aot_createArray` runtime
- ✅ `OP_OBJECT` — calls `aot_createObject` runtime
- ✅ `OP_INDEX_GET`, `OP_INDEX_SET` — calls `aot_indexGet`/`aot_indexSet` runtime
- ✅ `OP_GET_PROPERTY`, `OP_SET_PROPERTY` — calls `aot_getProperty`/`aot_setProperty` runtime
- ✅ `OP_THIS`, `OP_INVOKE` — calls `aot_invoke` runtime

### 2.5 — Extended opcodes
- ✅ `OP_LOAD_LOCAL_0` through `OP_LOAD_LOCAL_3`, `OP_CONST_ZERO`, `OP_CONST_ONE`, `OP_CONST_INT8`
- ✅ `OP_ADD_LOCAL_CONST` (fused local + const + add)
- ✅ `OP_BREAK`, `OP_CONTINUE`, `OP_END_TRY`, `OP_LOOP_HINT` — no-ops in AOT
- ✅ `OP_TRY`, `OP_THROW` — operand skip only (no EH in AOT)
- ✅ `OP_FOR_IN_INIT`, `OP_FOR_IN_NEXT`
- ✅ `OP_OPTIONAL_CHAIN`, `OP_SPREAD`
- ✅ `OP_TYPE_GUARD`, `OP_VALIDATE_SAFE_*` — operand skip only

---

## Phase 3: Remove Old C++ Codegen [✅ DONE]

### 3.1 — Delete `src/aot/aot_compiler.cpp` + `include/aot/aot_compiler.h`
- ✅ Removed from `CMakeLists.txt`
- ✅ All opcodes replaced by `llvm_codegen.cpp`

### 3.2 — Update `project_builder.cpp`
- ✅ `#include "aot/llvm_codegen.h"` replaces old include
- ✅ `LlvmCodegen::generateModule()` emits `.o` file
- ✅ Linked with system linker (`gcc`/`clang`)
- ✅ MSVC vcvarsall logic removed

### 3.3 — Update include path guard naming
- ✅ `NEUTRON_LLVM_CODEGEN_H`

---

## Phase 4: Optimization & Performance

### 4.1 — LLVM optimization pipeline [✅ DONE]
- ✅ `PassBuilder` with default O2 pipeline before emitting object code
- ✅ TargetMachine opt level set
- ✅ LLVM `verifyModule()` after codegen

### 4.2 — Type specialization (unboxed values) [✅ DONE]
- Int-specialized ALU ops: `fptosi` → `i64` op → `sitofp` round-trip for integer semantics
- Int comparisons: `icmp eq/slt/sgt` instead of `fcmp oeq/olt/ogt`
- Int inc/dec: `i64` add/sub with constant 1, leaving float paths for generic variants
- `OP_LOOP_IF_LESS_LOCAL`: `icmp slt` instead of `fcmp olt`
- NaN-boxing: doubles stored in `i64` directly, pointer tagging for objects
- No runtime type checks needed — int-specialized opcodes guarantee integer operands

### 4.3 — Inline caching [✅ DONE]
- Per-callsite `AotPropCache` struct `{klass*, inlineIndex}` stored as LLVM module-level global
- `aot_getPropertyCached` / `aot_setPropertyCached` runtime helpers check klass match → direct inline field access on hit, full lookup + cache update on miss
- Applied to `OP_GET_PROPERTY`, `OP_SET_PROPERTY`, `OP_OPTIONAL_CHAIN`
- Avoids string hash table lookup on repeated property access to same class

### 4.4 — Codegen tuning [✅ DONE]
- `-march=native` for NATIVE target: use `getHostCPUName()` + `getHostCPUFeatures()` to tune for current CPU
- Cross-compilation targets: keep `"generic"` CPU (unchanged)
- Suppress verbose IR dumps in release builds (`#ifdef NDEBUG`)
- LLVM LTO (bitcode emission + lld plugin) deferred — O2 pipeline already provides strong optimization

### 4.5 — Profile-guided optimization [✅ DONE]
- `NEUTRON_PGO` CMake option: `OFF` / `GENERATE` / `USE=<path>`
- GENERATE: adds `-fprofile-generate=<dir>` to compile+link flags
- USE: adds `-fprofile-use=<file>` to compile+link flags
- `scripts/pgo_collect.sh`: automates build → run → merge workflow
- `.gitignore`: `pgo.profdata`, `build-pgo/`

---

## Phase 5: Cross-compilation via LLVM [✅ DONE]

### 5.1 — Initialize all LLVM backends
- Replaced `InitializeNativeTarget()` + `InitializeNativeTargetAsmPrinter()` with `InitializeAllTargetInfos()`, `InitializeAllTargets()`, `InitializeAllTargetMCs()`, `InitializeAllAsmPrinters()` for cross-target codegen
- Added `AArch64` and `X86` to `llvm_map_components_to_libnames()` in CMakeLists.txt for static lib builds
- All 20 LLVM backends available for cross-compilation

### 5.2 — Wire `--target` flag to LLVM codegen
- Added `arch→TargetPlatform` mapping in `project_builder.cpp`: parses ARM64 vs X64 vs X86, detects Linux/macOS/Windows from target triple string
- Calls `setTargetPlatform()` on `LlvmCodegen` before `generateModule()` for cross-compilation
- Existing triple mapping (`llvm_codegen.cpp:1686-1693`) already handles all 6 non-native platforms
- TargetRegistry::lookupTarget() already in use for backend auto-detection

---

## Testing Strategy

| Step | Tests | Status |
|------|-------|--------|
| Phase 0 | `cmake --build` succeeds, `ldd neutron` shows LLVM libs | ✅ |
| Phase 1 | `test_arithmetic.aot.nt`, `test_bitwise.aot.nt` via LLVM codegen + compare output | ✅ |
| Phase 2 | `test_conditionals.aot.nt`, `test_loops.aot.nt`, `test_functions.aot.nt`, `test_globals.aot.nt` | ✅ |
| Phase 3 | All 10 `tests/aot/*.aot.nt` pass with LLVM-only codegen (128/128 all tests) | ✅ |
| Phase 4 | Benchmarks are within 20% of Rust `--release` equivalent | ⬜ |
| Phase 5 | Cross-compile `test_computation.aot.nt` for `aarch64-linux-gnu` | ⬜ |

---

## Key Decisions

1. ~~Keep `Value` struct as tagged union in LLVM IR initially~~ → NaN-boxing (`i64`) used from the start.
2. **Emit `.o` files, not assembler** — LLVM's `TargetMachine::addPassesToEmitFile()` with `CGFT_ObjectFile`.
3. **Link with system linker** — no need for `lld` until LTO in Phase 4.
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
