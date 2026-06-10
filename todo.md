# LLVM AOT Backend Migration

## Status
✅ **Phases 0–6 complete** — All bytecode opcodes emit LLVM IR; user-defined functions compile to native IR and dispatch directly (no interpreter fallback for sub-function calls). 128/128 interpreter + 10/10 AOT tests pass.

❌ **Phases 7–17** — Every non-trivial operation still calls a C++ runtime helper. The IR handles tag dispatch, control flow, arithmetic, and stack management; the actual work (hash lookups, GC allocation, bounds checks, printing) goes through `extern "C"` calls. See below for the breakdown.

---

## ✅ Completed — Phases 0–6

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

### Phase 7: Value Printing (`OP_SAY`) — EASY

IR already does full tag dispatch (number / nil / true / false / string / fallback). Each branch calls a helper — inline them.

| Helper | Lines | What it does | Inline approach |
|--------|-------|-------------|-----------------|
| `aot_printDoubleNumber` | 2218 | `printf("%g\n")` on raw double bits | Call `printf` via `declare i32 @printf(i8*, ...)` directly in IR |
| `aot_printStringObj` | 2260 | `fwrite(chars, 1, size, stdout)` | Extract ObjString data pointer + length, call `fwrite` |
| `aot_printValue` (fallback) | 2264 | `toString()` + `puts` for Array/Instance/Callable/Object | Extract payload pointer, call `->toString()` virtual method, then `puts` |

**Remove**: `aotPrintDoubleNumberFunc`, `aotPrintStringObjFunc`, `aotPrintFunc`

---

### Phase 8: Property Access Inline Cache — EASY

IR already checks `tag == Instance`, extracts `inst*` from payload, and calls the cached probe. The fast path (klass match) just does a GEP load/store — do that in IR.

| Helper | Lines | What it does | Inline approach |
|--------|-------|-------------|-----------------|
| `aot_tryGetCachedProp` | 1791 | If `cache->klass == inst->klass`, return `inst->inlineFields[idx]` | Emit `icmp` on klass pointers, `GEP` into `inlineFields`, `load` directly |
| `aot_trySetCachedProp` | 1844 | If klass matches, `inst->inlineFields[idx] = val`, return 1 | Same — `icmp` + `GEP` + `store` in IR |
| `aot_getPropertyCached` (miss handler) | 1798, 1998 | Full hash lookup — scans klass field table, checks JsonObject, Array.length, etc. | Keep as helper call — only on cache miss |
| `aot_setPropertyCached` (miss handler) | 1851 | Full set with cache update | Keep as helper call — only on cache miss |

**Result**: Cache hit path has zero helper calls — pure `icmp` + `GEP` + `load`/`store`.
**Remove**: `aotTryGetCachedPropFunc`, `aotTrySetCachedPropFunc`
**Keep**: `aotGetPropCachedFunc`, `aotSetPropCachedFunc` (miss only)

---

### Phase 9: Array Index Access (`OP_INDEX_GET` / `OP_INDEX_SET`) — EASY

IR already dispatches `tag == Array` vs fallback. The Array fast path does a bounds check + element load/store — inline it.

| Helper | Lines | What it does | Inline approach |
|--------|-------|-------------|-----------------|
| `aot_arrayGetCached` | 1699 | Cast raw pointer to `Array*`, `nanToValue` on index, bounds check, `at(i)`, `valueToNan` | `GEP` into array data, `icmp` for bounds, `load` element directly |
| `aot_arraySetCached` | 1745 | Same but `set(i, val)` | Same — `GEP` + bounds check + `store` |
| `aot_indexGet` (fallback) | 1704 | Full dynamic: handles JsonObject key, ObjString charAt | Keep as helper call |
| `aot_indexSet` (fallback) | 1750 | Full dynamic: handles JsonObject key | Keep as helper call |

**Result**: Array index get/set has zero C++ calls on the fast path.
**Remove**: `aotArrayGetCachedFunc`, `aotArraySetCachedFunc`
**Keep**: `aotIndexGetFunc`, `aotIndexSetFunc` (fallback only)

---

### Phase 10: String Interning (`OP_CONSTANT` for strings) — MEDIUM

| Helper | Lines | What it does | Inline approach |
|--------|-------|-------------|-----------------|
| `aot_internString` | 941, 960 | `vm->internString(str)` — hash the string, look up in VM's string table, insert if new | Extract `vm->strings` hash table, inline hash + probe in IR, call `allocate<ObjString>` for insert |

This requires exposing the VM's string table structure to LLVM IR (GEP into the `unordered_map` or custom hash table). Complexity depends on the hash table implementation.

**Fallback**: If the table is too complex to inline, keep as helper call.

---

### Phase 11: Array/Object Creation (`OP_ARRAY` / `OP_OBJECT`) — MEDIUM

