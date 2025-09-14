#ifndef NEUTRON_C_API_H
#define NEUTRON_C_API_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque pointer to the C++ Value object
struct CValue;

typedef enum {
    C_NIL,
    C_BOOLEAN,
    C_NUMBER,
    C_STRING,
    C_OBJECT,
    C_ARRAY,
    C_FUNCTION,
    C_MODULE
} CValueType;

CValueType c_value_get_type(const struct CValue* value);
bool c_value_get_boolean(const struct CValue* value);
double c_value_get_number(const struct CValue* value);
const char* c_value_get_string(const struct CValue* value);

// API for JsonObject
size_t c_object_get_size(const struct CValue* value);
const char* c_object_get_key_at(const struct CValue* value, size_t index);
const struct CValue* c_object_get_value_at(const struct CValue* value, size_t index);

// API for JsonArray
size_t c_array_get_size(const struct CValue* value);
const struct CValue* c_array_get_element_at(const struct CValue* value, size_t index);


#ifdef __cplusplus
}
#endif

#endif // NEUTRON_C_API_H
