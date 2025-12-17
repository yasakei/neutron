#ifndef neutron_h
#define neutron_h

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque pointers for Neutron data structures
typedef struct NeutronVM NeutronVM;
typedef struct NeutronValue NeutronValue;

#ifdef _WIN32
    #ifdef BUILDING_NEUTRON
        #define NEUTRON_API __declspec(dllexport)
    #else
        #define NEUTRON_API __declspec(dllimport)
    #endif
#else
    #define NEUTRON_API
#endif

// --- Value Management ---

// Value types
typedef enum {
    NEUTRON_NIL,
    NEUTRON_BOOLEAN,
    NEUTRON_NUMBER,
    NEUTRON_STRING
} NeutronType;

// Type checking
// Type checking
NEUTRON_API NeutronType neutron_get_type(NeutronValue* value);
NEUTRON_API bool neutron_is_nil(NeutronValue* value);
NEUTRON_API bool neutron_is_boolean(NeutronValue* value);
NEUTRON_API bool neutron_is_number(NeutronValue* value);
NEUTRON_API bool neutron_is_string(NeutronValue* value);

// Value getters
NEUTRON_API bool neutron_get_boolean(NeutronValue* value);
NEUTRON_API double neutron_get_number(NeutronValue* value);
NEUTRON_API const char* neutron_get_string(NeutronValue* value, size_t* length);

// Value creators
NEUTRON_API NeutronValue* neutron_new_nil();
NEUTRON_API NeutronValue* neutron_new_boolean(bool value);
NEUTRON_API NeutronValue* neutron_new_number(double value);
NEUTRON_API NeutronValue* neutron_new_string(NeutronVM* vm, const char* chars, size_t length);

// --- Native Functions ---

// The signature for a native C function
typedef NeutronValue* (*NeutronNativeFn)(NeutronVM* vm, int arg_count, NeutronValue** args);

// Function for registering a native function
NEUTRON_API void neutron_define_native(NeutronVM* vm, const char* name, NeutronNativeFn function, int arity);

#ifdef __cplusplus
}
#endif

#endif // neutron_h
