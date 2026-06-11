# LLVM AOT Backend Migration

## Status
✅ **Phases 0–16 complete** — All bytecode opcodes emit LLVM IR; user-defined functions compile to native IR and dispatch directly. 128/128 interpreter + 10/10 AOT tests pass.

❌ **Phase 17** — Module linking. See below.

---

## ✅ Completed — Phases 0–16

| Phase | What |
|-------|------|
| **0** | LLVM CMake detection, CI install, `llvm_codegen.h/.cpp` scaffold |
| **1** | Core stack ops, arithmetic, comparisons, stack management |
| **2** | Control flow, globals, functions/calls/closures, arrays/objects, extended opcodes |
| **3** | Old C++ string codegen deleted, `project_builder.cpp` uses `LlvmCodegen` |
| **4** | O2 optimization pipeline, int type specialization, inline caching, PGO |
| **5** | All 20 LLVM backends, `--target` flag for ARM64/X64/X86 cross-compilation |
| **6** | Direct native calls — `aot_tryDirectCall` fast path, sub-function compilation to LLVM IR, `aot_registerLlvmFunc` table, `aotFuncIndex` lookup |

---

## 📋 Remaining Work — Inline C++ Runtime Helpers into Pure LLVM IR

Every helper below is a C++ function called from LLVM IR. The goal is to inline each one into native IR instructions. Phases are ordered by effort + impact.

---

### ✅ Phase 7: Value Printing (`OP_SAY`) — EASY ✅

**Done**: Inlined `aot_printDoubleNumber` and `aot_printStringObj` into direct `printf` calls in IR. `aot_printValue` kept as fallback for virtual `toString()` dispatch (tag ≥ 3: Array/Instance/Callable/Object).

**Removed from struct**: `aotPrintDoubleNumberFunc`, `aotPrintStringObjFunc`
**Kept**: `aotPrintFunc` (fallback)

---

### ✅ Phase 8: Property Access Inline Cache — EASY ✅

**Done**: Cache hit path emits `icmp eq` on klass pointers, `GEP` into `inlineFields[idx].value`, and `emitValueToNan`/`emitNanToValue` directly — no helper call.

**Removed**: `aotTryGetCachedPropFunc`, `aotTrySetCachedPropFunc` (from struct + declareExternals)
**Kept**: `aotGetPropCachedFunc`, `aotSetPropCachedFunc` (miss handler — full hash lookup)

---

### ✅ Phase 9: Array Index Access (`OP_INDEX_GET` / `OP_INDEX_SET`) — EASY ✅

**Done**: Array fast path (untagged check + bounds check + GEP + emitValueToNan/emitNanToValue) inlined directly in IR.

**Removed**: `aotArrayGetCachedFunc`, `aotArraySetCachedFunc`
**Kept**: `aotIndexGetFunc`, `aotIndexSetFunc` (fallback — JsonObject key, ObjString charAt)

---

### ✅ Phase 10: String Interning (`OP_CONSTANT` for strings) — MEDIUM ✅

**Done**: Lazy intern cache approach — per-chunk global i64 array (`internCacheGlobal`). First access calls `aot_internString` helper + caches result; subsequent loads skip the call entirely. Phi merges cached vs fresh interned value.

**Kept**: `aotInternFunc` (still needed for first-time intern cache miss)

---

### ✅ Phase 11: Array/Object Creation (`OP_ARRAY` / `OP_OBJECT`) — MEDIUM ✅

**Done**: `aot_createArray` replaced with `aot_allocArray` (alloc + reserve only). Element copy loop inlined in IR via `emitNanToValue` + vector `_M_finish` update. Avoids per-element `push_back` capacity checks and `nanToValue` dispatch.

`aot_createObject` kept as helper (unordered_map insertion is too complex).

**Changed**: `aotCreateArrayFunc` → `aotAllocArrayFunc` (returns raw Array* instead of NaN-boxed value)

---

### ✅ Phase 12: For-In / Spread — MEDIUM ✅

**Done**: `aot_forInNext` and `aot_spread` inlined into IR.
- `aot_forInNext`: reused Phase 9's Array access pattern (untagged check + bounds check + GEP + `emitValueToNan`); nil fallback for non-array/non-number/OOB.
- `aot_spread`: Array path copies up to 256 elements via inline loop with `emitValueToNan`; non-Array path stores the value directly (count=1).

**Kept**: `aotForInInitFunc` (complex — JsonObject property iteration + GC alloc)
**Removed**: `aotForInNextFunc`, `aotSpreadFunc` (from struct + declareExternals + C++ source)

---

### ✅ Phase 13: Exception Handling (`OP_TRY` / `OP_END_TRY` / `OP_THROW`) — HARD ✅

| Helper | What we did |
|--------|-------------|
| `aot_tryPush` | Removed — AOT `OP_THROW` exits unconditionally via `aot_throwError`, so pushed frames are never read |
| `aot_tryPop` | Removed — paired with `aot_tryPush` removal |
| `aot_throwError` | Kept — prints error + stack trace + `exit(1)`; cold path |

**Done**:
- Replaced `std::vector<ExceptionFrame> exceptionFrames` with `ExceptionFrame exceptionFrameStack[64]` + `int exceptionFrameDepth` (fixed-size array, no heap allocation)
- Removed unused `fileName` (`std::string`) / `line` (`int`) fields from `ExceptionFrame`
- Updated all 26 `vm.cpp` references to use array+index operations
- Updated `aot_runtime.cpp` helpers then deleted them (dead code in AOT)
- Updated `todo.md` status

