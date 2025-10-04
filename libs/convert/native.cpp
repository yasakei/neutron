#include "native.h"
#include "vm.h"
#include <bitset>

namespace neutron {

Value native_char_to_int(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING || std::get<std::string>(arguments[0].as).length() != 1) {
        throw std::runtime_error("Expected a single-character string argument for char_to_int().");
    }
    return Value(static_cast<double>(std::get<std::string>(arguments[0].as)[0]));
}

Value native_int_to_char(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected an integer argument for int_to_char().");
    }
    return Value(std::string(1, static_cast<char>(std::get<double>(arguments[0].as))));
}

Value native_string_get_char_at(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::STRING || arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected string and integer arguments for string_get_char_at().");
    }
    std::string str = std::get<std::string>(arguments[0].as);
    int index = static_cast<int>(std::get<double>(arguments[1].as));
    if (index < 0 || index >= static_cast<int>(str.length())) {
        throw std::runtime_error("String index out of bounds.");
    }
    return Value(std::string(1, str[index]));
}

Value native_string_length(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected a string argument for string_length().");
    }
    return Value(static_cast<double>(std::get<std::string>(arguments[0].as).length()));
}

Value native_string_to_int(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected a string argument for int().");
    }
    
    std::string str = std::get<std::string>(arguments[0].as);
    
    // Handle empty string - return 0
    if (str.empty()) {
        return Value(0.0);
    }
    
    try {
        return Value(std::stod(str));
    } catch (const std::invalid_argument& ia) {
        throw std::runtime_error("Invalid string format for int().");
    } catch (const std::out_of_range& oor) {
        throw std::runtime_error("String value out of range for int().");
    }
}

Value native_int_to_string(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected a number argument for str().");
    }
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.15g", std::get<double>(arguments[0].as));
    return Value(std::string(buffer));
}

Value native_bin_to_int(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected a string argument for bin_to_int().");
    }
    try {
        return Value(static_cast<double>(std::stoi(std::get<std::string>(arguments[0].as), nullptr, 2)));
    } catch (const std::invalid_argument& ia) {
        throw std::runtime_error("Invalid binary string format for bin_to_int().");
    } catch (const std::out_of_range& oor) {
        throw std::runtime_error("Binary string value out of range for bin_to_int().");
    }
}

Value native_int_to_bin(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected a number argument for int_to_bin().");
    }
    return Value(std::bitset<32>(static_cast<int>(std::get<double>(arguments[0].as))).to_string());
}

void register_convert_functions(std::shared_ptr<Environment> env) {
    env->define("char_to_int", Value(new NativeFn(native_char_to_int, 1)));
    env->define("int_to_char", Value(new NativeFn(native_int_to_char, 1)));
    env->define("string_get_char_at", Value(new NativeFn(native_string_get_char_at, 2)));
    env->define("string_length", Value(new NativeFn(native_string_length, 1)));
    env->define("int", Value(new NativeFn(native_string_to_int, 1)));
    env->define("str", Value(new NativeFn(native_int_to_string, 1)));
    env->define("bin_to_int", Value(new NativeFn(native_bin_to_int, 1)));
    env->define("int_to_bin", Value(new NativeFn(native_int_to_bin, 1)));
}

} // namespace neutron

extern "C" void neutron_init_convert_module(neutron::VM* vm) {
    auto convert_env = std::make_shared<neutron::Environment>();
    neutron::register_convert_functions(convert_env);
    auto convert_module = new neutron::Module("convert", convert_env);
    vm->define_module("convert", convert_module);
}
