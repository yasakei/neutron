#ifndef NEUTRON_AOT_RUNTIME_H
#define NEUTRON_AOT_RUNTIME_H

#include <cstdint>
#include <cstring>

#ifdef __cplusplus
extern "C" {
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

// Convert between NaN-boxed values and runtime Value structs
// These are used by both the runtime helpers and the codegen

// extern "C" helpers called from AOT-compiled code
// All take a VM context pointer as the first argument

// Property access
uint64_t aot_getProperty(void* vm_ctx, uint64_t objVal, const char* propName);
void    aot_setProperty(void* vm_ctx, uint64_t objVal, const char* propName, uint64_t val);

// Index access
uint64_t aot_indexGet(void* vm_ctx, uint64_t objVal, uint64_t indexVal);
void     aot_indexSet(void* vm_ctx, uint64_t objVal, uint64_t indexVal, uint64_t val);

// Array/Object creation
// elements: pointer to array of count NaN-boxed uint64_t values
uint64_t aot_createArray(void* vm_ctx, const uint64_t* elements, uint8_t count);
// keys/values: interleaved NaN-boxed values from stack (key0, val0, ..., keyN, valN)
uint64_t aot_createObject(void* vm_ctx, const uint64_t* keys, const uint64_t* values, uint8_t count);

// Method invocation (receiver.method(args))
uint64_t aot_invoke(void* vm_ctx, uint64_t receiver, const char* methodName,
                    const uint64_t* args, uint8_t argCount);

// Global variable access (via VM globals table for object types)
uint64_t aot_getGlobal(void* vm_ctx, const char* name);
void     aot_setGlobal(void* vm_ctx, const char* name, uint64_t val);

// Print a NaN-boxed value (handles all types including strings, arrays, objects)
void aot_printValue(void* vm_ctx, uint64_t val);

// Intern a string and return NaN-boxed value
uint64_t aot_internString(void* vm_ctx, const char* str);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NEUTRON_AOT_RUNTIME_H