---

### ✅ Phase 14: Type-Annotated Assignments — EASY ✅

**Done**: `OP_SET_LOCAL_TYPED` inlines tag comparison against expected type via `icmp` on tag bits. Error path (type mismatch) calls `aot_reportTypeError` helper (string generation). Global/define typed still call helpers (hash map access).

**Changed**: `aot_setLocalTyped` → `aot_reportTypeError` (error-only reporter, called only on fail path)
**Removed**: `aotSetLocalTypedFunc` (replaced by `aotReportTypeErrorFunc`)
**Kept**: `aotSetGlobalTypedFunc`, `aotDefineTypedGlobalFunc` (hash map lookup/store)

---

### ✅ Phase 15: Safe Mode Validation — EASY ✅

**Done**: Variable validation (`OP_VALIDATE_SAFE_VARIABLE` / `OP_VALIDATE_SAFE_FILE_VARIABLE`) inlined entirely — error message constructed at compile time and passed to `aotRuntimeErrorFunc` directly. Function validation helpers kept (iterate function declaration params — complex).

**Removed**: `aotValidateSafeVarFunc`, `aotValidateSafeFileVarFunc`
**Kept**: `aotValidateSafeFuncFunc`, `aotValidateSafeFileFuncFunc` (param iteration is complex)

---

### ✅ Phase 16: Method Invocation (`OP_INVOKE`) — HARD ✅

| Helper | What we did |
|--------|-------------|
| `aot_invoke` | Kept as fallback — covers non-Instance receivers, Array push/pop, non-AOT methods, edge cases |
| `aot_tryDirectInvoke` | **Added** — fast path: does full method lookup then calls AOT-compiled function directly via `s_llvmFuncTable` (avoids `vm->call` overhead) |

**Done**: Added `aot_tryDirectInvoke` runtime helper that performs method lookup and calls the resolved AOT-compiled function directly (bypassing `vm->call`). Updated OP_INVOKE codegen to use try-direct + fallback pattern with phi merge (mirrors OP_CALL's architecture). Falls back to `aot_invoke` for non-AOT methods, Array push/pop, and edge cases.

---

### Phase 17: Module Linking (C++ bitcode → LTO) — INFRASTRUCTURE

Compile each C++ module to LLVM bitcode (`-flto -emit-llvm -c`), emit direct `extern "C"` declarations in IR for known module functions, and LTO-link everything.

| # | Task |
|---|------|
| 17.1 | Module interface audit ✅ *Done* |
| 17.2 | CMake rules to build `.bc` files per module |
| 17.3 | Emit `declare i64 @math_sqrt(ptr, i64)` etc. in `llvm_codegen.cpp` |
| 17.4 | Thin wrappers to unpack/repack NaN-boxed values |
| 17.5 | Replace system linker with `lld` / `gcc -flto` |
| 17.6 | Drop entries from `nonAotModules` as they gain AOT support |
| 17.7 | Full `--aot` test pass — 128 tests, identical to interpreter |

---

## Quick Reference: Call Count by Phase

| Phase | Helpers to remove | Helpers to keep |
|-------|------------------|-----------------|
| 7 (Print) ✅ | `aotPrintDoubleNumberFunc`, `aotPrintStringObjFunc` | `aotPrintFunc` (fallback) |
| 8 (Prop cache) ✅ | `aotTryGetCachedPropFunc`, `aotTrySetCachedPropFunc` | `aotGetPropCachedFunc`, `aotSetPropCachedFunc` |
| 9 (Array index) ✅ | `aotArrayGetCachedFunc`, `aotArraySetCachedFunc` | `aotIndexGetFunc`, `aotIndexSetFunc` |
| 10 (String) ✅ | — | `aotInternFunc` (cache miss still calls it) |
| 11 (Create) ✅ | `aotCreateArrayFunc` → `aotAllocArrayFunc` | `aotCreateObjectFunc` (deferred) |
| 12 (For/Spread) ✅ | `aotForInNextFunc`, `aotSpreadFunc` | `aotForInInitFunc` |
| 13 (EH) ✅ | `aotTryPushFunc`, `aotTryPopFunc` (removed, dead code) | `aotThrowErrorFunc` (cold path) |
| 14 (Typed) ✅ | `aotSetLocalTypedFunc` (+ added `aotReportTypeErrorFunc`) | `aotSetGlobalTypedFunc`, `aotDefineTypedGlobalFunc` |
| 15 (Safe) ✅ | `aotValidateSafeVarFunc`, `aotValidateSafeFileVarFunc` | `aotValidateSafeFuncFunc`, `aotValidateSafeFileFuncFunc` |
| 16 (Invoke) ✅ | `aotInvokeFunc` (now fallback-only) | `aotTryDirectInvokeFunc` (added), `aotInvokeFunc` (kept) |
| 17 (Module) | — | — (new code) |

**Total removals**: ~27 function declarations + `Function::Create` calls

---

## Key Decisions (unchanged)

1. **NaN-boxing** (`i64`) from the start — no `Value` struct in LLVM IR.
2. **Emit `.o` files** via `TargetMachine::addPassesToEmitFile()` with `CGFT_ObjectFile`.
3. **Two-pass codegen** — collect blocks & signatures, then emit IR.
4. **No dependency on `proton/`** — LLVM replaces both QBE and C++ string codegen.

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
