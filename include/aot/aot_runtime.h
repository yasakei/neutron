#ifndef NEUTRON_AOT_RUNTIME_H
#define NEUTRON_AOT_RUNTIME_H

#include <cstdint>
#include <cstring>

#ifdef __cplusplus
#include <cstddef>
extern "C" {
#endif

// Per-callsite property cache for inline caching in AOT
// Stores the klass+inlineIndex so the fast path can skip string hash lookup
#ifdef __cplusplus
namespace neutron { struct AotPropCache {
    void* klass;         // cached Class* (null = empty)
    uint8_t inlineIndex; // cached inline field index (0-3)
    uint8_t _pad[7];     // padding to 16 bytes
}; }
using AotPropCache = neutron::AotPropCache;
#else
typedef struct {
    void* klass;
    uint8_t inlineIndex;
    uint8_t _pad[7];
} AotPropCache;
#endif

// NaN-boxing constants (mirrors llvm_codegen.cpp)
// Tagged NaN base: 0x7FFC000000000000 (quiet NaN with bit 50 set)
// Tag in bits 49:47 (3 bits, 8 tag values)
// Payload in bits 46:0 (47 bits)
// Numbers are raw double bits (not NaN-boxed)
static const uint64_t AOT_NAN_BASE = 0x7FFC000000000000ULL;
static const uint64_t AOT_NAN_MASK = 0x7FFC000000000000ULL;
static const uint64_t AOT_NAN_PAYLOAD_MASK = 0x7FFFFFFFFFFFULL;

enum AotNanTag : uint64_t {
    AOT_NAN_NIL = 0,
    AOT_NAN_BOOL = 1,
    AOT_NAN_STRING = 2,
    AOT_NAN_ARRAY = 3,
    AOT_NAN_INSTANCE = 4,
    AOT_NAN_CALLABLE = 5,
    AOT_NAN_OBJECT = 6
};

static inline bool aot_isTagged(uint64_t val) {
    return (val & AOT_NAN_MASK) == AOT_NAN_BASE;
}

static inline uint64_t aot_getTag(uint64_t val) {
    return (val >> 47) & 0x7;
}

// Sentinel for "cache miss" — tagged value with tag=7 (unused) and payload=0
// No normal operation produces this value.
static const uint64_t AOT_SENTINEL = 0x7FFE380000000000ULL;

// Convert between NaN-boxed values and runtime Value structs
// These are used by both the runtime helpers and the codegen

// extern "C" helpers called from AOT-compiled code
// All take a VM context pointer as the first argument

// Property access
uint64_t aot_getProperty(void* vm_ctx, uint64_t objVal, const char* propName);
void    aot_setProperty(void* vm_ctx, uint64_t objVal, const char* propName, uint64_t val);

// Cached property access (AOT inline cache)
// cachePtr points to a per-callsite AotPropCache struct
// On hit: fast inline field access. On miss: full lookup + cache update.
uint64_t aot_getPropertyCached(void* vm_ctx, uint64_t objVal, const char* propName, void* cachePtr);
void    aot_setPropertyCached(void* vm_ctx, uint64_t objVal, const char* propName, uint64_t val, void* cachePtr);

// Addition (handles both numeric and string concatenation)
uint64_t aot_add(void* vm_ctx, uint64_t a, uint64_t b);

// Index access
uint64_t aot_indexGet(void* vm_ctx, uint64_t objVal, uint64_t indexVal);
void     aot_indexSet(void* vm_ctx, uint64_t objVal, uint64_t indexVal, uint64_t val);

// Array/Object creation
// aot_allocArray: allocate Array, pre-reserve capacity, return raw Array* pointer
void* aot_allocArray(void* vm_ctx, uint8_t count);
// keys/values: interleaved NaN-boxed values from stack (key0, val0, ..., keyN, valN)
uint64_t aot_createObject(void* vm_ctx, const uint64_t* keys, const uint64_t* values, uint8_t count);

// General function call (callee(args))
uint64_t aot_call(void* vm_ctx, uint64_t callee, const uint64_t* args, uint8_t argCount);

// Method invocation (receiver.method(args))
uint64_t aot_invoke(void* vm_ctx, uint64_t receiver, const char* methodName,
                    const uint64_t* args, uint8_t argCount);
// Fast path: try AOT-compiled method directly (avoids vm->call overhead)
uint64_t aot_tryDirectInvoke(void* vm_ctx, uint64_t receiver, const char* methodName,
                              const uint64_t* args, uint8_t argCount);

// For-in loop helper
// aot_forInInit: takes iterable, returns NaN-boxed keys array
uint64_t aot_forInInit(void* vm_ctx, uint64_t iterableVal);

// Reports a runtime error from a NaN-boxed exception value (for OP_THROW)
void aot_throwError(void* vm_ctx, uint64_t exceptionVal);
// Reports a runtime error from a C string message
void aot_runtimeError(void* vm_ctx, const char* message);

// Safe-mode validation helpers
void aot_validateSafeFunction(void* vm_ctx, uint64_t funcVal, int isSafeFile);
void aot_validateSafeFileFunction(void* vm_ctx, uint64_t funcVal);

// Typed set helpers
void aot_reportTypeError(void* vm_ctx, uint8_t expectedType, uint64_t val);
void aot_setGlobalTyped(void* vm_ctx, const char* name, uint64_t val);
void aot_defineTypedGlobal(void* vm_ctx, const char* name, uint64_t val, uint8_t typeByte);

// Global variable access (via VM globals table for object types)
uint64_t aot_getGlobal(void* vm_ctx, const char* name);
void     aot_setGlobal(void* vm_ctx, const char* name, uint64_t val);

// Print a NaN-boxed value (handles all types including strings, arrays, objects)
void aot_printValue(void* vm_ctx, uint64_t val);

// Intern a string and return NaN-boxed value
uint64_t aot_internString(void* vm_ctx, const char* str);

// ---- Focused helpers (take raw pointers, avoid nanToValue dispatch) ----

// Property cache: try fast path. Returns inline field value or AOT_SENTINEL.
uint64_t aot_tryGetCachedProp(void* inst, void* cache);

// Property set cache: try fast path. Returns 1 on hit, 0 on miss.
uint8_t  aot_trySetCachedProp(void* inst, void* cache, uint64_t val);

// Array index get: returns element or nil.
uint64_t aot_arrayGetCached(void* arr, uint64_t idxVal);

// Array index set: elements[idx] = val (no-op if out of bounds).
void     aot_arraySetCached(void* arr, uint64_t idxVal, uint64_t val);

// String char at: returns char as NaN-boxed string value, or nil if OOB.
uint64_t aot_stringCharAt(void* vm_ctx, uint64_t strVal, uint64_t idxVal);

// ---- Phase 6: Direct native function call support ----

// Register a compiled LLVM function pointer for a given function index.
void     aot_registerLlvmFunc(int idx, void* funcPtr);

// Try to call a pre-compiled LLVM function directly.
// Returns the NaN-boxed result, or AOT_SENTINEL if not possible (fall back to aot_call).
uint64_t aot_tryDirectCall(void* vm_ctx, uint64_t callee, const uint64_t* args, uint8_t argCount);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NEUTRON_AOT_RUNTIME_H
