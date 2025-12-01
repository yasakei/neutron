/*
 * Neutron Programming Language
 * Copyright (c) 2025 yasakei
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
static Value add(const std::vector<Value>& args) {
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
static Value subtract(const std::vector<Value>& args) {
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
static Value multiply(const std::vector<Value>& args) {
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
static Value divide(const std::vector<Value>& args) {
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
static Value sqrt_fn(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("sqrt() expects 1 argument.");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("sqrt() expects a number.");
    }
    double num = std::get<double>(args[0].as);
    return Value(std::sqrt(num));
}

namespace neutron {
    void register_math_functions(std::shared_ptr<Environment> env) {
        env->define("add", Value(new NativeFn(add, 2)));
        env->define("subtract", Value(new NativeFn(subtract, 2)));
        env->define("multiply", Value(new NativeFn(multiply, 2)));
        env->define("divide", Value(new NativeFn(divide, 2)));
        env->define("sqrt", Value(new NativeFn(sqrt_fn, 1)));
    }
}

extern "C" void neutron_init_math_module(neutron::VM* vm) {
    auto math_env = std::make_shared<neutron::Environment>();
    neutron::register_math_functions(math_env);
    auto math_module = new neutron::Module("math", math_env);
    vm->define_module("math", math_module);
}
