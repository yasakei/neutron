#include "neutron.h"
#include "vm.h"
#include "capi.h"
#include "types/obj_string.h"
#include <cstring>
#include <vector>

// Thread-local storage for return values to avoid cross-library memory issues
// Each thread gets its own return value slot that persists between calls
static thread_local neutron::Value tls_return_value;

// Thread-local storage for arguments to avoid cross-library memory issues
static thread_local std::vector<neutron::Value> tls_args;

// A C++ class that wraps a C native function
neutron::Value CNativeFn::call(neutron::VM& vm, std::vector<neutron::Value> args) {

    // Use thread-local storage for arguments to avoid heap allocation/deallocation
    // that can cause issues across shared library boundaries
    tls_args = args;
    
    std::vector<NeutronValue*> c_args;
    for (auto& arg : tls_args) {
        c_args.push_back(reinterpret_cast<NeutronValue*>(&arg));
    }

    // Call the C function
    NeutronValue* result = function(reinterpret_cast<NeutronVM*>(&vm), args.size(), c_args.data());

    // Copy the result value
    neutron::Value cpp_result = *reinterpret_cast<neutron::Value*>(result);
    
    // No cleanup needed - tls_args persists and result points to tls_return_value
    
    return cpp_result;
}

// --- Type Conversions ---

static neutron::Value* to_cpp_value(NeutronValue* value) {
    return reinterpret_cast<neutron::Value*>(value);
}

static NeutronValue* to_c_value(neutron::Value* value) {
    return reinterpret_cast<NeutronValue*>(value);
}

// --- Value Management ---

extern "C" {

NeutronType neutron_get_type(NeutronValue* value) {
    switch (to_cpp_value(value)->type) {
        case neutron::ValueType::NIL: return NEUTRON_NIL;
        case neutron::ValueType::BOOLEAN: return NEUTRON_BOOLEAN;
        case neutron::ValueType::NUMBER: return NEUTRON_NUMBER;
        case neutron::ValueType::OBJ_STRING: return NEUTRON_STRING;
        default: return NEUTRON_NIL; // Should not happen
    }
}

bool neutron_is_nil(NeutronValue* value) {
    return to_cpp_value(value)->type == neutron::ValueType::NIL;
}

bool neutron_is_boolean(NeutronValue* value) {
    return to_cpp_value(value)->type == neutron::ValueType::BOOLEAN;
}

bool neutron_is_number(NeutronValue* value) {
    return to_cpp_value(value)->type == neutron::ValueType::NUMBER;
}

bool neutron_is_string(NeutronValue* value) {
    return to_cpp_value(value)->type == neutron::ValueType::OBJ_STRING;
}

bool neutron_get_boolean(NeutronValue* value) {
    return std::get<bool>(to_cpp_value(value)->as);
}

double neutron_get_number(NeutronValue* value) {
    return std::get<double>(to_cpp_value(value)->as);
}

const char* neutron_get_string(NeutronValue* value, size_t* length) {
    const std::string& str = std::get<neutron::ObjString*>(to_cpp_value(value)->as)->chars;
    *length = str.length();
    return str.c_str();
}

NeutronValue* neutron_new_nil() {
    tls_return_value = neutron::Value();
    return to_c_value(&tls_return_value);
}

NeutronValue* neutron_new_boolean(bool value) {
    tls_return_value = neutron::Value(value);
    return to_c_value(&tls_return_value);
}

NeutronValue* neutron_new_number(double value) {
    tls_return_value = neutron::Value(value);
    return to_c_value(&tls_return_value);
}

NeutronValue* neutron_new_string(NeutronVM* vm, const char* chars, size_t length) {
    neutron::VM* cpp_vm = reinterpret_cast<neutron::VM*>(vm);
    tls_return_value = neutron::Value(cpp_vm->allocate<neutron::ObjString>(std::string(chars, length)));
    return to_c_value(&tls_return_value);
}

// --- Native Functions ---

void neutron_define_native(NeutronVM* vm, const char* name, NeutronNativeFn function, int arity) {
    neutron::VM* cpp_vm = reinterpret_cast<neutron::VM*>(vm);
    cpp_vm->define_native(name, new CNativeFn(function, arity));
}

} // extern "C"
