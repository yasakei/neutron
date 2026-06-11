/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
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
#include "aot/aot_runtime.h"
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
    double a = args[0].as.number;
    double b = args[1].as.number;
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
    double a = args[0].as.number;
    double b = args[1].as.number;
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
    double a = args[0].as.number;
    double b = args[1].as.number;
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
    double a = args[0].as.number;
    double b = args[1].as.number;
    if (b == 0) {
        throw std::runtime_error("Division by zero.");
    }
    double result = a / b;
    if (std::isnan(result) || std::isinf(result)) {
        throw std::runtime_error("Division resulted in invalid value.");
    }
    return Value(result);
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
    double num = args[0].as.number;
    if (num < 0) {
        throw std::runtime_error("sqrt() domain error: argument must be non-negative.");
    }
    double result = std::sqrt(num);
    if (std::isnan(result) || std::isinf(result)) {
        throw std::runtime_error("sqrt() resulted in invalid value.");
    }
    return Value(result);
}

Value pow_fn(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) {
        throw std::runtime_error("pow() expects 2 arguments.");
    }
    if (args[0].type != ValueType::NUMBER || args[1].type != ValueType::NUMBER) {
        throw std::runtime_error("pow() expects two numbers.");
    }
    double base = args[0].as.number;
    double exp = args[1].as.number;
    
    // Check for domain errors
    if (base == 0 && exp < 0) {
        throw std::runtime_error("pow() domain error: 0 raised to negative power.");
    }
    if (base < 0 && std::floor(exp) != exp) {
        throw std::runtime_error("pow() domain error: negative base with non-integer exponent.");
    }
    
    double result = std::pow(base, exp);
    if (std::isnan(result) || std::isinf(result)) {
        throw std::runtime_error("pow() resulted in invalid value.");
    }
    return Value(result);
}

Value abs_fn(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("abs() expects 1 argument.");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("abs() expects a number.");
    }
    double num = args[0].as.number;
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
    double num = args[0].as.number;
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
    double num = args[0].as.number;
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
    double num = args[0].as.number;
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
    double num = args[0].as.number;
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
    double num = args[0].as.number;
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
    double num = args[0].as.number;
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
        NativeFn* fn;

        fn = vm.allocate<NativeFn>(add, 2, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_add);
        env->define("add", Value(fn));

        fn = vm.allocate<NativeFn>(subtract, 2, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_subtract);
        env->define("subtract", Value(fn));

        fn = vm.allocate<NativeFn>(multiply, 2, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_multiply);
        env->define("multiply", Value(fn));

        fn = vm.allocate<NativeFn>(divide, 2, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_divide);
        env->define("divide", Value(fn));

        fn = vm.allocate<NativeFn>(sqrt_fn, 1, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_sqrt);
        env->define("sqrt", Value(fn));

        fn = vm.allocate<NativeFn>(pow_fn, 2, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_pow);
        env->define("pow", Value(fn));

        fn = vm.allocate<NativeFn>(abs_fn, 1, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_abs);
        env->define("abs", Value(fn));

        fn = vm.allocate<NativeFn>(ceil_fn, 1, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_ceil);
        env->define("ceil", Value(fn));

        fn = vm.allocate<NativeFn>(floor_fn, 1, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_floor);
        env->define("floor", Value(fn));

        fn = vm.allocate<NativeFn>(round_fn, 1, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_round);
        env->define("round", Value(fn));

        fn = vm.allocate<NativeFn>(sin_fn, 1, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_sin);
        env->define("sin", Value(fn));

        fn = vm.allocate<NativeFn>(cos_fn, 1, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_cos);
        env->define("cos", Value(fn));

        fn = vm.allocate<NativeFn>(tan_fn, 1, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_tan);
        env->define("tan", Value(fn));

        fn = vm.allocate<NativeFn>(random_fn, 0, true);
        aot_registerModuleNativeFn(fn, (void*)&aot_wrap_math_random);
        env->define("random", Value(fn));
    }
}

extern "C" void neutron_init_math_module(neutron::VM* vm) {
    auto math_env = std::make_shared<neutron::Environment>();
    neutron::register_math_functions(*vm, math_env);
    auto math_module = vm->allocate<neutron::Module>("math", math_env);
    vm->define_module("math", math_module);
}
