# LLVM AOT Backend Migration

## Goal
Replace C++ string codegen (`src/aot/aot_compiler.cpp`) with direct LLVM IR emission for reliable cross-platform AOT with Rust-grade performance.

---

> **Status**: Phases 0–5 complete ✅. All 24 AOT stub opcodes replaced with LLVM IR + C++ runtime helpers. All 128 interpreter + 10 AOT tests pass. Not yet "pure LLVM" — nearly every non-trivial operation calls a C++ helper; `aot_call` dispatches through interpreter bytecode loop; 8 modules force interpreter fallback. See "Remaining Work" below.

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

## Remaining Work: "Pure LLVM" (no C++ runtime helpers)

### Summary

The current AOT backend generates LLVM IR for the bytecode opcodes of the *entry function*, but nearly every non-trivial operation calls a C++ runtime helper via `extern "C"`. It is **not** pure LLVM — it's a hybrid where LLVM handles register allocation, control flow, and simple arithmetic, while everything else bounces through the C++ runtime.

### 1. Inline runtime helpers into LLVM IR

Every operation below still calls a C++ function instead of generating native LLVM IR:

| Helper | What it does | Why it matters |
|--------|-------------|----------------|
| ~~`aot_add`~~ | String concat or numeric `fadd` | ✅ **Done**: OP_ADD checks tag bits in LLVM IR — both numbers → direct `fadd`; otherwise → helper call |
| `aot_getProperty` / `aot_setProperty` | Hash lookup on ObjString key | Inline hash + table probe |
| `aot_getPropertyCached` / `aot_setPropertyCached` | Inline cache fast path | Emit `GEP` + klass check in IR (avoid call overhead) |
| `aot_indexGet` / `aot_indexSet` | Array/ObjString index | Inline bounds check + element load |
| `aot_createArray` / `aot_createObject` | Heap allocation | Inline `malloc` + init loop (or call GC helper) |
| `aot_internString` | String interning | Inline `strlen` + hash + table insert |
| `aot_forInInit` / `aot_forInNext` | Iterator protocol | Inline keys() call + index advance |
| `aot_spread` | Spread operator | Inline array copy loop |
| `aot_printValue` | Value printing | Inline type-dispatch on tag |
| `aot_throwError` / `aot_runtimeError` | Error reporting | Inline `setjmp`/`longjmp` or `landingpad` |

### 2. Replace `aot_call` with direct native calls

- `aot_call` dispatches through **interpreter bytecode loop** (`vm->call(callee, args)`) — the AOT entry function calls back into the interpreter for every sub-function call.
- **Fix**: Emit LLVM IR that directly calls the callee's compiled entry point. Requires:
  - Compiling all user-defined functions to LLVM IR (not just the entry point)
  - Building a function table mapping function index → `void(*)()` at JIT time
  - Marshalling args in LLVM IR instead of stack buffer + `aot_call`
  - Handling tail calls via `musttail`

### 3. Remove `nonAotModules` set

8 modules still force 100% interpreter fallback:

| Module | Reason |
|--------|--------|
| `http` | Uses libcurl; needs bitcode compilation |
| `json` | Uses jsoncpp; needs bitcode compilation |
| `sys` | OS calls (file I/O, env, etc.) |
| `time` | System clock calls |
| `crypto` | OpenSSL/libcrypto |
| `process` | Subprocess + signals |
| `arrays` | Complex array manipulation |
| `async` | Async/await state machine |
| `regex` | PCRE2/libre |

**Fix**: Phase 6 — compile each C++ module to LLVM bitcode (`.bc`), emit direct `extern "C"` calls in the LLVM IR, and LTO-link everything together.

### 4. Exception handling via LLVM `landingpad`

- `aot_tryPush` / `aot_tryPop` currently save/restore a `jmp_buf` on a VM-side frame stack.
- `aot_throwError` calls `longjmp`, unwinding through C frames (losing LLVM `opt` visibility).
- **Fix**: Use LLVM `landingpad` + `resume` instructions for native-quality EH, enabling cleanup landing pads and proper stack unwinding.

### 5. Property inline caching — full IR inlining

- `aot_getPropertyCached` still has function call overhead even on cache hit.
- Cache is a module-level global `{klass*, inlineIndex}` → fast path is `load klass → icmp → gep inlineIndex → gep instance slots → load`.
- **Fix**: Emit this sequence as inline IR in each callsite; only call the miss handler helper on cache miss.

---

## Phase 6: Static Module Linking for AOT [⏳ NOT STARTED]

### 6.1 — Module interface audit [✅ DONE]
- Listed all 16 built-in modules, their source files, and every exported function signature (~210 total).
- Identified 4 modules with AOT stubs (`math` 11, `random` 3, `fmt` 2, `path` 3) and 12 without.
- All modules use `Value(VM& vm, std::vector<Value> args)` calling convention.
- Full audit saved in `AOT_MODULE_AUDIT.md`.

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