| Helper | Lines | What it does | Inline approach |
|--------|-------|-------------|-----------------|
| `aot_createArray` | 1617, 1624 | `allocate<Array>()` + loop `push(elements[i])` | Inline GC `allocate` call (or `malloc` + constructor), then inline the element copy loop |
| `aot_createObject` | 1657, 1663 | `allocate<JsonObject>()` + loop insert key/val pairs | Same approach — GC alloc + inline loop |

Requires inlining `vm->allocate<T>()` which calls the GC. If GC is too complex, consider calling a simplified `aot_gc_alloc(size)` helper instead.

---

### Phase 12: For-In / Spread — MEDIUM

| Helper | Lines | What it does | Inline approach |
|--------|-------|-------------|-----------------|
| `aot_forInInit` | 1913 | Create `Array` of keys from JsonObject or Array | Inline the key collection loop + `allocate<Array>` |
| `aot_forInNext` | 1934 | Return `keys[i]` or nil if done | Inline Array access (reuse Phase 9) |
| `aot_spread` | 2021 | Copy array elements to stack buffer, up to 256 | Inline the copy loop with bounds check |

---

### Phase 13: Exception Handling (`OP_TRY` / `OP_END_TRY` / `OP_THROW`) — HARD

| Helper | Lines | What it does | Inline approach |
|--------|-------|-------------|-----------------|
| `aot_tryPush` | 2145 | Push `ExceptionFrame` (tryEnd, catchStart, finallyStart) onto VM frame stack | Use LLVM `landingpad` + `invoke` — let LLVM handle unwinding natively |
| `aot_tryPop` | 2115 | Pop the frame | LLVM `landingpad` handles this automatically |
| `aot_throwError` | 2094 | Print error + stack trace + `exit(1)` | LLVM `resume` + cleanup handlers |

**This is a complete rewrite** — replaces `setjmp`/`longjmp` with native LLVM EH. Requires `invoke` instead of `call` for every instruction that can throw.

---

### Phase 14: Type-Annotated Assignments — EASY

| Helper | Lines | What it does | Inline approach |
|--------|-------|-------------|-----------------|
| `aot_setLocalTyped` | 1020 | Validate value type against expected type byte | Inline tag comparison against expected type — simple `icmp` on tag bits |
| `aot_setGlobalTyped` | 1473 | Look up global type in `vm->globalTypes`, validate | Inline type lookup + `icmp` |
| `aot_defineTypedGlobal` | 1503 | Store in `vm->globals` + `vm->globalTypes` | Currently stores in VM hashtable — hard to inline. Keep as helper. |

**Remove**: `aotSetLocalTypedFunc`
**Consider**: Keeping `aotSetGlobalTypedFunc`, `aotDefineTypedGlobalFunc` as helpers

---

### Phase 15: Safe Mode Validation — EASY

| Helper | Lines | What it does | Inline approach |
|--------|-------|-------------|-----------------|
| `aot_validateSafeFunction` | 2128 | Check function params/return have type annotations | Inline the checks — iterate param list, check `typeAnnotation.has_value()` |
| `aot_validateSafeFileFunction` | 2136 | Same for `.ntsc` files | Same |
| `aot_validateSafeVariable` | 2163 | Check variable has type annotation | Inline the check — already a simple conditional |
| `aot_validateSafeFileVariable` | 2160 | Same for `.ntsc` files | Same |

**Remove**: All 4 validate function declarations + calls

---

### Phase 16: Method Invocation (`OP_INVOKE`) — HARD

| Helper | Lines | What it does | Inline approach |
|--------|-------|-------------|-----------------|
| `aot_invoke` | 1892, 1900 | Look up method on Instance klass, or handle built-in Array methods | Inline method table lookup (hash on ObjString key), bind receiver, call the method |

Very similar to property get + call. Could be refactored as: inline the method table probe, then emit a direct call to the resolved function pointer.

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
| 7 (Print) | `aotPrintDoubleNumberFunc`, `aotPrintStringObjFunc`, `aotPrintFunc`, `putsFunc` | — |
| 8 (Prop cache) | `aotTryGetCachedPropFunc`, `aotTrySetCachedPropFunc` | `aotGetPropCachedFunc`, `aotSetPropCachedFunc` |
| 9 (Array index) | `aotArrayGetCachedFunc`, `aotArraySetCachedFunc` | `aotIndexGetFunc`, `aotIndexSetFunc` |
| 10 (String) | `aotInternFunc` | — |
| 11 (Create) | `aotCreateArrayFunc`, `aotCreateObjectFunc` | — |
| 12 (For/Spread) | `aotForInInitFunc`, `aotForInNextFunc`, `aotSpreadFunc` | — |
| 13 (EH) | `aotTryPushFunc`, `aotTryPopFunc`, `aotThrowErrorFunc` | — |
| 14 (Typed) | `aotSetLocalTypedFunc` | `aotSetGlobalTypedFunc`, `aotDefineTypedGlobalFunc` |
| 15 (Safe) | All 4 validate functions | — |
| 16 (Invoke) | `aotInvokeFunc` | — |
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
