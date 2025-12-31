/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Public License 1.0.
 * For full license text, see LICENSE file in the root directory.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "native.h"
#include "vm.h"
#include <cmath>

using namespace neutron;

// Function to add two numbers
Value add(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) {
        throw std::runtime_error("add() expects 2 arguments.");
    }
    if (args[0].type != ValueType::NUMBER || args[1].type != ValueType::NUMBER) {
        throw std::runtime_error("add() expects two numbers.");
    }
    double a = std::get<double>(args[0].as);
    double b = std::get<double>(args[1].as);
    return Value(a + b);
}

// Function to subtract two numbers
Value subtract(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) {
        throw std::runtime_error("subtract() expects 2 arguments.");
    }
    if (args[0].type != ValueType::NUMBER || args[1].type != ValueType::NUMBER) {
        throw std::runtime_error("subtract() expects two numbers.");
    }
    double a = std::get<double>(args[0].as);
    double b = std::get<double>(args[1].as);
    return Value(a - b);
}

// Function to multiply two numbers
Value multiply(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) {
        throw std::runtime_error("multiply() expects 2 arguments.");
    }
    if (args[0].type != ValueType::NUMBER || args[1].type != ValueType::NUMBER) {
        throw std::runtime_error("multiply() expects two numbers.");
    }
    double a = std::get<double>(args[0].as);
    double b = std::get<double>(args[1].as);
    return Value(a * b);
}

// Function to divide two numbers
Value divide(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) {
        throw std::runtime_error("divide() expects 2 arguments.");
    }
    if (args[0].type != ValueType::NUMBER || args[1].type != ValueType::NUMBER) {
        throw std::runtime_error("divide() expects two numbers.");
    }
    double a = std::get<double>(args[0].as);
    double b = std::get<double>(args[1].as);
    if (b == 0) {
        throw std::runtime_error("Division by zero.");
    }
    return Value(a / b);
}

// Function to calculate the square root of a number
Value sqrt_fn(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("sqrt() expects 1 argument.");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("sqrt() expects a number.");
    }
    double num = std::get<double>(args[0].as);
    return Value(std::sqrt(num));
}

Value pow_fn(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) {
        throw std::runtime_error("pow() expects 2 arguments.");
    }
    if (args[0].type != ValueType::NUMBER || args[1].type != ValueType::NUMBER) {
        throw std::runtime_error("pow() expects two numbers.");
    }
    double base = std::get<double>(args[0].as);
    double exp = std::get<double>(args[1].as);
    return Value(std::pow(base, exp));
}

Value abs_fn(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("abs() expects 1 argument.");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("abs() expects a number.");
    }
    double num = std::get<double>(args[0].as);
    return Value(std::abs(num));
}

Value ceil_fn(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("ceil() expects 1 argument.");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("ceil() expects a number.");
    }
    double num = std::get<double>(args[0].as);
    return Value(std::ceil(num));
}

Value floor_fn(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("floor() expects 1 argument.");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("floor() expects a number.");
    }
    double num = std::get<double>(args[0].as);
    return Value(std::floor(num));
}

Value round_fn(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("round() expects 1 argument.");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("round() expects a number.");
    }
    double num = std::get<double>(args[0].as);
    return Value(std::round(num));
}

Value sin_fn(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("sin() expects 1 argument.");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("sin() expects a number.");
    }
    double num = std::get<double>(args[0].as);
    return Value(std::sin(num));
}

Value cos_fn(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("cos() expects 1 argument.");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("cos() expects a number.");
    }
    double num = std::get<double>(args[0].as);
    return Value(std::cos(num));
}

Value tan_fn(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("tan() expects 1 argument.");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("tan() expects a number.");
    }
    double num = std::get<double>(args[0].as);
    return Value(std::tan(num));
}

Value random_fn(VM& vm, std::vector<Value> args) {
    (void)vm;
    (void)args;
    // Simple random number between 0 and 1
    return Value((double)rand() / RAND_MAX);
}

namespace neutron {
    void register_math_functions(VM& vm, std::shared_ptr<Environment> env) {
        env->define("add", Value(vm.allocate<NativeFn>(add, 2, true)));
        env->define("subtract", Value(vm.allocate<NativeFn>(subtract, 2, true)));
        env->define("multiply", Value(vm.allocate<NativeFn>(multiply, 2, true)));
        env->define("divide", Value(vm.allocate<NativeFn>(divide, 2, true)));
        env->define("sqrt", Value(vm.allocate<NativeFn>(sqrt_fn, 1, true)));
        env->define("pow", Value(vm.allocate<NativeFn>(pow_fn, 2, true)));
        env->define("abs", Value(vm.allocate<NativeFn>(abs_fn, 1, true)));
        env->define("ceil", Value(vm.allocate<NativeFn>(ceil_fn, 1, true)));
        env->define("floor", Value(vm.allocate<NativeFn>(floor_fn, 1, true)));
        env->define("round", Value(vm.allocate<NativeFn>(round_fn, 1, true)));
        env->define("sin", Value(vm.allocate<NativeFn>(sin_fn, 1, true)));
        env->define("cos", Value(vm.allocate<NativeFn>(cos_fn, 1, true)));
        env->define("tan", Value(vm.allocate<NativeFn>(tan_fn, 1, true)));
        env->define("random", Value(vm.allocate<NativeFn>(random_fn, 0, true)));
    }
}

extern "C" void neutron_init_math_module(neutron::VM* vm) {
    auto math_env = std::make_shared<neutron::Environment>();
    neutron::register_math_functions(*vm, math_env);
    auto math_module = vm->allocate<neutron::Module>("math", math_env);
    vm->define_module("math", math_module);
}
