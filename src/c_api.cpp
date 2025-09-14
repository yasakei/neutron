#include "c_api.h"
#include "vm.h"

extern "C" {

CValueType c_value_get_type(const struct CValue* value) {
    const neutron::Value* v = reinterpret_cast<const neutron::Value*>(value);
    return static_cast<CValueType>(v->type);
}

bool c_value_get_boolean(const struct CValue* value) {
    const neutron::Value* v = reinterpret_cast<const neutron::Value*>(value);
    return v->as.boolean;
}

double c_value_get_number(const struct CValue* value) {
    const neutron::Value* v = reinterpret_cast<const neutron::Value*>(value);
    return v->as.number;
}

const char* c_value_get_string(const struct CValue* value) {
    const neutron::Value* v = reinterpret_cast<const neutron::Value*>(value);
    return v->as.string->c_str();
}

size_t c_object_get_size(const struct CValue* value) {
    const neutron::Value* v = reinterpret_cast<const neutron::Value*>(value);
    neutron::JsonObject* obj = static_cast<neutron::JsonObject*>(v->as.object);
    return obj->properties.size();
}

const char* c_object_get_key_at(const struct CValue* value, size_t index) {
    const neutron::Value* v = reinterpret_cast<const neutron::Value*>(value);
    neutron::JsonObject* obj = static_cast<neutron::JsonObject*>(v->as.object);
    auto it = obj->properties.begin();
    std::advance(it, index);
    return it->first.c_str();
}

const struct CValue* c_object_get_value_at(const struct CValue* value, size_t index) {
    const neutron::Value* v = reinterpret_cast<const neutron::Value*>(value);
    neutron::JsonObject* obj = static_cast<neutron::JsonObject*>(v->as.object);
    auto it = obj->properties.begin();
    std::advance(it, index);
    return reinterpret_cast<const CValue*>(&it->second);
}

size_t c_array_get_size(const struct CValue* value) {
    const neutron::Value* v = reinterpret_cast<const neutron::Value*>(value);
    neutron::JsonArray* arr = static_cast<neutron::JsonArray*>(v->as.object);
    return arr->elements.size();
}

const struct CValue* c_array_get_element_at(const struct CValue* value, size_t index) {
    const neutron::Value* v = reinterpret_cast<const neutron::Value*>(value);
    neutron::JsonArray* arr = static_cast<neutron::JsonArray*>(v->as.object);
    return reinterpret_cast<const CValue*>(&arr->elements[index]);
}

}