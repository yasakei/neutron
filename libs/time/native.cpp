/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, for both open source and commercial purposes.
 * 
 * Conditions:
 * 
 * 1. The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 * 
 * 2. Attribution is appreciated but NOT required.
 *    Suggested (optional) credit:
 *    "Built using Neutron Programming Language (c) yasakei"
 * 
 * 3. The name "Neutron" and the name of the copyright holder may not be used
 *    to endorse or promote products derived from this Software without prior
 *    written permission.
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
#include "types/obj_string.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#include <process.h>  // Required for std::thread on Windows
#endif
#include <thread>

using namespace neutron;

// Get current timestamp
Value time_now(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 0) {
        throw std::runtime_error("Expected 0 arguments for time.now().");
    }
    
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return Value(static_cast<double>(now));
}

// Format timestamp as string
Value time_format(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::runtime_error("Expected 1-2 arguments for time.format().");
    }
    
    if (arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("First argument for time.format() must be a number (timestamp).");
    }
    
    // Get the timestamp
    double timestamp = arguments[0].as.number;
    
    // Default format or custom format
    std::string format = "%Y-%m-%d %H:%M:%S";
    if (arguments.size() == 2) {
        if (arguments[1].type != ValueType::OBJ_STRING) {
            throw std::runtime_error("Second argument for time.format() must be a string (format).");
        }
        format = arguments[1].asString()->chars;
    }
    
    // Convert timestamp to time_t
    std::time_t t = static_cast<std::time_t>(timestamp / 1000); // Assuming milliseconds
    
    // Format the time
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), format.c_str());
    
    return Value(vm.internString(oss.str()));
}

// Sleep for specified milliseconds
Value time_sleep(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for time.sleep().");
    }
    
    if (arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Argument for time.sleep() must be a number (milliseconds).");
    }
    
    int milliseconds = static_cast<int>(arguments[0].as.number);
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    
    return Value(nullptr);
}

namespace neutron {
    void register_time_functions(VM& vm, std::shared_ptr<Environment> env) {
        env->define("now", Value(vm.allocate<NativeFn>(time_now, 0, true)));
        env->define("format", Value(vm.allocate<NativeFn>(time_format, -1, true))); // -1 for variable arguments
        env->define("sleep", Value(vm.allocate<NativeFn>(time_sleep, 1, true)));
    }
}

extern "C" void neutron_init_time_module(neutron::VM* vm) {
    auto time_env = std::make_shared<neutron::Environment>();
    neutron::register_time_functions(*vm, time_env);
    auto time_module = vm->allocate<neutron::Module>("time", time_env);
    vm->define_module("time", time_module);
}
