#include "neutron.h"
#include "vm.h"
#include "capi.h"

// A C++ class that wraps a C native function
neutron::Value CNativeFn::call(neutron::VM& vm, std::vector<neutron::Value> args) {

    // Create temporary argument copies on heap
    std::vector<neutron::Value*> temp_arg_ptrs;
    std::vector<NeutronValue*> c_args;
    
    for (const auto& arg : args) {
        neutron::Value* temp_arg = new neutron::Value(arg);
        temp_arg_ptrs.push_back(temp_arg);
        c_args.push_back(reinterpret_cast<NeutronValue*>(temp_arg));
    }

    // Call the C function
    NeutronValue* result = function(reinterpret_cast<NeutronVM*>(&vm), args.size(), c_args.data());

    // Copy the result value to avoid cross-library memory deallocation issues
    neutron::Value cpp_result = *reinterpret_cast<neutron::Value*>(result);
    
    // Clean up our temporary argument allocations
    for (auto ptr : temp_arg_ptrs) {
        delete ptr;
    }
    
    // Clean up the result pointer allocated by the C function
    // The value has been copied, so it's safe to delete the pointer
    delete reinterpret_cast<neutron::Value*>(result);
    
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
        case neutron::ValueType::STRING: return NEUTRON_STRING;
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
    return to_cpp_value(value)->type == neutron::ValueType::STRING;
}

bool neutron_get_boolean(NeutronValue* value) {
    return std::get<bool>(to_cpp_value(value)->as);
}

double neutron_get_number(NeutronValue* value) {
    return std::get<double>(to_cpp_value(value)->as);
}

const char* neutron_get_string(NeutronValue* value, size_t* length) {
    const std::string& str = std::get<std::string>(to_cpp_value(value)->as);
    *length = str.length();
    return str.c_str();
}

NeutronValue* neutron_new_nil() {
    return to_c_value(new neutron::Value());
}

NeutronValue* neutron_new_boolean(bool value) {
    return to_c_value(new neutron::Value(value));
}

NeutronValue* neutron_new_number(double value) {
    return to_c_value(new neutron::Value(value));
}

NeutronValue* neutron_new_string(const char* chars, size_t length) {
    return to_c_value(new neutron::Value(std::string(chars, length)));
}

// --- Native Functions ---

void neutron_define_native(NeutronVM* vm, const char* name, NeutronNativeFn function, int arity) {
    neutron::VM* cpp_vm = reinterpret_cast<neutron::VM*>(vm);
    cpp_vm->define_native(name, new CNativeFn(function, arity));
}

} // extern "C"
