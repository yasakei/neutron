#include "libs/convert/native.h"
#include <bitset>

namespace neutron {

Value native_char_to_int(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING || arguments[0].as.string->length() != 1) {
        throw std::runtime_error("Expected a single-character string argument for char_to_int().");
    }
    return Value(static_cast<double>((*arguments[0].as.string)[0]));
}

Value native_int_to_char(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected an integer argument for int_to_char().");
    }
    return Value(std::string(1, static_cast<char>(arguments[0].as.number)));
}

Value native_string_get_char_at(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::STRING || arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected string and integer arguments for string_get_char_at().");
    }
    std::string str = *arguments[0].as.string;
    int index = static_cast<int>(arguments[1].as.number);
    if (index < 0 || index >= static_cast<int>(str.length())) {
        throw std::runtime_error("String index out of bounds.");
    }
    return Value(std::string(1, str[index]));
}

Value native_string_length(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected a string argument for string_length().");
    }
    return Value(static_cast<double>(arguments[0].as.string->length()));
}

Value native_string_to_int(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected a string argument for int().");
    }
    try {
        return Value(std::stod(*arguments[0].as.string));
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
    snprintf(buffer, sizeof(buffer), "%.15g", arguments[0].as.number);
    return Value(std::string(buffer));
}

Value native_bin_to_int(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected a string argument for bin_to_int().");
    }
    try {
        return Value(static_cast<double>(std::stoi(*arguments[0].as.string, nullptr, 2)));
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
    return Value(std::bitset<32>(static_cast<int>(arguments[0].as.number)).to_string());
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

}
