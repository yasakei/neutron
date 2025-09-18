#include "neutron.h"
#include "vm.h"

// A C++ class that wraps a C native function
class CNativeFn : public neutron::Callable {
public:
    CNativeFn(NeutronNativeFn function, int arity)
        : function(function), _arity(arity) {
        printf("CNativeFn constructor\n");
    }

    ~CNativeFn() {
        printf("CNativeFn destructor\n");
    }

    int arity() override { return _arity; }

    neutron::Value call(neutron::VM& vm, std::vector<neutron::Value> args) override {

        // This is still unsafe. A proper implementation would need a more robust
        // way to manage the memory of the arguments and return value.
        std::vector<NeutronValue*> c_args;
        for (const auto& arg : args) {
            c_args.push_back(reinterpret_cast<NeutronValue*>(new neutron::Value(arg)));
        }

        NeutronValue* result = function(reinterpret_cast<NeutronVM*>(&vm), args.size(), c_args.data());

        neutron::Value cpp_result = *reinterpret_cast<neutron::Value*>(result);

        // Clean up the allocated memory
        for (auto arg : c_args) {
            delete reinterpret_cast<neutron::Value*>(arg);
        }
        delete reinterpret_cast<neutron::Value*>(result);

        return cpp_result;
    }

    std::string toString() override { return "<native fn>"; }

private:
    NeutronNativeFn function;
    int _arity;
};

// --- Type Conversions ---

static neutron::Value* to_cpp_value(NeutronValue* value) {
    return reinterpret_cast<neutron::Value*>(value);
}

static NeutronValue* to_c_value(neutron::Value* value) {
    return reinterpret_cast<NeutronValue*>(value);
}

// --- Value Management ---

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
