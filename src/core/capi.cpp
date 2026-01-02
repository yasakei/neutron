#include "neutron.h"
#include "vm.h"
#include "capi.h"
#include "types/obj_string.h"
#include <cstring>
#include <vector>

// Thread-local storage for return values to avoid cross-library memory issues
// Each thread gets its own return value slot that persists between calls
static thread_local neutron::Value tls_return_value;

// A C++ class that wraps a C native function
neutron::Value CNativeFn::call(neutron::VM& vm, std::vector<neutron::Value> args) {

    // Use stack allocation for arguments - simpler and avoids TLS cleanup issues
    std::vector<NeutronValue*> c_args;
    c_args.reserve(args.size());
    
    for (auto& arg : args) {
        c_args.push_back(reinterpret_cast<NeutronValue*>(&arg));
    }

    // Call the C function
    NeutronValue* result = function(reinterpret_cast<NeutronVM*>(&vm), args.size(), c_args.data());

    // Copy the result value
    neutron::Value cpp_result = *reinterpret_cast<neutron::Value*>(result);
    
    // result points to tls_return_value which persists
    
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
    return to_cpp_value(value)->as.boolean;
}

double neutron_get_number(NeutronValue* value) {
    return to_cpp_value(value)->as.number;
}

const char* neutron_get_string(NeutronValue* value, size_t* length) {
    const std::string& str = to_cpp_value(value)->as.obj_string->chars;
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
    tls_return_value = neutron::Value(cpp_vm->internString(std::string(chars, length)));
    return to_c_value(&tls_return_value);
}

// --- Native Functions ---

void neutron_define_native(NeutronVM* vm, const char* name, NeutronNativeFn function, int arity) {
    neutron::VM* cpp_vm = reinterpret_cast<neutron::VM*>(vm);
    cpp_vm->define_native(name, new CNativeFn(function, arity));
}

} // extern "C"
