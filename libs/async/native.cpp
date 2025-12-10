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
#include "types/obj_string.h"
#include "runtime/environment.h"
#include "types/value.h"
#include "types/json_object.h"
#include "types/array.h"
#include "expr.h"
#include <iostream>
#include <future>
#include <thread>
#include <functional>
#include <memory>
#include <chrono>

using namespace neutron;

// Structure to hold the promise and future for async operations
struct AsyncHandle {
    std::promise<Value> promise;
    std::future<Value> future;
    std::thread thread;
    
    AsyncHandle() {
        future = promise.get_future();
    }
    
    ~AsyncHandle() {
        if (thread.joinable()) {
            thread.detach();
        }
    }
};

class FutureObject : public Object {
public:
    std::shared_ptr<AsyncHandle> handle;
    
    FutureObject() : handle(std::make_shared<AsyncHandle>()) {}
    
    std::string toString() const override {
        return "<Future>";
    }
};

// Function to run an async operation in a separate thread
static Value async_run(VM& vm, const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("async.run() expects 1 argument (function)");
    }
    if (args[0].type != ValueType::CALLABLE) {
        throw std::runtime_error("async.run() expects a function argument");
    }
    
    Value callable = args[0];
    auto futureObj = vm.allocate<FutureObject>();
    auto handle = futureObj->handle;
    
    // Spawn thread
    handle->thread = std::thread([&vm, callable, handle]() {
        vm.lock();
        try {
            std::vector<Value> args; // No args for now
            Value result = vm.call(callable, args);
            handle->promise.set_value(result);
        } catch (const std::exception& e) {
            try {
                handle->promise.set_exception(std::current_exception());
            } catch (...) {}
        } catch (...) {
             try {
                handle->promise.set_exception(std::current_exception());
            } catch (...) {}
        }
        vm.unlock();
    });
    
    return Value(futureObj);
}

// Function to await the result of an async operation
static Value async_await(VM& vm, const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("async.await() expects 1 argument (async handle)");
    }
    if (args[0].type != ValueType::OBJECT) {
        throw std::runtime_error("async.await() expects an async handle object");
    }
    
    Object* obj = std::get<Object*>(args[0].as);
    FutureObject* futureObj = dynamic_cast<FutureObject*>(obj);
    
    if (!futureObj) {
        throw std::runtime_error("async.await() expects a Future object");
    }
    
    // Release lock to allow async thread to run
    int count = vm.unlock_fully();
    
    try {
        Value result = futureObj->handle->future.get();
        vm.relock(count);
        return result;
    } catch (const std::exception& e) {
        vm.relock(count);
        throw std::runtime_error(e.what());
    }
}

// Function to sleep asynchronously
static Value async_sleep(VM& vm, const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("async.sleep() expects 1 argument (milliseconds)");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("async.sleep() expects a number argument (milliseconds)");
    }
    
    double ms = std::get<double>(args[0].as);
    
    int count = vm.unlock_fully();
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(ms)));
    vm.relock(count);
    
    return Value(true);
}

// Function to create a timer that executes a function after a delay
static Value async_timer(VM& vm, const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("async.timer() expects 2 arguments (function, delay_ms)");
    }
     if (args[0].type != ValueType::CALLABLE) {
        throw std::runtime_error("async.timer() expects a function argument");
    }
    if (args[1].type != ValueType::NUMBER) {
        throw std::runtime_error("async.timer() expects a number argument (delay)");
    }
    
    Value callable = args[0];
    double ms = std::get<double>(args[1].as);
    
    auto futureObj = vm.allocate<FutureObject>();
    auto handle = futureObj->handle;
    
    handle->thread = std::thread([&vm, callable, handle, ms]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(ms)));
        
        vm.lock();
        try {
            std::vector<Value> args;
            Value result = vm.call(callable, args);
            handle->promise.set_value(result);
        } catch (...) {
             try {
                handle->promise.set_exception(std::current_exception());
            } catch (...) {}
        }
        vm.unlock();
    });
    
    return Value(futureObj);
}

void neutron_init_async_module(VM* vm) {
    std::cout << "Async module initialized (New)" << std::endl;
    vm->define_native("async_run", new NativeFn(async_run, 1, true));
    vm->define_native("async_await", new NativeFn(async_await, 1, true));
    vm->define_native("async_sleep", new NativeFn(async_sleep, 1, true));
    vm->define_native("async_timer", new NativeFn(async_timer, 2, true));
    
    // Create module object
    auto env = std::make_shared<Environment>();
    env->define("run", Value(vm->allocate<NativeFn>(async_run, 1, true)));
    env->define("await", Value(vm->allocate<NativeFn>(async_await, 1, true)));
    env->define("sleep", Value(vm->allocate<NativeFn>(async_sleep, 1, true)));
    env->define("timer", Value(vm->allocate<NativeFn>(async_timer, 2, true)));
    
    auto module = vm->allocate<Module>("async", env);
    vm->define_module("async", module);
}