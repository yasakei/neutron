#include "math/native.h"
#include "vm.h"

using namespace neutron;

// Function to add two numbers
static Value add(const std::vector<Value>& args) {
    if (args.size() != 2) {
        // Handle error: incorrect number of arguments
        return Value();
    }
    if (args[0].type != ValueType::NUMBER || args[1].type != ValueType::NUMBER) {
        // Handle error: incorrect argument types
        return Value();
    }
    double a = std::get<double>(args[0].as);
    double b = std::get<double>(args[1].as);
    return Value(a + b);
}

// Function to subtract two numbers
static Value subtract(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return Value();
    }
    if (args[0].type != ValueType::NUMBER || args[1].type != ValueType::NUMBER) {
        return Value();
    }
    double a = std::get<double>(args[0].as);
    double b = std::get<double>(args[1].as);
    return Value(a - b);
}

// Function to multiply two numbers
static Value multiply(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return Value();
    }
    if (args[0].type != ValueType::NUMBER || args[1].type != ValueType::NUMBER) {
        return Value();
    }
    double a = std::get<double>(args[0].as);
    double b = std::get<double>(args[1].as);
    return Value(a * b);
}

// Function to divide two numbers
static Value divide(const std::vector<Value>& args) {
    if (args.size() != 2) {
        return Value();
    }
    if (args[0].type != ValueType::NUMBER || args[1].type != ValueType::NUMBER) {
        return Value();
    }
    double a = std::get<double>(args[0].as);
    double b = std::get<double>(args[1].as);
    if (b == 0) {
        // Handle error: division by zero
        return Value();
    }
    return Value(a / b);
}

void register_math_functions(std::shared_ptr<Environment> env) {
    env->define("add", Value(new CNativeFn(add, 2)));
    env->define("subtract", Value(new CNativeFn(subtract, 2)));
    env->define("multiply", Value(new CNativeFn(multiply, 2)));
    env->define("divide", Value(new CNativeFn(divide, 2)));
}


extern "C" void neutron_init_math_module(VM* vm) {
    auto math_env = std::make_shared<Environment>();
    register_math_functions(math_env);
    auto math_module = new Module("math", math_env);
    vm->define_module("math", math_module);
}
