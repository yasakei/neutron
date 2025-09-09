#include "libs/math/native.h"
#include <cmath>

namespace neutron {

Value math_add(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for add().");
    }
    if (arguments[0].type != ValueType::NUMBER || arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Arguments for add() must be numbers.");
    }
    return Value(arguments[0].as.number + arguments[1].as.number);
}

Value math_subtract(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for subtract().");
    }
    if (arguments[0].type != ValueType::NUMBER || arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Arguments for subtract() must be numbers.");
    }
    return Value(arguments[0].as.number - arguments[1].as.number);
}

Value math_multiply(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for multiply().");
    }
    if (arguments[0].type != ValueType::NUMBER || arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Arguments for multiply() must be numbers.");
    }
    return Value(arguments[0].as.number * arguments[1].as.number);
}

Value math_divide(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for divide().");
    }
    if (arguments[0].type != ValueType::NUMBER || arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Arguments for divide() must be numbers.");
    }
    if (arguments[1].as.number == 0) {
        throw std::runtime_error("Division by zero.");
    }
    return Value(arguments[0].as.number / arguments[1].as.number);
}

Value math_pow(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for pow().");
    }
    if (arguments[0].type != ValueType::NUMBER || arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Arguments for pow() must be numbers.");
    }
    return Value(pow(arguments[0].as.number, arguments[1].as.number));
}

Value math_sqrt(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for sqrt().");
    }
    if (arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Argument for sqrt() must be a number.");
    }
    return Value(sqrt(arguments[0].as.number));
}

Value math_abs(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for abs().");
    }
    if (arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Argument for abs() must be a number.");
    }
    return Value(static_cast<double>(abs(arguments[0].as.number)));
}

void register_math_functions(std::shared_ptr<Environment> env) {
    env->define("add", Value(new NativeFn(math_add, 2)));
    env->define("subtract", Value(new NativeFn(math_subtract, 2)));
    env->define("multiply", Value(new NativeFn(math_multiply, 2)));
    env->define("divide", Value(new NativeFn(math_divide, 2)));
    env->define("pow", Value(new NativeFn(math_pow, 2)));
    env->define("sqrt", Value(new NativeFn(math_sqrt, 1)));
    env->define("abs", Value(new NativeFn(math_abs, 1)));
}

}
