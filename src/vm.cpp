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

#include "vm.h"
#include "runtime/native_functions.h"
#include "sys/native.h"
#include "compiler/compiler.h"
#include "compiler/bytecode.h"
#include "runtime/debug.h"
#include "runtime/error_handler.h"
#include "compiler/scanner.h"
#include "compiler/parser.h"
#include "types/obj_string.h"
#include "types/json_object.h"
#include "types/instance.h"
#include "types/bound_method.h"
#include "types/bound_array_method.h"
#include "types/buffer.h"
#include "types/bound_string_method.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#if defined(_WIN32) || defined(WIN32)
#include "cross-platfrom/dlfcn_compat.h"
#else
#include <dlfcn.h>
#endif
#include <cmath>
#include <algorithm>

#include "capi.h"
#include "json/native.h"
#include "fmt/native.h"
#include "time/native.h"
#include "arrays/native.h"
#include "math/native.h"
#include "http/native.h"
#include "async/native.h"
#include "regex/native.h"
#include "modules/module_registry.h"
#include "utils/component_interface.h"

namespace neutron {
    
// External symbols needed by bytecode_runner
extern "C" {
    [[maybe_unused]] const unsigned char bytecode_data[] = {0};  // Empty bytecode array
    [[maybe_unused]] const unsigned int bytecode_size = 0;       // Size is 0
}

// Exception used to unwind C++ stack when a runtime error is caught by Neutron code
struct VMException {
    Value value;
    VMException(Value v) : value(v) {}
};

bool isTruthy(const Value& value) {
    switch (value.type) {
        case ValueType::NIL:
            return false;
        case ValueType::BOOLEAN:
            return std::get<bool>(value.as);
        default:
            return true;
    }
}

// Helper function for error reporting with stack trace
[[noreturn]] void runtimeError(VM* vm, const std::string& message, int line = -1) {
    // Check if there is an active exception handler that covers the current instruction
    if (!vm->frames.empty() && !vm->exceptionFrames.empty()) {
        CallFrame* frame = &vm->frames.back();
        if (frame->function && frame->function->chunk) {
            int current_pos = frame->ip - frame->function->chunk->code.data() - 1;
            
            // Look for a handler in the current frame
            for (int i = vm->exceptionFrames.size() - 1; i >= 0; i--) {
                VM::ExceptionFrame& handler = vm->exceptionFrames[i];
                if (current_pos >= handler.tryStart && current_pos <= handler.tryEnd) {
                    // Found a handler, throw a VMException to be caught in VM::run
                    throw VMException(Value(message));
                }
            }
        }
    }

    // Build stack trace
    std::vector<StackFrame> stackTrace;
    
    for (auto it = vm->frames.rbegin(); it != vm->frames.rend(); ++it) {
        std::string funcName = "<script>";
        if (it->function && it->function->declaration) {
            funcName = it->function->declaration->name.lexeme;
        }
        
        int frameLine = it->currentLine > 0 ? it->currentLine : line;
        stackTrace.emplace_back(funcName, it->fileName.empty() ? vm->currentFileName : it->fileName, frameLine);
    }
    
    ErrorHandler::reportRuntimeError(message, vm->currentFileName, line, stackTrace);
    exit(1);
}

VM::VM() : ip(nullptr), nextGC(1024), currentFileName("<stdin>"), hasException(false), pendingException(Value()) {  // Start GC at 1024 bytes
    // Initialize error handler
    ErrorHandler::setColorEnabled(true);
    ErrorHandler::setStackTraceEnabled(true);
    
    globals["say"] = Value(allocate<NativeFn>(std::function<Value(std::vector<Value>)>(native_say), 1));
    
    // Register array functions
    // Built-in modules (sys, math, json, http, time, convert) are now loaded on-demand
    // when explicitly imported with "use modulename;"
    
    // External modules are registered but not initialized until imported
    run_external_module_initializers(this);
    
    // Set up module search paths
    module_search_paths.push_back(".");
    module_search_paths.push_back("lib/");
    module_search_paths.push_back("libs/");
    module_search_paths.push_back("box/");
}

VM::~VM() {
    for (Object* obj : heap) {
        delete obj;
    }
    heap.clear();
}

// Thread safety implementation
void VM::lock() {
    std::thread::id this_id = std::this_thread::get_id();
    if (owner_thread == this_id) {
        lock_count++;
    } else {
        vm_mutex.lock();
        owner_thread = this_id;
        lock_count = 1;
    }
}

void VM::unlock() {
    if (owner_thread != std::this_thread::get_id()) {
        return;
    }
    lock_count--;
    if (lock_count == 0) {
        owner_thread = std::thread::id();
        vm_mutex.unlock();
    }
}

int VM::unlock_fully() {
    if (owner_thread != std::this_thread::get_id()) {
        return 0;
    }
    int count = lock_count;
    lock_count = 0;
    owner_thread = std::thread::id();
    vm_mutex.unlock();
    return count;
}

void VM::relock(int count) {
    vm_mutex.lock();
    owner_thread = std::this_thread::get_id();
    lock_count = count;
}

Function::Function(const FunctionStmt* declaration, std::shared_ptr<Environment> closure) 
    : name(), arity_val(0), chunk(new Chunk()), declaration(declaration), closure(closure) {}

Function::~Function() {
    delete chunk;
}

Value Function::call(VM& /*vm*/, std::vector<Value> /*arguments*/) {
    // This is now handled by the VM's call stack
    return Value();
}

std::string Function::toString() const {
    if (declaration) {
        return "<fn " + declaration->name.lexeme + ">";
    } else if (!name.empty()) {
        return "<fn " + name + ">";
    }
    return "<script>";
}

int Function::arity() { return arity_val; }

void VM::interpret(Function* function) {
    lock();
    push(Value(function));
    callValue(Value(function), 0);
    run();
    unlock();
}

void VM::push(const Value& value) {
    // Increased stack size from 256 to 4096 to handle complex nested expressions
    if (stack.size() >= 4096) {
        ErrorHandler::stackOverflowError(currentFileName, frames.empty() ? -1 : frames.back().currentLine);
        exit(1);
    }
    stack.push_back(value);
}

Value VM::pop() {
    if (stack.empty()) {
        ErrorHandler::fatal("Stack underflow - internal VM error", ErrorType::STACK_ERROR);
    }
    Value value = stack.back();
    stack.pop_back();
    return value;
}

bool VM::call(Function* function, int argCount) {
    if (argCount != function->arity_val) {
        std::string funcName = function->declaration ? function->declaration->name.lexeme : "<anonymous>";
        runtimeError(this, "Expected " + std::to_string(function->arity_val) + " arguments but got " + std::to_string(argCount) + " for function '" + funcName + "'.", frames.empty() ? -1 : frames.back().currentLine);
        return false;
    }

    if (frames.size() == 256) {
        runtimeError(this, "Stack overflow.", frames.empty() ? -1 : frames.back().currentLine);
        return false;
    }

    CallFrame* frame = &frames.emplace_back();
    frame->function = function;
    if (function->chunk->code.empty()) {
        frame->ip = nullptr;
    } else {
        frame->ip = function->chunk->code.data();
    }
    frame->slot_offset = stack.size() - argCount;  // Point to first argument position
    frame->fileName = currentFileName;
    frame->currentLine = -1;

    return true;
}

bool VM::callValue(Value callee, int argCount) {
    // Handle BoundArrayMethod and BoundStringMethod
    if (callee.type == ValueType::OBJECT) {
        Object* obj = std::get<Object*>(callee.as);
        if (BoundArrayMethod* arrayMethod = dynamic_cast<BoundArrayMethod*>(obj)) {
            return callArrayMethod(arrayMethod, argCount);
        } else if (BoundStringMethod* stringMethod = dynamic_cast<BoundStringMethod*>(obj)) {
            return callStringMethod(stringMethod, argCount);
        }
    }
    
    if (callee.type == ValueType::CALLABLE) {
        Callable* callable = std::get<Callable*>(callee.as);
        
        // Check for BoundMethod first
        if (BoundMethod* boundMethod = dynamic_cast<BoundMethod*>(callable)) {
            // For bound methods, replace the method on the stack with the receiver
            // Stack before: [... | boundMethod | arg1 | arg2]
            // Stack after:  [... | receiver | arg1 | arg2]
            // The receiver becomes an implicit first argument
            size_t methodPos = stack.size() - argCount - 1;
            stack[methodPos] = boundMethod->receiver;
            // Call with argCount+1 to include the receiver as first argument (slot 0)
            // But we need to adjust the method's arity check
            if (boundMethod->method->arity_val != argCount) {
                std::string funcName = boundMethod->method->declaration ? 
                                     boundMethod->method->declaration->name.lexeme : "<method>";
                runtimeError(this, "Expected " + std::to_string(boundMethod->method->arity_val) + " arguments but got " + std::to_string(argCount) + " for method '" + funcName + "'.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            // Manually set up the call frame to include receiver at slot 0
            if (frames.size() == 256) {
                runtimeError(this, "Stack overflow.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            CallFrame* frame = &frames.emplace_back();
            frame->function = boundMethod->method;
            if (boundMethod->method->chunk->code.empty()) {
                frame->ip = nullptr;
            } else {
                frame->ip = boundMethod->method->chunk->code.data();
            }
            // slot_offset points to the receiver (one position before args)
            frame->slot_offset = stack.size() - argCount - 1;
            frame->fileName = currentFileName;
            frame->currentLine = -1;
            frame->isBoundMethod = true;  // Mark as bound method call
            return true;
        } else if (Function* function = dynamic_cast<Function*>(callable)) {
            return call(function, argCount);
        } else if (NativeFn* native = dynamic_cast<NativeFn*>(callable)) {
            if (native->arity() != -1 && native->arity() != argCount) {
                runtimeError(this, "Expected " + std::to_string(native->arity()) + " arguments but got " + std::to_string(argCount) + " for native function.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            std::vector<Value> args;
            for (int i = 0; i < argCount; i++) {
                args.push_back(stack[stack.size() - argCount + i]);
            }
            try {
                Value result = native->call(*this, args);
                stack.resize(stack.size() - argCount - 1);
                push(result);
            } catch (const std::exception& e) {
                runtimeError(this, e.what(), frames.empty() ? -1 : frames.back().currentLine);
            }
            return true;
        } else if (callable->isCNativeFn()) {
            if (callable->arity() != -1 && callable->arity() != argCount) {
                runtimeError(this, "Expected " + std::to_string(callable->arity()) + " arguments but got " + std::to_string(argCount) + " for native function.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            std::vector<Value> args;
            for (int i = 0; i < argCount; i++) {
                args.push_back(stack[stack.size() - argCount + i]);
            }
            try {
                Value result = callable->call(*this, args);
                stack.resize(stack.size() - argCount - 1);
                push(result);
            } catch (const std::exception& e) {
                runtimeError(this, e.what(), frames.empty() ? -1 : frames.back().currentLine);
            }
            return true;
        }
    }

    // Handle CLASS type
    if (callee.type == ValueType::CLASS) {
        Class* klass = std::get<Class*>(callee.as);
        
        // Create instance
        Instance* instance = allocate<Instance>(klass);
        Value instanceVal(instance);
        
        // Check for initializer
        auto it = klass->methods.find("initialize");
        if (it != klass->methods.end()) {
             if (it->second.type == ValueType::CALLABLE && std::holds_alternative<Callable*>(it->second.as)) {
                Callable* callable = std::get<Callable*>(it->second.as);
                if (Function* initializer = dynamic_cast<Function*>(callable)) {
                
                if (initializer->arity_val != argCount) {
                     runtimeError(this, "Expected " + std::to_string(initializer->arity_val) + " arguments but got " + std::to_string(argCount) + " for constructor.", frames.empty() ? -1 : frames.back().currentLine);
                     return false;
                }
                
                // Replace the class on the stack with the instance (this)
                stack[stack.size() - argCount - 1] = instanceVal;
                
                // Manually set up the call frame
                if (frames.size() == 256) {
                    runtimeError(this, "Stack overflow.", frames.empty() ? -1 : frames.back().currentLine);
                    return false;
                }

                CallFrame* frame = &frames.emplace_back();
                frame->function = initializer;
                if (initializer->chunk->code.empty()) {
                    frame->ip = nullptr;
                } else {
                    frame->ip = initializer->chunk->code.data();
                }
                frame->slot_offset = stack.size() - argCount - 1; // Include receiver
                frame->fileName = currentFileName;
                frame->currentLine = -1;
                frame->isBoundMethod = true;
                return true;
                }
             }
        } 
        
        // No initializer or not a function
        if (argCount != 0) {
            runtimeError(this, "Expected 0 arguments but got " + std::to_string(argCount) + " for constructor.", frames.empty() ? -1 : frames.back().currentLine);
            return false;
        }
        
        stack.resize(stack.size() - argCount - 1);
        push(instanceVal);
        return true;
    }

    runtimeError(this, "Can only call functions and classes.", frames.empty() ? -1 : frames.back().currentLine);
    return false;
}

bool VM::callArrayMethod(BoundArrayMethod* method, int argCount) {
    Array* arr = method->array;
    std::string methodName = method->methodName;
    
    // Save original stack size (before method and args)
    size_t stackBase = stack.size() - argCount - 1;
    
    // Get arguments from stack
    std::vector<Value> args;
    for (int i = 0; i < argCount; i++) {
        args.push_back(stack[stackBase + 1 + i]);
    }
    
    Value result;
    
    try {
        if (methodName == "length") {
            // length() - return array size
            if (argCount != 0) {
                runtimeError(this, "Expected 0 arguments but got " + std::to_string(argCount) + " for Array.length.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            result = Value(static_cast<double>(arr->size()));
        } else if (methodName == "push") {
            // push(value) - add element to end
            if (argCount != 1) {
                runtimeError(this, "Expected 1 argument but got " + std::to_string(argCount) + " for Array.push.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            arr->push(args[0]);
            result = Value(); // nil
        } else if (methodName == "pop") {
            // pop() - remove and return last element
            if (argCount != 0) {
                runtimeError(this, "Expected 0 arguments but got " + std::to_string(argCount) + " for Array.pop.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            result = arr->pop();
        } else if (methodName == "slice") {
            // slice(start, end) - return subarray
            if (argCount != 2) {
                runtimeError(this, "Expected 2 arguments but got " + std::to_string(argCount) + " for Array.slice.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            if (args[0].type != ValueType::NUMBER || args[1].type != ValueType::NUMBER) {
                runtimeError(this, "Array.slice expects number arguments.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            int start = static_cast<int>(std::get<double>(args[0].as));
            int end = static_cast<int>(std::get<double>(args[1].as));
            
            if (start < 0) start = 0;
            if (end > static_cast<int>(arr->size())) end = arr->size();
            if (start > end) start = end;
            
            std::vector<Value> sliced;
            for (int i = start; i < end; i++) {
                sliced.push_back(arr->at(i));
            }
            result = Value(allocate<Array>(sliced));
        } else if (methodName == "indexOf") {
            // indexOf(value) - return index of first occurrence or -1
            if (argCount != 1) {
                runtimeError(this, "indexOf() expects 1 argument.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            int index = -1;
            for (size_t i = 0; i < arr->size(); i++) {
                if (arr->at(i).toString() == args[0].toString()) {
                    index = static_cast<int>(i);
                    break;
                }
            }
            result = Value(static_cast<double>(index));
        } else if (methodName == "join") {
            // join(separator) - join array elements into string
            if (argCount != 1) {
                runtimeError(this, "join() expects 1 argument.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            std::string separator = args[0].toString();
            std::string joined;
            for (size_t i = 0; i < arr->size(); i++) {
                if (i > 0) joined += separator;
                joined += arr->at(i).toString();
            }
            result = Value(joined);
        } else if (methodName == "reverse") {
            // reverse() - reverse array in place
            if (argCount != 0) {
                runtimeError(this, "reverse() expects 0 arguments.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            std::reverse(arr->elements.begin(), arr->elements.end());
            result = Value(); // nil
        } else if (methodName == "sort") {
            // sort() - sort array in place (numbers then strings)
            if (argCount != 0) {
                runtimeError(this, "sort() expects 0 arguments.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            std::sort(arr->elements.begin(), arr->elements.end(), [](const Value& a, const Value& b) {
                // Numbers come before strings
                if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) {
                    return std::get<double>(a.as) < std::get<double>(b.as);
                } else if (a.type == ValueType::OBJ_STRING && b.type == ValueType::OBJ_STRING) {
                    return a.toString() < b.toString();
                } else if (a.type == ValueType::NUMBER) {
                    return true;
                } else {
                    return false;
                }
            });
            result = Value(); // nil
        } else if (methodName == "map") {
            // map(function) - transform each element
            if (argCount != 1) {
                runtimeError(this, "map() expects 1 argument (function).", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            if (args[0].type != ValueType::CALLABLE) {
                runtimeError(this, "map() argument must be a function.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            
            // Save the current frame depth - we want to return to this depth after each call
            size_t baseFrameDepth = frames.size();
            
            std::vector<Value> mapped;
            for (size_t i = 0; i < arr->size(); i++) {
                // Push function and argument for call
                push(args[0]); // function
                push(arr->at(i)); // argument
                
                // Call the function - this sets up a new frame but doesn't execute it
                if (!callValue(args[0], 1)) {
                    return false;
                }
                
                // Now execute the function by running until we return to base frame depth
                run(baseFrameDepth);
                
                // Get result and store it
                mapped.push_back(pop());
            }
            result = Value(allocate<Array>(mapped));
        } else if (methodName == "filter") {
            // filter(function) - filter elements by predicate
            if (argCount != 1) {
                runtimeError(this, "filter() expects 1 argument (function).", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            if (args[0].type != ValueType::CALLABLE) {
                runtimeError(this, "filter() argument must be a function.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            
            size_t baseFrameDepth = frames.size();
            
            std::vector<Value> filtered;
            for (size_t i = 0; i < arr->size(); i++) {
                // Push function and argument for call
                push(args[0]); // function
                push(arr->at(i)); // argument
                
                // Call the function
                if (!callValue(args[0], 1)) {
                    return false;
                }
                
                // Execute the function
                run(baseFrameDepth);
                
                // Get result and test it
                Value test = pop();
                if (isTruthy(test)) {
                    filtered.push_back(arr->at(i));
                }
            }
            result = Value(allocate<Array>(filtered));
        } else if (methodName == "find") {
            // find(function) - find first element matching predicate
            if (argCount != 1) {
                runtimeError(this, "find() expects 1 argument (function).", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            if (args[0].type != ValueType::CALLABLE) {
                runtimeError(this, "find() argument must be a function.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            
            size_t baseFrameDepth = frames.size();
            
            result = Value(); // nil by default
            for (size_t i = 0; i < arr->size(); i++) {
                // Push function and argument for call
                push(args[0]); // function
                push(arr->at(i)); // argument
                
                // Call the function
                if (!callValue(args[0], 1)) {
                    return false;
                }
                
                // Execute the function
                run(baseFrameDepth);
                
                // Get result and test it
                Value test = pop();
                if (isTruthy(test)) {
                    result = arr->at(i);
                    break;
                }
            }
        } else {
            runtimeError(this, "Unknown array method: " + methodName, frames.empty() ? -1 : frames.back().currentLine);
            return false;
        }
        
        // Restore stack to original size and push result
        stack.resize(stackBase);
        push(result);
        return true;
        
    } catch (const VMException& e) {
        throw;
    } catch (const std::runtime_error& e) {
        runtimeError(this, e.what(), frames.empty() ? -1 : frames.back().currentLine);
        return false;
    }
}

bool VM::callStringMethod(BoundStringMethod* method, int argCount) {
    std::string str = method->stringValue;
    std::string methodName = method->methodName;
    
    // Save original stack size (before method and args)
    size_t stackBase = stack.size() - argCount - 1;
    
    // Get arguments from stack
    std::vector<Value> args;
    for (int i = 0; i < argCount; i++) {
        args.push_back(stack[stackBase + 1 + i]);
    }
    
    Value result;
    
    try {
        if (methodName == "length") {
            // length() - return string length
            if (argCount != 0) {
                runtimeError(this, "length() expects 0 arguments.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            result = Value(static_cast<double>(str.length()));
        } else if (methodName == "contains") {
            // contains(substring)
            if (argCount != 1) {
                runtimeError(this, "contains() expects 1 argument.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            std::string substr = args[0].toString();
            bool found = str.find(substr) != std::string::npos;
            result = Value(found);
        } else if (methodName == "split") {
            // split(delimiter)
            if (argCount != 1) {
                runtimeError(this, "split() expects 1 argument.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            std::string delimiter = args[0].toString();
            std::vector<Value> parts;
            
            if (delimiter.empty()) {
                // Split by character
                for (char c : str) {
                    parts.push_back(Value(std::string(1, c)));
                }
            } else {
                size_t pos = 0;
                std::string token;
                std::string s = str; // Make a copy to modify
                while ((pos = s.find(delimiter)) != std::string::npos) {
                    token = s.substr(0, pos);
                    parts.push_back(Value(token));
                    s.erase(0, pos + delimiter.length());
                }
                parts.push_back(Value(s));
            }
            result = Value(allocate<Array>(parts));
        } else if (methodName == "substring") {
            // substring(start, [end])
            if (argCount < 1 || argCount > 2) {
                runtimeError(this, "substring() expects 1 or 2 arguments.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            
            if (args[0].type != ValueType::NUMBER) {
                runtimeError(this, "substring() expects first argument to be a number.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
            int start = static_cast<int>(std::get<double>(args[0].as));
            int len = static_cast<int>(str.length());
            
            if (start < 0) start = 0;
            if (start > len) start = len;
            
            int end = len;
            if (argCount == 2) {
                if (args[1].type != ValueType::NUMBER) {
                    runtimeError(this, "substring() expects second argument to be a number.", frames.empty() ? -1 : frames.back().currentLine);
                    return false;
                }
                end = static_cast<int>(std::get<double>(args[1].as));
                if (end < 0) end = 0;
                if (end > len) end = len;
            }
            
            if (end < start) {
                int temp = start;
                start = end;
                end = temp;
            }
            
            result = Value(str.substr(start, end - start));
        } else {
            runtimeError(this, "Unknown string method: " + methodName, frames.empty() ? -1 : frames.back().currentLine);
            return false;
        }
        
        // Restore stack to original size and push result
        stack.resize(stackBase);
        push(result);
        return true;
        
    } catch (const VMException& e) {
        throw;
    } catch (const std::runtime_error& e) {
        runtimeError(this, e.what(), frames.empty() ? -1 : frames.back().currentLine);
        return false;
    }
}

void VM::run(size_t minFrameDepth) {
    CallFrame* frame = &frames.back();

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() \
    ([&]() -> Value& { \
        uint8_t index = READ_BYTE(); \
        if (index >= frame->function->chunk->constants.size()) { \
            runtimeError(this, "Constant index out of bounds: " + std::to_string(index) + \
                        " (max: " + std::to_string(frame->function->chunk->constants.size() - 1) + ")", \
                        frame->currentLine); \
        } \
        return frame->function->chunk->constants[index]; \
    }())
#define READ_STRING() (std::get<ObjString*>(READ_CONSTANT().as)->chars)

    while (true) {
    try {
        for (;;) {
            // Update currentLine from bytecode before processing instruction
        size_t offset = frame->ip - frame->function->chunk->code.data();
        if (offset < frame->function->chunk->lines.size()) {
            frame->currentLine = frame->function->chunk->lines[offset];
        }
        
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case (uint8_t)OpCode::OP_RETURN: {
                Value result = pop();
                size_t return_slot_offset = frame->slot_offset;  // Points to first argument
                bool was_bound_method = frame->isBoundMethod;
                frames.pop_back();
                
                if (frames.empty() || frames.size() <= minFrameDepth) {
                    // We are done executing - either the main script or we've returned to target depth
                    
                    // Clean up stack frame (arguments and callee)
                    if (was_bound_method) {
                        stack.resize(return_slot_offset);  // Keep everything before receiver
                    } else if (return_slot_offset > 0) {
                        // If it was a function call, clean up stack
                        // Ensure we don't underflow if stack is somehow smaller (shouldn't happen)
                        if (stack.size() >= return_slot_offset - 1) {
                            stack.resize(return_slot_offset - 1);  // Remove callee too
                        }
                    } else {
                        // Top level script or no arguments/callee on stack
                        stack.clear();
                    }
                    
                    push(result);
                    return;
                }

                frame = &frames.back();
                // For bound methods, receiver is at slot_offset, so resize to slot_offset
                // For regular functions, callee is at slot_offset-1, so resize to slot_offset-1
                if (was_bound_method) {
                    stack.resize(return_slot_offset);  // Keep everything before receiver
                } else if (return_slot_offset > 0) {
                    stack.resize(return_slot_offset - 1);  // Remove callee + arguments (NEUT-020 fix)
                } else {
                    // Edge case: top-level script or no arguments
                    stack.clear();
                }
                push(result);
                break;
            }
            case (uint8_t)OpCode::OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case (uint8_t)OpCode::OP_CLOSURE: {
                Value constant = READ_CONSTANT();
                // The constant should be a Function
                if (constant.type == ValueType::CALLABLE) {
                    push(constant);
                } else {
                    runtimeError(this, "OP_CLOSURE constant must be a function.", frames.empty() ? -1 : frames.back().currentLine);
                }
                break;
            }
            case (uint8_t)OpCode::OP_NIL: {
                push(Value());
                break;
            }
            case (uint8_t)OpCode::OP_TRUE: {
                push(Value(true));
                break;
            }
            case (uint8_t)OpCode::OP_FALSE: {
                push(Value(false));
                break;
            }
            case (uint8_t)OpCode::OP_POP: {
                pop();
                break;
            }
            case (uint8_t)OpCode::OP_DUP: {
                // Duplicate the value on top of the stack
                push(stack.back());
                break;
            }
            case (uint8_t)OpCode::OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(stack[frame->slot_offset + slot]);
                break;
            }
            case (uint8_t)OpCode::OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                stack[frame->slot_offset + slot] = stack.back();
                break;
            }
            case (uint8_t)OpCode::OP_GET_GLOBAL: {
                std::string name = READ_STRING();
                auto it = globals.find(name);
                if (it == globals.end()) {
                    // Check if it looks like a module name (common module names)
                    if (name == "json" || name == "math" || name == "sys" || name == "http" || 
                        name == "time" || name == "fmt" || name == "arrays") {
                        runtimeError(this, "Undefined variable '" + name + "'. Did you forget to import it? Use 'use " + name + ";' at the top of your file.", 
                                    frames.empty() ? -1 : frames.back().currentLine);
                    } else {
                        runtimeError(this, "Undefined variable '" + name + "'.", 
                                    frames.empty() ? -1 : frames.back().currentLine);
                    }
                }
                push(globals[name]);
                break;
            }
            case (uint8_t)OpCode::OP_DEFINE_GLOBAL: {
                std::string name = READ_STRING();
                globals[name] = stack.back();
                pop();
                break;
            }
            case (uint8_t)OpCode::OP_DEFINE_TYPED_GLOBAL: {
                std::string name = READ_STRING();
                TokenType type = static_cast<TokenType>(READ_BYTE());
                
                globals[name] = stack.back();
                globalTypes[name] = type;  // Store the type information
                pop();
                break;
            }
            case (uint8_t)OpCode::OP_SET_GLOBAL: {
                std::string name = READ_STRING();
                auto it = globals.find(name);
                if (it == globals.end()) {
                    runtimeError(this, "Undefined variable '" + name + "'.", 
                                frames.empty() ? -1 : frames.back().currentLine);
                }
                globals[name] = stack.back();
                break;
            }
            case (uint8_t)OpCode::OP_SET_GLOBAL_TYPED: {
                std::string name = READ_STRING();
                
                auto it = globals.find(name);
                if (it == globals.end()) {
                    runtimeError(this, "Undefined variable '" + name + "'.", 
                                frames.empty() ? -1 : frames.back().currentLine);
                }
                
                // Look up the expected type for this global variable
                auto typeIt = globalTypes.find(name);
                if (typeIt == globalTypes.end()) {
                    // If no type is stored, fall back to regular assignment
                    globals[name] = stack.back();
                    break;
                }
                
                TokenType expectedType = typeIt->second;
                Value value = stack.back();
                
                // Check type compatibility
                bool isValid = false;
                switch (expectedType) {
                    case TokenType::TYPE_INT:
                    case TokenType::TYPE_FLOAT:
                        isValid = value.type == ValueType::NUMBER;
                        break;
                    case TokenType::TYPE_STRING:
                        isValid = value.type == ValueType::OBJ_STRING;
                        break;
                    case TokenType::TYPE_BOOL:
                        isValid = value.type == ValueType::BOOLEAN;
                        break;
                    case TokenType::TYPE_ARRAY:
                        isValid = value.type == ValueType::ARRAY;
                        break;
                    case TokenType::TYPE_OBJECT:
                        isValid = value.type == ValueType::OBJECT;
                        break;
                    case TokenType::TYPE_ANY:
                        isValid = true;
                        break;
                    default:
                        isValid = true; // Unknown types are allowed
                        break;
                }
                
                if (!isValid) {
                    std::string expectedTypeName;
                    switch (expectedType) {
                        case TokenType::TYPE_INT:
                        case TokenType::TYPE_FLOAT:
                            expectedTypeName = "number";
                            break;
                        case TokenType::TYPE_STRING:
                            expectedTypeName = "string";
                            break;
                        case TokenType::TYPE_BOOL:
                            expectedTypeName = "boolean";
                            break;
                        case TokenType::TYPE_ARRAY:
                            expectedTypeName = "array";
                            break;
                        case TokenType::TYPE_OBJECT:
                            expectedTypeName = "object";
                            break;
                        case TokenType::TYPE_ANY:
                            expectedTypeName = "any";
                            break;
                        default:
                            expectedTypeName = "unknown";
                            break;
                    }
                    
                    std::string actualTypeName = value.type == ValueType::NIL ? "nil" : 
                                                  value.type == ValueType::BOOLEAN ? "boolean" :
                                                  value.type == ValueType::NUMBER ? "number" :
                                                  value.type == ValueType::OBJ_STRING ? "string" :
                                                  value.type == ValueType::ARRAY ? "array" :
                                                  value.type == ValueType::OBJECT ? "object" :
                                                  "callable";
                    
                    runtimeError(this, "Type mismatch: Cannot assign value of type '" + actualTypeName + 
                                "' to variable of type '" + expectedTypeName + "'",
                                frames.empty() ? -1 : frames.back().currentLine);
                }
                
                globals[name] = value;
                break;
            }
            case (uint8_t)OpCode::OP_SET_LOCAL_TYPED: {
                uint8_t slot = READ_BYTE();
                TokenType expectedType = static_cast<TokenType>(READ_BYTE());
                
                Value value = stack.back();
                
                // Check type compatibility
                bool isValid = false;
                switch (expectedType) {
                    case TokenType::TYPE_INT:
                    case TokenType::TYPE_FLOAT:
                        isValid = value.type == ValueType::NUMBER;
                        break;
                    case TokenType::TYPE_STRING:
                        isValid = value.type == ValueType::OBJ_STRING;
                        break;
                    case TokenType::TYPE_BOOL:
                        isValid = value.type == ValueType::BOOLEAN;
                        break;
                    case TokenType::TYPE_ARRAY:
                        isValid = value.type == ValueType::ARRAY;
                        break;
                    case TokenType::TYPE_OBJECT:
                        isValid = value.type == ValueType::OBJECT;
                        break;
                    case TokenType::TYPE_ANY:
                        isValid = true;
                        break;
                    default:
                        isValid = true; // Unknown types are allowed
                        break;
                }
                
                if (!isValid) {
                    std::string expectedTypeName;
                    switch (expectedType) {
                        case TokenType::TYPE_INT:
                        case TokenType::TYPE_FLOAT:
                            expectedTypeName = "number";
                            break;
                        case TokenType::TYPE_STRING:
                            expectedTypeName = "string";
                            break;
                        case TokenType::TYPE_BOOL:
                            expectedTypeName = "boolean";
                            break;
                        case TokenType::TYPE_ARRAY:
                            expectedTypeName = "array";
                            break;
                        case TokenType::TYPE_OBJECT:
                            expectedTypeName = "object";
                            break;
                        case TokenType::TYPE_ANY:
                            expectedTypeName = "any";
                            break;
                        default:
                            expectedTypeName = "unknown";
                            break;
                    }
                    
                    std::string actualTypeName = value.type == ValueType::NIL ? "nil" : 
                                                  value.type == ValueType::BOOLEAN ? "boolean" :
                                                  value.type == ValueType::NUMBER ? "number" :
                                                  value.type == ValueType::OBJ_STRING ? "string" :
                                                  value.type == ValueType::ARRAY ? "array" :
                                                  value.type == ValueType::OBJECT ? "object" :
                                                  "callable";
                    
                    runtimeError(this, "Type mismatch: Cannot assign value of type '" + actualTypeName + 
                                "' to variable of type '" + expectedTypeName + "'",
                                frames.empty() ? -1 : frames.back().currentLine);
                }
                
                stack[frame->slot_offset + slot] = value;
                break;
            }
            case (uint8_t)OpCode::OP_GET_PROPERTY: {
                std::string propertyName = READ_STRING();
                Value object = stack.back();
                
                if (object.type == ValueType::MODULE) {
                    Module* module = std::get<Module*>(object.as);
                    try {
                        Value property = module->get(propertyName);
                        stack.pop_back();
                        push(property);
                    } catch (const std::runtime_error& e) {
                        runtimeError(this, std::string(e.what()) + " Make sure the module is properly imported with 'use' statement.",
                                    frames.empty() ? -1 : frames.back().currentLine);
                    }
                } else if (object.type == ValueType::ARRAY) {
                    Array* arr = std::get<Array*>(object.as);
                    
                    if (propertyName == "length") {
                        stack.pop_back();
                        push(Value(static_cast<double>(arr->size())));
                    } else if (propertyName == "push" || propertyName == "pop" || 
                               propertyName == "slice" || propertyName == "map" ||
                               propertyName == "filter" || propertyName == "find" ||
                               propertyName == "indexOf" || propertyName == "join" ||
                               propertyName == "reverse" || propertyName == "sort") {
                        // Return a bound method that captures the array
                        stack.pop_back();
                        push(Value(allocate<BoundArrayMethod>(arr, propertyName)));
                    } else {
                        runtimeError(this, "Array does not have property '" + propertyName + "'.",
                                    frames.empty() ? -1 : frames.back().currentLine);
                    }
                } else if (object.type == ValueType::OBJ_STRING) {
                    // Handle string properties and methods
                    std::string str = object.asString()->chars;
                    
                    if (propertyName == "length") {
                        stack.pop_back();
                        push(Value(static_cast<double>(str.length())));
                    } else if (propertyName == "chars") {
                        // Return an array of individual characters
                        Array* charArray = allocate<Array>();
                        for (char c : str) {
                            charArray->push(Value(std::string(1, c)));
                        }
                        stack.pop_back();
                        push(Value(charArray));
                    } else if (propertyName == "contains" || propertyName == "split" || propertyName == "substring") {
                        // Return a bound method that captures the string
                        stack.pop_back();
                        push(Value(allocate<BoundStringMethod>(str, propertyName)));
                    } else {
                        runtimeError(this, "String does not have property '" + propertyName + "'.",
                                    frames.empty() ? -1 : frames.back().currentLine);
                    }
                } else if (object.type == ValueType::INSTANCE) {
                    // Handle instance properties and methods
                    Instance* inst = std::get<Instance*>(object.as);
                    
                    // Check fields first
                    auto it = inst->fields.find(propertyName);
                    if (it != inst->fields.end()) {
                        stack.pop_back();
                        push(it->second);
                    } else {
                        // Check methods in the class
                        auto methIt = inst->klass->methods.find(propertyName);
                        if (methIt != inst->klass->methods.end()) {
                            // Create a bound method that captures the instance
                            Value methodValue = methIt->second;
                            if (methodValue.type == ValueType::CALLABLE) {
                                Function* func = dynamic_cast<Function*>(std::get<Callable*>(methodValue.as));
                                if (func != nullptr) {
                                    stack.pop_back();
                                    push(Value(allocate<BoundMethod>(object, func)));
                                } else {
                                    // Not a Function, but still a callable - use as-is
                                    stack.pop_back();
                                    push(methodValue);
                                }
                            } else {
                                stack.pop_back();
                                push(methodValue);
                            }
                        } else {
                            runtimeError(this, "Property '" + propertyName + "' not found on instance.",
                                        frames.empty() ? -1 : frames.back().currentLine);
                        }
                    }
                } else if (object.type == ValueType::OBJECT) {
                    Object* objPtr = std::get<Object*>(object.as);
                    
                    // Check if it's a JsonObject
                    JsonObject* obj = dynamic_cast<JsonObject*>(objPtr);
                    if (obj != nullptr) {
                        auto it = obj->properties.find(propertyName);
                        if (it != obj->properties.end()) {
                            stack.pop_back();
                            push(it->second);
                        } else {
                            runtimeError(this, "Property '" + propertyName + "' not found on object.", frames.empty() ? -1 : frames.back().currentLine);
                        }
                    } else {
                        runtimeError(this, "Object does not support property access.", frames.empty() ? -1 : frames.back().currentLine);
                    }
                } else {
                    runtimeError(this, "Only modules, arrays, strings, and objects have properties. Cannot use dot notation on this value type.", frames.empty() ? -1 : frames.back().currentLine);
                }
                break;
            }
            case (uint8_t)OpCode::OP_SET_PROPERTY: {
                std::string propertyName = READ_STRING();
                Value value = pop();  // The value to set
                Value object = pop();  // The object/instance
                
                if (object.type == ValueType::INSTANCE) {
                    Instance* inst = std::get<Instance*>(object.as);
                    inst->fields[propertyName] = value;
                    push(value);  // Assignment expression returns the value
                } else if (object.type == ValueType::OBJECT) {
                    Object* objPtr = std::get<Object*>(object.as);
                    JsonObject* jsonObj = dynamic_cast<JsonObject*>(objPtr);
                    if (jsonObj != nullptr) {
                        jsonObj->properties[propertyName] = value;
                        push(value);
                    } else {
                        runtimeError(this, "Cannot set property on this object type.", frames.empty() ? -1 : frames.back().currentLine);
                    }
                } else {
                    runtimeError(this, "Only instances and objects support property assignment.", frames.empty() ? -1 : frames.back().currentLine);
                }
                break;
            }
            case (uint8_t)OpCode::OP_THIS: {
                // 'this' is stored as the first local variable (slot 0) in method calls
                CallFrame* frame = &frames[frames.size() - 1];
                push(stack[frame->slot_offset]);
                break;
            }
            case (uint8_t)OpCode::OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                
                bool result = false;
                if (a.type != b.type) {
                    result = false;
                } else if (a.type == ValueType::NIL) {
                    result = true;  // All nil values are equal
                } else if (a.type == ValueType::BOOLEAN) {
                    result = (std::get<bool>(a.as) == std::get<bool>(b.as));
                } else if (a.type == ValueType::NUMBER) {
                    result = (std::get<double>(a.as) == std::get<double>(b.as));
                } else if (a.type == ValueType::OBJ_STRING) {
                    result = (a.asString()->chars == b.asString()->chars);
                } else {
                    // For complex types (OBJECT, ARRAY, etc.), fall back to string comparison
                    result = (a.toString() == b.toString());
                }
                push(Value(result));
                break;
            }
            case (uint8_t)OpCode::OP_NOT_EQUAL: {
                Value b = pop();
                Value a = pop();
                
                bool result = false;
                if (a.type != b.type) {
                    result = true;
                } else if (a.type == ValueType::NIL) {
                    result = false;  // All nil values are equal
                } else if (a.type == ValueType::BOOLEAN) {
                    result = (std::get<bool>(a.as) != std::get<bool>(b.as));
                } else if (a.type == ValueType::NUMBER) {
                    result = (std::get<double>(a.as) != std::get<double>(b.as));
                } else if (a.type == ValueType::OBJ_STRING) {
                    result = (a.asString()->chars != b.asString()->chars);
                } else {
                    // For complex types (OBJECT, ARRAY, etc.), fall back to string comparison
                    result = (a.toString() != b.toString());
                }
                push(Value(result));
                break;
            }
            case (uint8_t)OpCode::OP_GREATER: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                push(Value(a > b));
                break;
            }
            case (uint8_t)OpCode::OP_LESS: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                push(Value(a < b));
                break;
            }
            case (uint8_t)OpCode::OP_ADD: {
                Value b = pop();
                Value a = pop();
                
                if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) {
                    double num_a = std::get<double>(a.as);
                    double num_b = std::get<double>(b.as);
                    push(Value(num_a + num_b));
                } else {
                    std::string str_a = a.toString();
                    std::string str_b = b.toString();
                    push(Value(str_a + str_b));
                }
                break;
            }
            case (uint8_t)OpCode::OP_SUBTRACT: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                push(Value(a - b));
                break;
            }
            case (uint8_t)OpCode::OP_MULTIPLY: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                push(Value(a * b));
                break;
            }
            case (uint8_t)OpCode::OP_DIVIDE: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                if (b == 0) {
                    runtimeError(this, "Division by zero.", frames.empty() ? -1 : frames.back().currentLine);
                }
                push(Value(a / b));
                break;
            }
            case (uint8_t)OpCode::OP_MODULO: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                if (b == 0) {
                    runtimeError(this, "Modulo by zero.", frames.empty() ? -1 : frames.back().currentLine);
                }
                push(Value(fmod(a, b)));
                break;
            }
            case (uint8_t)OpCode::OP_NOT: {
                Value value = pop();
                push(Value(!isTruthy(value)));
                break;
            }
            case (uint8_t)OpCode::OP_NEGATE: {
                if (stack.back().type != ValueType::NUMBER) {
                    runtimeError(this, "Operand must be a number.", frames.empty() ? -1 : frames.back().currentLine);
                }
                double value = std::get<double>(pop().as);
                push(Value(-value));
                break;
            }
            case (uint8_t)OpCode::OP_SAY: {
                std::cout << pop().toString() << std::endl;
                break;
            }
            case (uint8_t)OpCode::OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case (uint8_t)OpCode::OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                Value condition = pop();  // Pop the condition value
                if (!isTruthy(condition)) {
                    frame->ip += offset;
                }
                break;
            }
            case (uint8_t)OpCode::OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
                        case (uint8_t)OpCode::OP_CALL: {
                                uint8_t argCount = READ_BYTE();
                                // The function object should be at position [stack.size() - argCount - 1]
                                // Arguments are at positions [stack.size() - argCount] to [stack.size() - 1]
                                Value callee = stack[stack.size() - argCount - 1];
                                if (!callValue(callee, argCount)) {
                                        return;
                }
                                frame = &frames.back();
                                break;
            }
                        case (uint8_t)OpCode::OP_ARRAY: {
                                uint8_t count = READ_BYTE();
                                std::vector<Value> elements;
                                elements.reserve(count);
                
                // Pop 'count' elements from the stack in reverse order
                for (int i = 0; i < count; i++) {
                    elements.insert(elements.begin(), pop());
                }
                
                                push(Value(allocate<Array>(std::move(elements))));
                                break;
            }
                        case (uint8_t)OpCode::OP_OBJECT: {
                uint8_t count = READ_BYTE();
                auto obj = allocate<JsonObject>();
                
                // Pop 'count' pairs from the stack
                // Stack: [key1, val1, key2, val2] (top)
                // We pop val2, key2, val1, key1
                
                for (int i = 0; i < count; i++) {
                    Value val = pop();
                    Value key = pop();
                    
                    if (key.type != ValueType::OBJ_STRING) {
                        runtimeError(this, "Object keys must be strings.", frames.empty() ? -1 : frames.back().currentLine);
                    }
                    
                    obj->properties[key.asString()->chars] = val;
                }
                
                push(Value(obj));
                break;
            }
                        case (uint8_t)OpCode::OP_INDEX_GET: {
                                Value index = pop();
                                Value object = pop();
                
                                if (object.type == ValueType::ARRAY) {
                                        if (index.type != ValueType::NUMBER) {
                        runtimeError(this, "Array index must be a number.", frames.empty() ? -1 : frames.back().currentLine);
                                            return;
                    }
                    
                                        int idx = static_cast<int>(std::get<double>(index.as));
                    Array* array = std::get<Array*>(object.as);
                    
                                        if (idx < 0 || idx >= static_cast<int>(array->size())) {
                        std::string range = array->size() == 0 ? "[]" : "[0, " + std::to_string(array->size()-1) + "]";
                        std::string errorMsg = "Array index out of bounds: index " + std::to_string(idx) + 
                                              " is not within " + range;
                        runtimeError(this, errorMsg, 
                                    frames.empty() ? -1 : frames.back().currentLine);
                        return;
                    }
                    
                                        push(array->at(idx));
                } else if (object.type == ValueType::OBJ_STRING) {
                    if (index.type != ValueType::NUMBER) {
                        runtimeError(this, "String index must be a number.", frames.empty() ? -1 : frames.back().currentLine);
                        return;
                    }
                    
                    int idx = static_cast<int>(std::get<double>(index.as));
                    std::string str = object.asString()->chars;
                    
                    if (idx < 0 || idx >= static_cast<int>(str.length())) {
                        std::string range = str.length() == 0 ? "[]" : "[0, " + std::to_string(str.length()-1) + "]";
                        std::string errorMsg = "String index out of bounds: index " + std::to_string(idx) + 
                                              " is not within " + range;
                        runtimeError(this, errorMsg, 
                                    frames.empty() ? -1 : frames.back().currentLine);
                        return;
                    }
                    
                    // Return the character at the index as a string
                    push(Value(std::string(1, str[idx])));
                } else if (object.type == ValueType::BUFFER) {
                    if (index.type != ValueType::NUMBER) {
                        runtimeError(this, "Buffer index must be a number.", frames.empty() ? -1 : frames.back().currentLine);
                        return;
                    }
                    
                    int idx = static_cast<int>(std::get<double>(index.as));
                    Buffer* buffer = object.asBuffer();
                    
                    if (idx < 0 || idx >= static_cast<int>(buffer->size())) {
                        runtimeError(this, "Buffer index out of bounds.", frames.empty() ? -1 : frames.back().currentLine);
                        return;
                    }
                    
                    push(Value(static_cast<double>(buffer->get(idx))));
                } else if (object.type == ValueType::OBJECT) {
                    Object* objPtr = std::get<Object*>(object.as);
                    JsonObject* jsonObj = dynamic_cast<JsonObject*>(objPtr);
                    
                    if (jsonObj) {
                        if (index.type != ValueType::OBJ_STRING) {
                            runtimeError(this, "Object key must be a string.", frames.empty() ? -1 : frames.back().currentLine);
                            return;
                        }
                        std::string key = index.asString()->chars;
                        if (jsonObj->properties.count(key)) {
                            push(jsonObj->properties[key]);
                        } else {
                            push(Value()); // Return nil if key not found
                        }
                    } else {
                         runtimeError(this, "This object type does not support index access.", frames.empty() ? -1 : frames.back().currentLine);
                         return;
                    }
                } else {
                    runtimeError(this, "Only arrays, strings, buffers, and objects support index access.", frames.empty() ? -1 : frames.back().currentLine);
                    return;
                }
                                break;
            }
                        case (uint8_t)OpCode::OP_INDEX_SET: {
                Value value = pop();
                                Value index = pop();
                                Value object = pop();
                
                                if (object.type == ValueType::ARRAY) {
                                        if (index.type != ValueType::NUMBER) {
                        runtimeError(this, "Array index must be a number.", frames.empty() ? -1 : frames.back().currentLine);
                                            return;
                    }
                    
                                        int idx = static_cast<int>(std::get<double>(index.as));
                    Array* array = std::get<Array*>(object.as);
                    
                                        if (idx < 0 || idx >= static_cast<int>(array->size())) {
                        std::string range = array->size() == 0 ? "[]" : "[0, " + std::to_string(array->size()-1) + "]";
                        std::string errorMsg = "Array index out of bounds: index " + std::to_string(idx) + 
                                              " is not within " + range;
                        runtimeError(this, errorMsg, 
                                    frames.empty() ? -1 : frames.back().currentLine);
                        return;
                    }
                    
                                        array->set(idx, value);
                    push(value); // Return the assigned value
                } else if (object.type == ValueType::BUFFER) {
                    if (index.type != ValueType::NUMBER) {
                        runtimeError(this, "Buffer index must be a number.", frames.empty() ? -1 : frames.back().currentLine);
                        return;
                    }
                    
                    if (value.type != ValueType::NUMBER) {
                        runtimeError(this, "Buffer value must be a number (byte).", frames.empty() ? -1 : frames.back().currentLine);
                        return;
                    }
                    
                    int idx = static_cast<int>(std::get<double>(index.as));
                    int val = static_cast<int>(std::get<double>(value.as));
                    Buffer* buffer = object.asBuffer();
                    
                    if (idx < 0 || idx >= static_cast<int>(buffer->size())) {
                        runtimeError(this, "Buffer index out of bounds.", frames.empty() ? -1 : frames.back().currentLine);
                        return;
                    }
                    
                    if (val < 0 || val > 255) {
                        runtimeError(this, "Buffer value must be a byte (0-255).", frames.empty() ? -1 : frames.back().currentLine);
                        return;
                    }
                    
                    buffer->set(idx, static_cast<uint8_t>(val));
                    push(value);
                } else if (object.type == ValueType::OBJECT) {
                    Object* objPtr = std::get<Object*>(object.as);
                    JsonObject* jsonObj = dynamic_cast<JsonObject*>(objPtr);
                    
                    if (jsonObj) {
                        if (index.type != ValueType::OBJ_STRING) {
                            runtimeError(this, "Object key must be a string.", frames.empty() ? -1 : frames.back().currentLine);
                            return;
                        }
                        std::string key = index.asString()->chars;
                        jsonObj->properties[key] = value;
                        push(value);
                    } else {
                         runtimeError(this, "This object type does not support index assignment.", frames.empty() ? -1 : frames.back().currentLine);
                         return;
                    }
                } else {
                    runtimeError(this, "Only arrays, buffers, and objects support index assignment.", frames.empty() ? -1 : frames.back().currentLine);
                                        return;
                }
                                break;
            }
            case (uint8_t)OpCode::OP_TRY: {
                // Set up an exception handler frame
                // Read handler information from bytecode
                uint16_t tryEnd = READ_SHORT(); // End of try block
                uint16_t catchStart = READ_SHORT(); // Start of catch block (-1 if none)
                uint16_t finallyStart = READ_SHORT(); // Start of finally block (-1 if none)
                
                int currentIP = (frame->ip - 1) - frame->function->chunk->code.data(); // Position before reading shorts
                int currentFrameBase = frame->slot_offset;
                
                exceptionFrames.emplace_back(
                    currentIP, 
                    tryEnd, 
                    catchStart == 0xFFFF ? -1 : catchStart,  // 0xFFFF represents -1 (no handler)
                    finallyStart == 0xFFFF ? -1 : finallyStart, // 0xFFFF represents -1 (no handler)
                    currentFrameBase,
                    frame->fileName,
                    frame->currentLine
                );
                break;
            }
            case (uint8_t)OpCode::OP_END_TRY: {
                // End of try block - remove the exception handler frame
                if (!exceptionFrames.empty()) {
                    exceptionFrames.pop_back();
                }
                
                // For Neutron's exception handling semantics:
                // When a try-finally (without catch) executes and an exception was handled,
                // the exception is consumed (not re-thrown) after the finally block completes.
                // So we simply clear any pending exception state when END_TRY is reached.
                if (hasException) {
                    // Clear the pending exception - the finally block has executed
                    // and the exception is consumed (not re-thrown)
                    pendingException = Value();
                    hasException = false;
                }
                
                break;
            }
            case (uint8_t)OpCode::OP_THROW: {
                // A value has been pushed to the stack - this is the exception
                Value exception = pop();
                
                // Find the closest exception handler in the current call frame
                ExceptionFrame* handler = nullptr;
                for (int i = exceptionFrames.size() - 1; i >= 0; i--) {
                    ExceptionFrame& frame_handler = exceptionFrames[i];
                    int current_pos = frame->ip - frame->function->chunk->code.data() - 1; // -1 to account for the read byte
                    if (current_pos >= frame_handler.tryStart && current_pos <= frame_handler.tryEnd) {
                        handler = &frame_handler;
                        break;
                    }
                }
                
                if (!handler) {
                    // No exception handler found - runtime error
                    std::string errorMsg = "Uncaught exception: ";
                    if (exception.type == ValueType::OBJ_STRING) {
                        errorMsg += exception.asString()->chars;
                    } else {
                        errorMsg += exception.toString();
                    }
                    runtimeError(this, errorMsg, frames.empty() ? -1 : frames.back().currentLine);
                }
                
                // Store the exception info for re-throw after finally if needed
                // Don't remove exception frames yet if there's a finally block - we'll need them for re-throw
                if (handler->finallyStart != -1 && handler->finallyStart != 0xFFFF) {
                    // We'll handle the exception frames appropriately when the finally block completes
                    // Store the exception for later re-throw
                    pendingException = exception; // Store the exception to be re-thrown after finally
                    hasException = true;
                    
                    // Jump to the finally block
                    frame->ip = frame->function->chunk->code.data() + handler->finallyStart;
                    // Don't pop the exception frame yet - we'll need it when finally block completes
                    break; // Exit OP_THROW processing
                }
                
                // Adjust the stack to the frame base when the exception frame was created
                // This is stack unwinding to the exception frame scope
                stack.resize(handler->frameBase);
                
                // Remove all exception frames up to and including this one
                while (!exceptionFrames.empty() && 
                       &exceptionFrames.back() != handler) {
                    exceptionFrames.pop_back();
                }
                if (!exceptionFrames.empty()) {
                    exceptionFrames.pop_back(); // Remove the current handler
                }
                
                // Handle the exception according to the available handlers
                if (handler->catchStart != -1 && handler->catchStart != 0xFFFF) {
                    // There is a catch block - execute it
                    frame->ip = frame->function->chunk->code.data() + handler->catchStart;
                    
                    // Push the exception value onto the stack for the catch block
                    push(exception);
                } else if (handler->finallyStart != -1 && handler->finallyStart != 0xFFFF) {
                    // Only finally block exists - we need to execute it and then re-throw
                    // The exception has already been stored and frames retained earlier
                    // This case should not be reached now since we handle it above
                    // This is a fallback if somehow the logic gets here
                    runtimeError(this, "Internal error: Exception processing state inconsistent", frames.empty() ? -1 : frames.back().currentLine);
                } else {
                    // No catch and no finally - runtime error (shouldn't happen due to parser requirement)
                    runtimeError(this, "Exception occurred but no handler available", frames.empty() ? -1 : frames.back().currentLine);
                }
                
                break;
            }
        }
        }
    } catch (const VMException& e) {
        // Handle caught runtime exception
        push(e.value); // Push exception value
        
        // We need to find the handler again because we unwound the C++ stack
        // but the VM state (frames, exceptionFrames) is preserved.
        
        CallFrame* frame = &frames.back();
        int current_pos = frame->ip - frame->function->chunk->code.data() - 1;
        
        ExceptionFrame* handler = nullptr;
        for (int i = exceptionFrames.size() - 1; i >= 0; i--) {
            ExceptionFrame& frame_handler = exceptionFrames[i];
            if (current_pos >= frame_handler.tryStart && current_pos <= frame_handler.tryEnd) {
                handler = &frame_handler;
                break;
            }
        }
        
        if (handler) {
            // Adjust stack
            stack.resize(handler->frameBase);
            
            // Remove exception frames
            while (!exceptionFrames.empty() && &exceptionFrames.back() != handler) {
                exceptionFrames.pop_back();
            }
            exceptionFrames.pop_back(); // Remove handler itself
            
            push(e.value); // Push exception again after resize
            
            // Jump to catch block
            frame->ip = frame->function->chunk->code.data() + handler->catchStart;
            
            // Refresh frame pointer
            frame = &frames.back();
            
            // Continue outer loop
            continue;
        } else {
            // Should not happen if runtimeError checked correctly, but just in case
            runtimeError(this, e.value.toString(), frames.empty() ? -1 : frames.back().currentLine);
            return;
        }
    } catch (const std::runtime_error& e) {
        runtimeError(this, e.what(), frames.empty() ? -1 : frames.back().currentLine);
        return;
    }
    break; // Break outer loop if for(;;) exited normally
    } // End outer while(true)

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
}

void VM::define_native(const std::string& name, Callable* function) {
    globals[name] = Value(function);
}

void VM::add_module_search_path(const std::string& path) {
    module_search_paths.push_back(path);
}

void VM::define_module(const std::string& name, Module* module) {
    globals[name] = Value(module);
}

void VM::define(const std::string& name, const Value& value) {
    globals[name] = value;
}

Value VM::call(const Value& callee, const std::vector<Value>& arguments) {
    lock();
    try {
        if (callee.type != ValueType::CALLABLE) {
            throw std::runtime_error("Can only call functions.");
        }
        
        Callable* function = std::get<Callable*>(callee.as);
        
        // Save current stack state
        size_t stack_size = stack.size();
        
        Value result;

        if (BoundMethod* bound = dynamic_cast<BoundMethod*>(function)) {
             if (bound->method->arity() != -1 && bound->method->arity() != (int)arguments.size()) {
                 throw std::runtime_error("Expected " + std::to_string(bound->method->arity()) + " arguments but got " + std::to_string(arguments.size()) + ".");
             }

             // Push receiver and arguments
             push(bound->receiver);
             for (const auto& arg : arguments) {
                 push(arg);
             }
             
             // Manually set up frame for BoundMethod
             if (frames.size() == 256) {
                throw std::runtime_error("Stack overflow.");
             }
             CallFrame* frame = &frames.emplace_back();
             frame->function = bound->method;
             if (bound->method->chunk->code.empty()) {
                 frame->ip = nullptr;
             } else {
                 frame->ip = bound->method->chunk->code.data();
             }
             // slot_offset points to the receiver (one position before args)
             frame->slot_offset = stack.size() - arguments.size() - 1;
             frame->fileName = currentFileName;
             frame->currentLine = -1;
             frame->isBoundMethod = true;

             run(frames.size() - 1);
             
             if (stack.size() > stack_size) {
                 result = stack.back();
             } else {
                 result = Value();
             }

        } else if (Function* func = dynamic_cast<Function*>(function)) {
             if (func->arity() != -1 && func->arity() != (int)arguments.size()) {
                 throw std::runtime_error("Expected " + std::to_string(func->arity()) + " arguments but got " + std::to_string(arguments.size()) + ".");
             }

             // Push callee (for OP_RETURN cleanup consistency)
             push(callee);
             for (const auto& arg : arguments) {
                 push(arg);
             }
             
             call(func, arguments.size());
             
             run(frames.size() - 1);
             
             if (stack.size() > stack_size) {
                 result = stack.back();
             } else {
                 result = Value();
             }

        } else {
             // NativeFn or other Callable
             if (function->arity() != -1 && function->arity() != (int)arguments.size()) {
                 throw std::runtime_error("Expected " + std::to_string(function->arity()) + " arguments but got " + std::to_string(arguments.size()) + ".");
             }

             // Push callee and args for consistency
             push(callee);
             for (const auto& arg : arguments) {
                 push(arg);
             }

             result = function->call(*this, arguments);
        }
        
        // Clean up any remaining stack items
        while (stack.size() > stack_size) {
            pop();
        }
        
        unlock();
        return result;
    } catch (...) {
        unlock();
        throw;
    }
}

Value VM::execute_string(const std::string& source) {
    lock();
    try {
        // Parse and execute the Neutron code string
        Scanner scanner(source);
        std::vector<Token> tokens = scanner.scanTokens();
        
        Parser parser(tokens);
        std::vector<std::unique_ptr<Stmt>> statements = parser.parse();
        
        // Compile and run the code
        Compiler compiler(*this);
        Function* function = compiler.compile(statements);
        
        if (function) {
            // Save current state
            size_t stack_size = stack.size();
            
            // Execute the function
            interpret(function);
            
            // Get the return value (if any)
            Value result;
            if (stack.size() > stack_size) {
                result = pop();
            }
            
            // Clean up
            delete function;
            
            unlock();
            return result;
        }
        
        unlock();
        return Value(); // Return nil if compilation failed
    } catch (...) {
        unlock();
        throw;
    }
}

void VM::load_module(const std::string& name) {
    // Check if module is already loaded in the cache
    if (loadedModuleCache.find(name) != loadedModuleCache.end()) {
        return; // Module already loaded
    }
    
    // Check if module is already defined as a global
    if (globals.find(name) != globals.end()) {
        loadedModuleCache[name] = true;
        return; // Module already loaded
    }

    // Check for built-in modules first
    if (name == "json") {
        neutron_init_json_module(this);
        loadedModuleCache[name] = true;
        return;
    } else if (name == "fmt") {
        neutron_init_fmt_module(this);
        loadedModuleCache[name] = true;
        return;
    } else if (name == "arrays") {
        neutron_init_arrays_module(this);
        loadedModuleCache[name] = true;
        return;
    } else if (name == "time") {
        neutron_init_time_module(this);
        loadedModuleCache[name] = true;
        return;
    } else if (name == "http") {
        neutron_init_http_module(this);
        loadedModuleCache[name] = true;
        return;
    } else if (name == "math") {
        neutron_init_math_module(this);
        loadedModuleCache[name] = true;
        return;
    } else if (name == "sys") {
        neutron_init_sys_module(this);
        loadedModuleCache[name] = true;
        return;
    } else if (name == "async") {
        neutron_init_async_module(this);
        loadedModuleCache[name] = true;
        return;
    } else if (name == "regex") {
        neutron_init_regex_module(this);
        loadedModuleCache[name] = true;
        return;
    }

    std::string module_nt_path = "box/" + name + "/" + name + ".nt";

    // Search paths for .nt modules
    std::vector<std::string> search_paths = {
        ".box/modules/" + name + "/",  // .box/modules/name/name.nt (Box installed)
        "box/" + name + "/",  // box/name/name.nt
        "box/",               // box/name.nt
        "dev_tests/",         // dev_tests/name.nt
        "lib/",               // lib/name.nt
        ""                    // ./name.nt (current directory)
    };

    // First, try to find a .nt module file in the search paths
    std::string found_nt_path;
    std::ifstream module_nt_file;
    
    for (const auto& path : search_paths) {
        std::string test_path;
        if (path == "box/" + name + "/") {
            test_path = path + name + ".nt";
        } else {
            test_path = path + name + ".nt";
        }
        
        module_nt_file.open(test_path);
        if (module_nt_file.is_open()) {
            found_nt_path = test_path;
            break;
        }
    }

    // If we found a .nt file, load it as a module
    if (module_nt_file.is_open()) {
        // It's a Neutron module, we need to execute it.
        std::string source((std::istreambuf_iterator<char>(module_nt_file)),
                            std::istreambuf_iterator<char>());
        module_nt_file.close();
        
        // Parse the module
        Scanner scanner(source);
        std::vector<Token> tokens = scanner.scanTokens();
        Parser parser(tokens);
        std::vector<std::unique_ptr<Stmt>> statements = parser.parse();
        
        // Create a module environment
        auto module_env = std::make_shared<Environment>();
        
        // Save current globals
        auto saved_globals = globals;
        
        // Create a temporary VM with the module environment as globals
        // This is a hack to make the compiler use the module environment
        globals.clear();
        
        // Create a temporary compiler and execute the module code
        Compiler compiler(*this);
        Function* module_function = compiler.compile(statements);
        if (module_function) {
            // Execute the module to populate its functions/variables in the module environment
            interpret(module_function);
            delete module_function;  // Clean up the allocated function
        }
        
        // Copy the defined values from globals to the module environment
        for (const auto& pair : globals) {
            module_env->define(pair.first, pair.second);
        }
        
        // Restore globals
        globals = saved_globals;
        
        // Create the module with the populated environment
        auto module = allocate<Module>(name, module_env);
        define_module(name, module);
        loadedModuleCache[name] = true;
        return;
    }

    // Try to load as a native shared library module
    void* handle = nullptr;
    
    // Search for shared library in module search paths
    std::vector<std::string> lib_extensions;
#ifdef __APPLE__
    lib_extensions.push_back(".dylib");
    lib_extensions.push_back(".so");
#elif defined(_WIN32) || defined(WIN32)
    lib_extensions.push_back(".dll");
#else
    lib_extensions.push_back(".so");
#endif

    std::vector<std::string> lib_prefixes = {"", "lib"};
    
    // Add .box/modules/name/ to search paths for backward compatibility
    std::vector<std::string> native_search_paths = module_search_paths;
    native_search_paths.push_back(".box/modules/" + name + "/");

    for (const auto& path : native_search_paths) {
        for (const auto& prefix : lib_prefixes) {
            for (const auto& ext : lib_extensions) {
                std::string try_path = path;
                if (!try_path.empty() && try_path.back() != '/') {
                    try_path += "/";
                }
                try_path += prefix + name + ext;
                
                std::ifstream f(try_path);
                if (f.good()) {
                    f.close();
                    handle = dlopen(try_path.c_str(), RTLD_LAZY);
                    if (handle) break;
                }
            }
            if (handle) break;
        }
        if (handle) break;
    }
    
    if (handle) {
        // It's a native module, we need to load it.
        auto module_env = std::make_shared<Environment>();
        
        // Save current globals
        auto saved_globals = globals;
        
        // Create a temporary VM with the module environment as globals
        globals.clear();
        
        void (*init_func)(VM*) = (void (*)(VM*))dlsym(handle, "neutron_module_init");
        if (!init_func) {
            dlclose(handle);
            runtimeError(this, "Module '" + name + "' is not a valid Neutron module: missing neutron_module_init function.",
                        frames.empty() ? -1 : frames.back().currentLine);
        }
        init_func(this);
        
        // Copy the defined values from globals to the module environment
        for (const auto& pair : globals) {
            module_env->define(pair.first, pair.second);
        }
        
        // Restore globals
        globals = saved_globals;
        
        // Create the module with the populated environment
        auto module = allocate<Module>(name, module_env, handle);
        define_module(name, module);
        loadedModuleCache[name] = true;
        return;
    } else {
        // Handle error: module not found
        runtimeError(this, "Module '" + name + "' not found. Make sure to use 'use " + name + ";' before accessing it.",

                    frames.empty() ? -1 : frames.back().currentLine);
    }
}

void VM::addEmbeddedFile(const std::string& path, const std::string& content) {
    embeddedFiles[path] = content;
}

void VM::load_file(const std::string& filepath) {
    std::string source;
    bool found = false;

    // Check embedded files first
    auto it = embeddedFiles.find(filepath);
    if (it != embeddedFiles.end()) {
        source = it->second;
        found = true;
    } else {
        // Try to open the file
        std::ifstream file(filepath);
        if (!file.is_open()) {
            // Try with module search paths
            for (const auto& search_path : module_search_paths) {
                std::string full_path = search_path + filepath;
                
                // Check embedded files in search paths
                auto it2 = embeddedFiles.find(full_path);
                if (it2 != embeddedFiles.end()) {
                    source = it2->second;
                    found = true;
                    break;
                }

                file.open(full_path);
                if (file.is_open()) {
                    break;
                }
            }
        }
        
        if (file.is_open()) {
            source = std::string((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            file.close();
            found = true;
        }
    }

    if (!found) {
        runtimeError(this, "File '" + filepath + "' not found.",
                    frames.empty() ? -1 : frames.back().currentLine);
        return;
    }
    
    // Parse the file
    Scanner scanner(source);
    std::vector<Token> tokens = scanner.scanTokens();
    Parser parser(tokens);
    std::vector<std::unique_ptr<Stmt>> statements = parser.parse();
    
    // Compile and execute the file in the current global scope
    Compiler compiler(*this);
    Function* file_function = compiler.compile(statements);
    if (file_function) {
        // Execute the file to populate globals
        interpret(file_function);
        delete file_function;
    }
}

Module* VM::load_file_as_module(const std::string& filepath) {
    std::string source;
    bool found = false;

    // Check embedded files first
    auto it = embeddedFiles.find(filepath);
    if (it != embeddedFiles.end()) {
        source = it->second;
        found = true;
    } else {
        // Try to open the file
        std::ifstream file(filepath);
        if (!file.is_open()) {
            // Try with module search paths
            for (const auto& search_path : module_search_paths) {
                std::string full_path = search_path + filepath;
                
                // Check embedded files in search paths
                auto it2 = embeddedFiles.find(full_path);
                if (it2 != embeddedFiles.end()) {
                    source = it2->second;
                    found = true;
                    break;
                }

                file.open(full_path);
                if (file.is_open()) {
                    break;
                }
            }
        }
        
        if (file.is_open()) {
            source = std::string((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            file.close();
            found = true;
        }
    }

    if (!found) {
        runtimeError(this, "File '" + filepath + "' not found.",
                    frames.empty() ? -1 : frames.back().currentLine);
        return nullptr;
    }
    
    // Parse the file
    Scanner scanner(source);
    std::vector<Token> tokens = scanner.scanTokens();
    Parser parser(tokens);
    std::vector<std::unique_ptr<Stmt>> statements = parser.parse();
    
    // Create a new environment for the module
    auto module_env = std::make_shared<Environment>();
    
    // Save current globals
    auto saved_globals = globals;
    
    // Create a temporary VM state with the module environment as globals
    // Note: We can't easily swap globals map with Environment, so we'll 
    // use a different approach: interpret_module helper
    
    interpret_module(statements, module_env);
    
    // Create the module
    return allocate<Module>(filepath, module_env);
}

void VM::registerComponent(std::shared_ptr<ComponentInterface> component) {
    if (component) {
        // Check if component is compatible before loading
        if (component->isCompatible()) {
            loadedComponents.push_back(component);
            component->initialize(this);
        }
    }
}

std::vector<std::shared_ptr<ComponentInterface>> VM::getComponents() const {
    return loadedComponents;
}

std::shared_ptr<ComponentInterface> VM::getComponent(const std::string& name) const {
    for (const auto& component : loadedComponents) {
        if (component->getName() == name) {
            return component;
        }
    }
    return nullptr;
}

void VM::collectGarbage() {
    // Mark all reachable objects
    markRoots();
    
    // Trace references
    traceReferences();
    
    // Sweep all unreachable objects
    sweep();
    
    // Set next collection threshold (double the current live size)
    nextGC = heap.size() * 2;
}

void VM::markRoots() {
    // Mark all objects on the stack
    for (const auto& value : stack) {
        markValue(value);
    }
    
    // Mark all global objects
    for (const auto& pair : globals) {
        markValue(pair.second);
    }
    
    // Mark objects in call frame functions (only functions since closures are part of them)
    for (const auto& frame : frames) {
        if (frame.function) {
            // Mark the function itself
            markObject(frame.function);
        }
    }

    // Mark temporary roots
    for (Object* obj : tempRoots) {
        markObject(obj);
    }
}

void VM::markObject(Object* obj) {
    if (obj == nullptr || obj->is_marked) return;
    obj->mark();
    grayStack.push_back(obj);
}

void VM::traceReferences() {
    while (!grayStack.empty()) {
        Object* obj = grayStack.back();
        grayStack.pop_back();
        blackenObject(obj);
    }
}

void VM::blackenObject(Object* obj) {
    if (auto* arr = dynamic_cast<Array*>(obj)) {
        for (const auto& element : arr->elements) markValue(element);
    } else if (auto* bam = dynamic_cast<BoundArrayMethod*>(obj)) {
        markObject(bam->array);
    } else if (auto* json_obj = dynamic_cast<JsonObject*>(obj)) {
        for (const auto& prop : json_obj->properties) markValue(prop.second);
    } else if (auto* json_arr = dynamic_cast<JsonArray*>(obj)) {
        for (const auto& element : json_arr->elements) markValue(element);
    } else if (auto* inst = dynamic_cast<Instance*>(obj)) {
        for (const auto& field : inst->fields) markValue(field.second);
        markObject(inst->klass);
    } else if (dynamic_cast<Buffer*>(obj)) {
        // Buffer has no references to other objects
    } else if (auto* module = dynamic_cast<Module*>(obj)) {
        if (module->env) {
            for (const auto& pair : module->env->values) markValue(pair.second);
        }
    } else if (auto* bm = dynamic_cast<BoundMethod*>(obj)) {
        markValue(bm->receiver);
        markObject(bm->method);
    } else if (auto* func = dynamic_cast<Function*>(obj)) {
        if (func->closure) {
            // Mark all values in the closure environment chain
            auto env = func->closure;
            while (env) {
                for (const auto& pair : env->values) markValue(pair.second);
                env = env->enclosing;
            }
        }
        if (func->chunk) {
            for (const auto& constant : func->chunk->constants) markValue(constant);
        }
    } else if (auto* klass = dynamic_cast<Class*>(obj)) {
        if (klass->class_env) {
            for (const auto& pair : klass->class_env->values) markValue(pair.second);
        }
        for (const auto& pair : klass->methods) {
            markValue(pair.second);
        }
    }
}

void VM::markValue(const Value& value) {
    Object* obj = nullptr;
    switch (value.type) {
        case ValueType::OBJ_STRING: obj = std::get<ObjString*>(value.as); break;
        case ValueType::ARRAY: obj = std::get<Array*>(value.as); break;
        case ValueType::OBJECT: obj = std::get<Object*>(value.as); break;
        case ValueType::CALLABLE: obj = std::get<Callable*>(value.as); break;
        case ValueType::MODULE: obj = std::get<Module*>(value.as); break;
        case ValueType::CLASS: obj = std::get<Class*>(value.as); break;
        case ValueType::INSTANCE: obj = std::get<Instance*>(value.as); break;
        default: return;
    }
    markObject(obj);
}

void VM::sweep() {
    auto it = heap.begin();
    while (it != heap.end()) {
        if (!(*it)->is_marked) {
            // Object is not marked, so delete it
            Object* obj = *it;
            delete obj;
            it = heap.erase(it);
        } else {
            // Object is marked, unmark it and keep it
            (*it)->is_marked = false;
            ++it;
        }
    }
}

bool VM::handleException(const Value& exception) {
    // Find the most recent handler that covers the current IP position
    for (int i = exceptionFrames.size() - 1; i >= 0; i--) {
        ExceptionFrame& handler = exceptionFrames[i];
        CallFrame* frame = &frames.back();
        int current_pos = frame->ip - frame->function->chunk->code.data() - 1; // -1 to account for the read byte
        
        if (current_pos >= handler.tryStart && current_pos <= handler.tryEnd) {
            // Found a matching handler
            
            // Adjust the stack to the frame base when the exception frame was created
            stack.resize(handler.frameBase);
            
            // Remove all exception frames up to and including this one
            while (!exceptionFrames.empty() && 
                   &exceptionFrames.back() != &handler) {
                exceptionFrames.pop_back();
            }
            if (!exceptionFrames.empty()) {
                exceptionFrames.pop_back(); // Remove the current handler
            }
            
            // Jump to the catch block if it exists
            if (handler.catchStart != -1 && handler.catchStart != 0xFFFF) {
                frame->ip = frame->function->chunk->code.data() + handler.catchStart;
                
                // Push the exception value onto the stack for the catch block
                push(exception);
                
                return true; // Exception handled
            } else if (handler.finallyStart != -1 && handler.finallyStart != 0xFFFF) {
                // If only finally exists, go to finally block
                frame->ip = frame->function->chunk->code.data() + handler.finallyStart;
                // In a complete implementation, we'd need to handle the fact that 
                // the exception wasn't caught but needs to be re-thrown after finally
                // For now, just execute finally and re-throw
                push(exception); // Store exception for potential re-throw after finally
                
                return true; // Exception processing started
            }
        }
    }
    
    // No exception handler found
    return false;
}

void VM::interpret_module(const std::vector<std::unique_ptr<Stmt>>& statements, std::shared_ptr<Environment> module_env) {
    // Compile
    Compiler compiler(*this);
    Function* function = compiler.compile(statements);
    if (!function) return;

    // Save current globals
    auto saved_globals = globals;
    
    // Prepare module globals
    globals.clear();
    // Copy existing module environment values to globals
    for (const auto& pair : module_env->values) {
        globals[pair.first] = pair.second;
    }
    
    // Also need to ensure standard library functions are available if they are not in module_env?
    // Usually modules should have access to standard globals like 'say', 'print', etc.
    // But if we clear globals, they won't have access unless we copy them.
    // However, 'say' is defined in VM constructor.
    // If we want the module to have access to standard globals, we should copy from saved_globals.
    // But we want to isolate the module's definitions from polluting the main globals.
    // So we should copy saved_globals to globals (so module can use them), 
    // execute, and then ONLY copy NEW definitions back to module_env.
    // But how do we distinguish new definitions?
    // We can check if they existed before.
    
    // Better approach:
    // 1. Start with globals = saved_globals (so module can use 'say', etc.)
    // 2. Execute module.
    // 3. Iterate globals. If a key was NOT in saved_globals (or value changed?), add to module_env.
    //    But wait, if module redefines 'say', it shouldn't affect main 'say'.
    //    This suggests we should copy.
    
    globals = saved_globals; 
    
    // But if we just use globals = saved_globals, then OP_DEFINE_GLOBAL will modify 'globals', 
    // which IS the map we are using.
    // So we are modifying the copy.
    // Wait, 'globals' is a member variable: std::unordered_map<std::string, Value> globals;
    // So 'globals = saved_globals' copies the map.
    // Modifications to 'globals' won't affect 'saved_globals'.
    // So this is safe!
    
    try {
        push(Value(function));
        call(function, 0);
        run();
    } catch (...) {
        globals = saved_globals;
        throw;
    }
    
    // Extract definitions from the module execution
    // We want to capture everything that is now in 'globals' but wasn't in 'saved_globals',
    // OR was updated.
    // But typically modules define new things.
    // If we want to capture *everything* the module sees as global, we copy all globals to module_env.
    // But that would include 'say', etc.
    // Do we want 'say' in the module export?
    // Probably not.
    // We only want things defined IN the module.
    
    // This is hard to track without a separate "module scope".
    // But for now, let's just copy everything that is NOT in the initial standard globals?
    // Or maybe we accept that module_env will contain standard globals too.
    // If we import (a) from module, and module has 'say', we might import 'say'.
    // That's probably fine.
    
    // However, to be cleaner, we could track what was added.
    for (const auto& pair : globals) {
        // If it wasn't in saved_globals, it's definitely new.
        if (saved_globals.find(pair.first) == saved_globals.end()) {
            module_env->define(pair.first, pair.second);
        } else {
            // It was in saved_globals. Did it change?
            // If it changed, we might want to export it?
            // Or maybe we just export everything.
            // If we export everything, then `use (say) = from module` works even if module didn't define say but inherited it.
            // That seems acceptable.
            module_env->define(pair.first, pair.second);
        }
    }
    
    // Restore original globals
    globals = saved_globals;
}
}
