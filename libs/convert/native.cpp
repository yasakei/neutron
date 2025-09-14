#include "libs/convert/native.h"
#include "libs/convert/convert_ops.h"
#include <string>
#include <vector>

namespace neutron {

Value native_char_to_int(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING || arguments[0].as.string->length() != 1) {
        throw std::runtime_error("Expected a single-character string argument for char_to_int().");
    }
    return Value(char_to_int_c(arguments[0].as.string->c_str()));
}

Value native_int_to_char(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected an integer argument for int_to_char().");
    }
    char* s = int_to_char_c(arguments[0].as.number);
    Value result = Value(std::string(s));
    free(s);
    return result;
}

Value native_string_get_char_at(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::STRING || arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected string and integer arguments for string_get_char_at().");
    }
    char* c = string_get_char_at_c(arguments[0].as.string->c_str(), (int)arguments[1].as.number);
    if (c) {
        Value result = Value(std::string(c));
        free(c);
        return result;
    }
    throw std::runtime_error("String index out of bounds.");
}

Value native_string_length(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected a string argument for string_length().");
    }
    return Value(string_length_c(arguments[0].as.string->c_str()));
}

Value native_string_to_int(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected a string argument for int().");
    }
    return Value(string_to_int_c(arguments[0].as.string->c_str()));
}

Value native_int_to_string(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected a number argument for str().");
    }
    char* s = int_to_string_c(arguments[0].as.number);
    Value result = Value(std::string(s));
    free(s);
    return result;
}

Value native_bin_to_int(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected a string argument for bin_to_int().");
    }
    return Value(bin_to_int_c(arguments[0].as.string->c_str()));
}

Value native_int_to_bin(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected a number argument for int_to_bin().");
    }
    char* s = int_to_bin_c((int)arguments[0].as.number);
    Value result = Value(std::string(s));
    free(s);
    return result;
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