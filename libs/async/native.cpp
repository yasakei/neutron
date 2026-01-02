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
    std::shared_future<Value> future;
    std::thread thread;
    bool completed = false;
    Value result;
    
    AsyncHandle() {
        future = promise.get_future().share();
    }
    
    ~AsyncHandle() {
        if (thread.joinable()) {
            thread.join();
        }
    }
};

class FutureObject : public Array {
public:
    std::shared_ptr<AsyncHandle> handle;
    int id;
    static int next_id;
    
    FutureObject(Value callable) : Array() {
        elements.push_back(callable);
        handle = std::make_shared<AsyncHandle>();
        id = next_id++;
    }
    
    std::string toString() const override {
        return "<Future:" + std::to_string(id) + ">";
    }
};

int FutureObject::next_id = 1;

// Function to run an async operation in a separate thread
static Value async_run(VM& vm, const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("async.run() expects 1 argument (function)");
    }
    if (args[0].type != ValueType::CALLABLE) {
        throw std::runtime_error("async.run() expects a function argument");
    }
    
    Value callable = args[0];
    auto futureObj = vm.allocate<FutureObject>(callable);
    auto handle = futureObj->handle;
    
    // Spawn thread
    handle->thread = std::thread([&vm, futureObj, handle]() {
        vm.lock();
        try {
            // Get the callable from the FutureObject
            Value callable = futureObj->elements[0];
            std::vector<Value> args; // No args for now
            Value result = vm.call(callable, args);
            handle->result = result;
            handle->completed = true;
            handle->promise.set_value(result);
        } catch (const std::exception& e) {
            handle->completed = true;
            try {
                handle->promise.set_exception(std::current_exception());
            } catch (...) {}
        } catch (...) {
            handle->completed = true;
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
    
    FutureObject* futureObj = nullptr;
    if (args[0].type == ValueType::ARRAY) {
        Array* arr = args[0].as.array;
        futureObj = dynamic_cast<FutureObject*>(arr);
    } else if (args[0].type == ValueType::OBJECT) {
        Object* obj = args[0].as.object;
        futureObj = dynamic_cast<FutureObject*>(obj);
    }
    
    if (!futureObj) {
        throw std::runtime_error("async.await() expects an async handle object");
    }
    
    // Wait for the thread to finish if it's still running
    if (futureObj->handle->thread.joinable()) {
        // Release lock to allow async thread to run
        int count = vm.unlock_fully();
        futureObj->handle->thread.join();
        vm.relock(count);
    }
    
    // Return the stored result
    if (futureObj->handle->completed) {
        return futureObj->handle->result;
    }
    
    // Fallback - shouldn't happen
    return Value();
}

// Function to sleep asynchronously
static Value async_sleep(VM& vm, const std::vector<Value>& args) {
    (void)vm; // Unused parameter
    if (args.size() != 1) {
        throw std::runtime_error("async.sleep() expects 1 argument (milliseconds)");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("async.sleep() expects a number argument (milliseconds)");
    }
    
    double ms = args[0].as.number;
    
    // We cannot unlock here because the VM stack is not thread-safe.
    // If we unlock, another thread might execute bytecode and corrupt the stack.
    // So we must hold the lock while sleeping.
    // int count = vm.unlock_fully();
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(ms)));
    // vm.relock(count);
    
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
    double ms = args[1].as.number;
    
    auto futureObj = vm.allocate<FutureObject>(callable);
    auto handle = futureObj->handle;
    
    handle->thread = std::thread([&vm, futureObj, handle, ms]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(ms)));
        
        vm.lock();
        try {
            Value callable = futureObj->elements[0];
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