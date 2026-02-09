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
#include "types/bound_string_method.h"
#include "types/string_method_registry.h"
#include "types/string_formatter.h"
#include "types/buffer.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#if defined(_WIN32) || defined(WIN32)
#include "cross-platfrom/dlfcn_compat.h"
#else
#include <dlfcn.h>
#endif
#include <cmath>
#include <cstring>
#include <algorithm>

#include "capi.h"
#include "../libs/json/native.h"
#include "../libs/fmt/native.h"
#include "../libs/time/native.h"
#include "../libs/arrays/native.h"
#include "../libs/crypto/native.h"
#include "../libs/path/native.h"
#include "../libs/random/native.h"
#include "../libs/math/native.h"
#include "../libs/http/native.h"
#include "../libs/async/native.h"
#include "../libs/regex/native.h"
#include "../libs/process/native.h"
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

#ifdef _MSC_VER
__forceinline
#else
inline __attribute__((always_inline))
#endif
bool isTruthy(const Value& value) {
    // Fast path for common cases
    if (value.type == ValueType::BOOLEAN) return value.as.boolean;
    return value.type != ValueType::NIL;
}

/**
 * @brief Reports a runtime error, produces a stack trace, and either transfers control to an in-flight handler or terminates.
 *
 * If an active exception handler that covers the current instruction is found, the VM frames are unwound to that handler
 * and a VMException is thrown to resume execution in the VM's run loop. If no handler is found, the error and stack trace
 * are reported and the process is terminated.
 *
 * @param vm The VM instance where the error occurred.
 * @param message The error message to report.
 * @param line Optional source line number to use as the error location; if -1 the line is inferred from the stack trace.
 *
 * @throws VMException When a matching in-flight exception handler is located; used to transfer control back into the VM.
 */
[[noreturn]] void runtimeError(VM* vm, const std::string& message, int line = -1) {
    // Check if there is an active exception handler that covers the current instruction
    // We need to search up the call stack
    
    // Iterate exception frames backwards (most recent first)
    for (size_t idx = vm->exceptionFrames.size(); idx > 0; idx--) {
        size_t i = idx - 1;
        VM::ExceptionFrame& handler = vm->exceptionFrames[i];
        
        // Find the call frame that owns this handler
        // The handler belongs to the frame where frame->slot_offset == handler.frameBase
        CallFrame* frame = nullptr;
        size_t frameIndex = 0;
        bool foundFrame = false;
        
        for (size_t jdx = vm->frames.size(); jdx > 0; jdx--) {
            size_t j = jdx - 1;
            if (vm->frames[j].slot_offset == handler.frameBase) {
                frame = &vm->frames[j];
                frameIndex = j;
                foundFrame = true;
                break;
            }
        }
        
        if (foundFrame && frame) {
            // Calculate current position in that frame
            ptrdiff_t current_pos;
            if (frame == &vm->frames.back()) {
                // Current frame: use current IP
                if (frame->function && frame->function->chunk) {
                    current_pos = frame->ip - frame->function->chunk->code.data() - 1;
                } else {
                    continue;
                }
            } else {
                // Parent frame: IP points to return address (instruction after call)
                // The call instruction is what we are "in".
                // We assume the call instruction is at least 1 byte.
                if (frame->function && frame->function->chunk) {
                    current_pos = frame->ip - frame->function->chunk->code.data() - 1;
                } else {
                    continue;
                }
            }
            
            if (current_pos >= handler.tryStart && current_pos <= handler.tryEnd) {
                // Found a handler!
                
                // Unwind stack to this frame
                // We need to pop frames until this frame is the top one
                while (vm->frames.size() > (size_t)frameIndex + 1) {
                    vm->frames.pop_back();
                }
                
                // Unwind exception frames
                // Keep handlers up to this one (inclusive)
                vm->exceptionFrames.resize(i + 1);
                
                // Throw VMException to be caught in VM::run
                throw VMException(Value(message));
            }
        }
    }

    // Build stack trace
    std::vector<StackFrame> stackTrace;
    
    for (auto it = vm->frames.rbegin(); it != vm->frames.rend(); ++it) {
        std::string funcName = "<script>";
        if (it->function) {
            if (it->function->declaration) {
                funcName = it->function->declaration->name.lexeme;
            } else if (!it->function->name.empty()) {
                funcName = it->function->name;
            } else {
                funcName = "<anonymous>";
            }
        }
        
        int frameLine = -1;
        if (it->function && it->function->chunk && it->ip) {
            size_t offset = it->ip - it->function->chunk->code.data();
            if (offset > 0) offset--;
            if (offset < it->function->chunk->lines.size()) {
                frameLine = it->function->chunk->lines[offset];
            }
        }
        
        if (frameLine == -1) frameLine = it->currentLine > 0 ? it->currentLine : line;
        
        stackTrace.emplace_back(funcName, (it->fileName == nullptr || it->fileName->empty()) ? vm->currentFileName : *it->fileName, frameLine);
    }
    
    int errorLine = line;
    if (errorLine == -1 && !stackTrace.empty()) {
        errorLine = stackTrace[0].line;
    }
    
    ErrorHandler::reportRuntimeError(message, vm->currentFileName, errorLine, stackTrace);
    ErrorHandler::printSummary();
    exit(1);
}

VM::VM() : ip(nullptr), nextGC(32768), currentFileName("<stdin>"), hasException(false), pendingException(Value()), isSafeFile(false) {  // Start GC at 32768 objects
    // Reserve moderate stack - benchmarks rarely exceed a few hundred slots.
    // Grows automatically if needed. Big reserve (1M) wastes 16MB at startup.
    stack.reserve(8192);
    frames.reserve(64);
    heap.reserve(16384);
    
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
    // Don't manually delete objects - they should be managed by GC
    // Just clear the heap vector
    heap.clear();
    // Free pooled instances
    for (Instance* inst : instancePool) {
        delete inst;
    }
    instancePool.clear();
}

Instance* VM::allocateInstance(Class* klass) {
    Instance* inst;
    if (!instancePool.empty()) {
        // Reuse from pool
        inst = instancePool.back();
        instancePool.pop_back();
        inst->reset(klass);
    } else {
        inst = new Instance(klass);
    }
    heap.push_back(inst);
    
    if (heap.size() >= nextGC) {
        tempRoots.push_back(inst);
        collectGarbage();
        if (!tempRoots.empty() && tempRoots.back() == inst) {
            tempRoots.pop_back();
        }
    }
    return inst;
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
    : name(), arity_val(0), chunk(new Chunk()), declaration(declaration), closure(closure) {
    obj_type = ObjType::OBJ_FUNCTION;
    
    // Extract parameter types from declaration
    if (declaration) {
        for (const auto& param : declaration->params) {
            if (param.typeAnnotation.has_value()) {
                paramTypes.push_back(param.typeAnnotation.value().type);
            } else {
                paramTypes.push_back(std::nullopt);
            }
        }
        
        // Extract return type
        if (declaration->returnType.has_value()) {
            returnType = declaration->returnType.value().type;
        }
    }
}

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
        if (name == "<script>") return "<script>";
        return "<fn " + name + ">";
    }
    return "<fn <anonymous>>";
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

    // Check parameter types if function has type annotations
    // Skip entirely if no type annotations (common case for benchmarks)
    if (!function->paramTypes.empty() && function->paramTypes.size() == static_cast<size_t>(argCount)) {
        for (int i = 0; i < argCount; i++) {
            if (function->paramTypes[i].has_value()) {
                Value arg = stack[stack.size() - argCount + i];
                TokenType expectedType = function->paramTypes[i].value();
                
                // Check type compatibility
                bool isValid = false;
                switch (expectedType) {
                    case TokenType::TYPE_INT:
                    case TokenType::TYPE_FLOAT:
                        isValid = arg.type == ValueType::NUMBER;
                        break;
                    case TokenType::TYPE_STRING:
                        isValid = arg.type == ValueType::OBJ_STRING;
                        break;
                    case TokenType::TYPE_BOOL:
                        isValid = arg.type == ValueType::BOOLEAN;
                        break;
                    case TokenType::TYPE_ARRAY:
                        isValid = arg.type == ValueType::ARRAY;
                        break;
                    case TokenType::TYPE_OBJECT:
                        isValid = arg.type == ValueType::OBJECT;
                        break;
                    case TokenType::TYPE_ANY:
                        isValid = true;
                        break;
                    default:
                        isValid = true; // Unknown types are allowed
                        break;
                }
                
                if (!isValid) {
                    std::string funcName = function->declaration ? function->declaration->name.lexeme : "<anonymous>";
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
                        default:
                            expectedTypeName = "unknown";
                            break;
                    }
                    
                    std::string actualTypeName = arg.type == ValueType::NIL ? "nil" : 
                                                  arg.type == ValueType::BOOLEAN ? "boolean" :
                                                  arg.type == ValueType::NUMBER ? "number" :
                                                  arg.type == ValueType::OBJ_STRING ? "string" :
                                                  arg.type == ValueType::ARRAY ? "array" :
                                                  arg.type == ValueType::OBJECT ? "object" :
                                                  "callable";
                    
                    runtimeError(this, "Type mismatch in function '" + funcName + "' parameter " + std::to_string(i + 1) + 
                                ": expected '" + expectedTypeName + "' but got '" + actualTypeName + "'",
                                frames.empty() ? -1 : frames.back().currentLine);
                    return false;
                }
            }
        }
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
    frame->fileName = &currentFileName;
    frame->currentLine = -1;

    return true;
}

bool VM::callValue(Value callee, int argCount) {
    // Handle CLASS type first (common in OOP-heavy code)
    if (callee.type == ValueType::CLASS) {
        Class* klass = callee.as.klass;
        
        // Create instance
        Instance* instance = allocateInstance(klass);
        Value instanceVal(instance);
        
        // Check for initializer
        if (klass->initializer) {
            Function* initializer = klass->initializer;
            
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
            frame->fileName = &currentFileName;
            frame->currentLine = -1;
            frame->isBoundMethod = true;
            frame->isInitializer = true;
            return true;
        } 
        
        // No initializer or not a function
        if (argCount != 0) {
            runtimeError(this, "Expected 0 arguments but got " + std::to_string(argCount) + " for constructor.", frames.empty() ? -1 : frames.back().currentLine);
            return false;
        }
        
        stack[stack.size() - argCount - 1] = instanceVal;
        stack.resize(stack.size() - argCount);
        return true;
    }

    // Handle BoundArrayMethod and BoundStringMethod via obj_type tag (no RTTI)
    if (callee.type == ValueType::OBJECT) {
        Object* obj = callee.as.object;
        if (obj->obj_type == ObjType::OBJ_BOUND_ARRAY_METHOD) {
            return callArrayMethod(static_cast<BoundArrayMethod*>(obj), argCount);
        } else if (obj->obj_type == ObjType::OBJ_BOUND_STRING_METHOD) {
            return callStringMethod(static_cast<BoundStringMethod*>(obj), argCount);
        }
    }
    
    if (callee.type == ValueType::CALLABLE) {
        Callable* callable = callee.as.callable;
        
        // Use obj_type tag instead of dynamic_cast chain for fast dispatch
        switch (callable->obj_type) {
        case ObjType::OBJ_BOUND_METHOD: {
            BoundMethod* boundMethod = static_cast<BoundMethod*>(callable);
            size_t methodPos = stack.size() - argCount - 1;
            stack[methodPos] = boundMethod->receiver;
            if (boundMethod->method->arity_val != argCount) {
                std::string funcName = boundMethod->method->declaration ? 
                                     boundMethod->method->declaration->name.lexeme : "<method>";
                runtimeError(this, "Expected " + std::to_string(boundMethod->method->arity_val) + " arguments but got " + std::to_string(argCount) + " for method '" + funcName + "'.", frames.empty() ? -1 : frames.back().currentLine);
                return false;
            }
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
            frame->slot_offset = stack.size() - argCount - 1;
            frame->fileName = &currentFileName;
            frame->currentLine = -1;
            frame->isBoundMethod = true;
            return true;
        }
        case ObjType::OBJ_FUNCTION: {
            Function* function = static_cast<Function*>(callable);
            return call(function, argCount);
        }
        case ObjType::OBJ_NATIVE_FN: {
            NativeFn* native = static_cast<NativeFn*>(callable);
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
        }
        default: {
            if (callable->isCNativeFn()) {
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
            break;
        }
        } // end switch
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
            int start = static_cast<int>(args[0].as.number);
            int end = static_cast<int>(args[1].as.number);
            
            if (start < 0) start = 0;
            if (end > static_cast<int>(arr->size())) end = static_cast<int>(arr->size());
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
                    return a.as.number < b.as.number;
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
        
    } catch (const VMException&) {
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
        // Use the registry to get the method handler
        StringMethodRegistry& registry = StringMethodRegistry::getInstance();
        StringMethodHandler* handler = registry.getMethod(methodName);
        
        if (handler == nullptr) {
            runtimeError(this, "Unknown string method: " + methodName, frames.empty() ? -1 : frames.back().currentLine);
            return false;
        }
        
        // Validate arguments
        if (!handler->validateArgs(args)) {
            int expectedArity = handler->getArity();
            std::string errorMsg = methodName + "() ";
            if (expectedArity >= 0) {
                errorMsg += "expects " + std::to_string(expectedArity) + " arguments, got " + std::to_string(argCount);
            } else {
                errorMsg += "received invalid arguments. Expected: " + handler->getDescription();
            }
            runtimeError(this, errorMsg, frames.empty() ? -1 : frames.back().currentLine);
            return false;
        }
        
        // Execute the method
        result = handler->execute(this, str, args);
        
        // Restore stack to original size and push result
        stack.resize(stackBase);
        push(result);
        return true;
        
    } catch (const VMException&) {
        throw;
    } catch (const std::runtime_error& e) {
        runtimeError(this, e.what(), frames.empty() ? -1 : frames.back().currentLine);
        return false;
    }
}

void VM::run(size_t minFrameDepth) {
    // Stack operations - use direct vector access for speed
    std::vector<Value>& stk = stack;  // Local reference to avoid this-> indirection
    
    auto syncStack = [&]() {};
    auto reloadStack = [&]() {};

    // Inline push for hot path
    #define FAST_PUSH(val) stk.push_back(val)
    #define FAST_POP() (stk.pop_back())
    #define FAST_PEEK(d) (stk[stk.size() - 1 - (d)])
    #define FAST_TOP() (stk.back())
    
    auto push = [&](const Value& value) {
        stk.push_back(value);
    };
    
    auto pop = [&]() -> Value {
        Value v = stk.back();
        stk.pop_back();
        return v;
    };

    auto peek = [&](int distance) -> Value& {
        return stk[stk.size() - 1 - distance];
    };

    // =====================================================================
    // GLOBAL VARIABLE CACHE: Avoids hash map lookup on every OP_GET_GLOBAL.
    // Uses ObjString* as key (interned strings have unique addresses).
    // Cache is indexed by (constant_idx & 127) for fast lookup.
    // =====================================================================
    struct GlobalCacheEntry {
        ObjString* key;    // Interned string name (nullptr = empty slot)
        Value* value;      // Direct pointer into globals map
    };
    GlobalCacheEntry global_cache[128];
    std::memset(global_cache, 0, sizeof(global_cache));

    // Per-callsite method cache for OP_INVOKE: 8 entries indexed by bytecode IP hash.
    // Each OP_INVOKE callsite maps to its own cache slot, avoiding thrashing when
    // multiple methods are called in a loop (e.g., initialize + dist).
    struct MethodCacheEntry {
        Class* klass;
        ObjString* method_name;
        Function* method;
        const uint8_t* callsite_ip; // bytecode IP at the OP_INVOKE site
    };
    static constexpr size_t METHOD_CACHE_SIZE = 8;
    MethodCacheEntry method_cache[METHOD_CACHE_SIZE] = {};

    // Per-callsite property inline cache for OP_GET_PROPERTY on instances.
    // Caches the inline field index so we skip the linear scan in getField().
    struct PropCacheEntry {
        const uint8_t* callsite_ip;
        Class* klass;
        uint8_t inline_index;  // cached index into inlineFields[]
    };
    static constexpr size_t PROP_CACHE_SIZE = 8;
    PropCacheEntry prop_cache[PROP_CACHE_SIZE] = {};

    // Pre-computed small integer Values for OP_CONST_INT8 fast path.
    // Indexed by unsigned byte value (0-255). int8_t reinterpretation
    // gives the actual constant value.
    static Value small_int_table[256];
    static bool small_int_initialized = false;
    if (!small_int_initialized) {
        for (int i = 0; i < 256; i++) {
            small_int_table[i] = Value(static_cast<double>(static_cast<int8_t>(i)));
        }
        small_int_initialized = true;
    }

    CallFrame* frame = &frames.back();

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk->constants[READ_BYTE()])
#define READ_STRING() (READ_CONSTANT().as.obj_string->chars)

// Computed goto optimization (disabled by default due to C++ scope rules)
// Only enable on GCC/Clang - MSVC doesn't support computed goto
#if defined(__GNUC__) || defined(__clang__)
    #define COMPUTED_GOTO 1
#else
    #define COMPUTED_GOTO 0
#endif

#if COMPUTED_GOTO
    static void* dispatch_table[] = {
        &&CASE_OP_RETURN,
        &&CASE_OP_CONSTANT,
        &&CASE_OP_CONSTANT_LONG,
        &&CASE_OP_NIL,
        &&CASE_OP_TRUE,
        &&CASE_OP_FALSE,
        &&CASE_OP_POP,
        &&CASE_OP_DUP,
        &&CASE_OP_GET_LOCAL,
        &&CASE_OP_SET_LOCAL,
        &&CASE_OP_GET_GLOBAL,
        &&CASE_OP_DEFINE_GLOBAL,
        &&CASE_OP_SET_GLOBAL,
        &&CASE_OP_SET_GLOBAL_TYPED,
        &&CASE_OP_SET_LOCAL_TYPED,
        &&CASE_OP_DEFINE_TYPED_GLOBAL,
        &&CASE_OP_GET_PROPERTY,
        &&CASE_OP_SET_PROPERTY,
        &&CASE_OP_EQUAL,
        &&CASE_OP_GREATER,
        &&CASE_OP_LESS,
        &&CASE_OP_ADD,
        &&CASE_OP_SUBTRACT,
        &&CASE_OP_MULTIPLY,
        &&CASE_OP_DIVIDE,
        &&CASE_OP_MODULO,
        &&CASE_OP_NOT,
        &&CASE_OP_NEGATE,
        &&CASE_OP_SAY,
        &&CASE_OP_JUMP,
        &&CASE_OP_JUMP_IF_FALSE,
        &&CASE_OP_LOOP,
        &&CASE_OP_CALL,
        &&CASE_OP_CLOSURE,
        &&CASE_DEFAULT, // OP_GET_UPVALUE
        &&CASE_DEFAULT, // OP_SET_UPVALUE
        &&CASE_DEFAULT, // OP_CLOSE_UPVALUE
        &&CASE_OP_ARRAY,
        &&CASE_OP_OBJECT,
        &&CASE_OP_INDEX_GET,
        &&CASE_OP_INDEX_SET,
        &&CASE_OP_THIS,
        &&CASE_DEFAULT, // OP_BREAK
        &&CASE_DEFAULT, // OP_CONTINUE
        &&CASE_OP_TRY,
        &&CASE_OP_END_TRY,
        &&CASE_OP_THROW,
        &&CASE_OP_NOT_EQUAL,
        &&CASE_DEFAULT, // OP_LOGICAL_AND
        &&CASE_OP_VALIDATE_SAFE_FUNCTION,
        &&CASE_OP_VALIDATE_SAFE_VARIABLE,
        &&CASE_OP_VALIDATE_SAFE_FILE_FUNCTION,
        &&CASE_OP_VALIDATE_SAFE_FILE_VARIABLE,
        &&CASE_DEFAULT, // OP_LOGICAL_OR
        &&CASE_OP_BITWISE_AND,
        &&CASE_OP_BITWISE_OR,
        &&CASE_OP_BITWISE_XOR,
        &&CASE_OP_BITWISE_NOT,
        &&CASE_OP_LEFT_SHIFT,
        &&CASE_OP_RIGHT_SHIFT,
        &&CASE_OP_INVOKE,
        &&CASE_OP_INCREMENT_LOCAL,
        &&CASE_OP_DECREMENT_LOCAL,
        &&CASE_OP_LOOP_IF_LESS_LOCAL,
        &&CASE_OP_INCREMENT_GLOBAL,
        // Extended opcodes (bytecode optimizer)
        &&CASE_OP_CALL,              // OP_CALL_FAST → same as OP_CALL
        &&CASE_OP_CALL,              // OP_TAIL_CALL → same as OP_CALL (for now)
        &&CASE_OP_GET_GLOBAL_FAST,
        &&CASE_OP_SET_GLOBAL_FAST,
        &&CASE_OP_LOAD_LOCAL_0,
        &&CASE_OP_LOAD_LOCAL_1,
        &&CASE_OP_LOAD_LOCAL_2,
        &&CASE_OP_LOAD_LOCAL_3,
        &&CASE_OP_CONST_ZERO,
        &&CASE_OP_CONST_ONE,
        &&CASE_OP_CONST_INT8,
        &&CASE_OP_ADD_INT,
        &&CASE_OP_SUB_INT,
        &&CASE_OP_MUL_INT,
        &&CASE_OP_DIV_INT,
        &&CASE_OP_MOD_INT,
        &&CASE_OP_NEGATE_INT,
        &&CASE_OP_LESS_INT,
        &&CASE_OP_GREATER_INT,
        &&CASE_OP_EQUAL_INT,
        &&CASE_OP_INC_LOCAL_INT,
        &&CASE_OP_DEC_LOCAL_INT,
        &&CASE_OP_LESS_JUMP,
        &&CASE_OP_GREATER_JUMP,
        &&CASE_OP_EQUAL_JUMP,
        &&CASE_OP_ADD_LOCAL_CONST,
        &&CASE_DEFAULT,              // OP_TYPE_GUARD → nop
        &&CASE_DEFAULT,              // OP_LOOP_HINT → nop
    };
    #define DISPATCH() goto *dispatch_table[READ_BYTE()]
    #define CASE(op) CASE_##op:
#else
    #define DISPATCH() break
    #define CASE(op) case (uint8_t)OpCode::op:
#endif

    while (true) {
    try {
#if COMPUTED_GOTO
        DISPATCH();
#else
        for (;;) {
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
#endif
            CASE(OP_RETURN) {
                Value result = stk.back();
                stk.pop_back();
                size_t return_slot_offset = frame->slot_offset;
                bool was_bound_method = frame->isBoundMethod;
                bool was_initializer = frame->isInitializer;
                
                Value returnValue;
                if (__builtin_expect(!was_initializer, 1)) {
                    returnValue = result;
                } else {
                    // For initializers, return 'this' instead of the function result
                    if (return_slot_offset < stk.size()) {
                        returnValue = stk[return_slot_offset];
                    } else {
                        returnValue = result;
                    }
                }

                frames.pop_back();
                
                if (__builtin_expect(!frames.empty() && frames.size() > minFrameDepth, 1)) {
                    frame = &frames.back();
                    if (__builtin_expect(was_bound_method, 1)) {
                        // Combine resize + push into direct write
                        stk[return_slot_offset] = returnValue;
                        stk.resize(return_slot_offset + 1);
                    } else if (return_slot_offset > 0) {
                        stk[return_slot_offset - 1] = returnValue;
                        stk.resize(return_slot_offset);
                    } else {
                        stk.clear();
                        stk.push_back(returnValue);
                    }
                    DISPATCH();
                }

                // Slow path: returning from top-level or target depth
                if (was_bound_method) {
                    stack.resize(return_slot_offset);
                } else if (return_slot_offset > 0) {
                    if (stack.size() >= return_slot_offset - 1) {
                        stack.resize(return_slot_offset - 1);
                    }
                } else {
                    stack.clear();
                }
                push(returnValue);
                syncStack();
                return;
            }
            CASE(OP_CONSTANT) {
                Value constant = READ_CONSTANT();
                push(constant);
                DISPATCH();
            }
            CASE(OP_CONSTANT_LONG) {
                // Read 16-bit constant index (big-endian) - sequence reads to avoid undefined behavior
                uint8_t high = READ_BYTE();
                uint8_t low = READ_BYTE();
                uint16_t constantIndex = (high << 8) | low;
                Value constant = frame->function->chunk->constants[constantIndex];
                push(constant);
                DISPATCH();
            }
            CASE(OP_CLOSURE) {
                Value constant = READ_CONSTANT();
                // The constant should be a Function
                if (constant.type == ValueType::CALLABLE) {
                    push(constant);
                } else {
                    runtimeError(this, "OP_CLOSURE constant must be a function.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
            CASE(OP_NIL) {
                FAST_PUSH(Value());
                DISPATCH();
            }
            CASE(OP_TRUE) {
                FAST_PUSH(Value(true));
                DISPATCH();
            }
            CASE(OP_FALSE) {
                FAST_PUSH(Value(false));
                DISPATCH();
            }
            CASE(OP_POP) {
                stk.pop_back();
                DISPATCH();
            }
            CASE(OP_DUP) {
                FAST_PUSH(stk.back());
                DISPATCH();
            }
            CASE(OP_GET_LOCAL) {
                uint8_t slot = READ_BYTE();
                FAST_PUSH(stk[frame->slot_offset + slot]);
                DISPATCH();
            }
            CASE(OP_SET_LOCAL) {
                uint8_t slot = READ_BYTE();
                stk[frame->slot_offset + slot] = stk.back();
                DISPATCH();
            }
            CASE(OP_GET_GLOBAL) {
                uint8_t idx = READ_BYTE();
                ObjString* nameStr = frame->function->chunk->constants[idx].as.obj_string;
                uint8_t slot = idx & 127;
                GlobalCacheEntry& entry = global_cache[slot];
                if (__builtin_expect(entry.key == nameStr, 1)) {
                    FAST_PUSH(*entry.value);
                } else {
                    const std::string& name = nameStr->chars;
                    auto it = globals.find(name);
                    if (it == globals.end()) {
                        if (name == "json" || name == "math" || name == "sys" || name == "http" || 
                            name == "time" || name == "fmt" || name == "arrays") {
                            runtimeError(this, "Undefined variable '" + name + "'. Did you forget to import it? Use 'use " + name + ";' at the top of your file.", 
                                        frames.empty() ? -1 : frames.back().currentLine);
                        } else {
                            runtimeError(this, "Undefined variable '" + name + "'.", 
                                        frames.empty() ? -1 : frames.back().currentLine);
                        }
                    }
                    entry.key = nameStr;
                    entry.value = &(it->second);
                    FAST_PUSH(it->second);
                }
                DISPATCH();
            }
            CASE(OP_DEFINE_GLOBAL) {
                const std::string& name = READ_STRING();
                globals[name] = peek(0);
                pop();
                // Invalidate global cache — map may have rehashed
                std::memset(global_cache, 0, sizeof(global_cache));
                DISPATCH();
            }
            CASE(OP_DEFINE_TYPED_GLOBAL) {
                const std::string& name = READ_STRING();
                TokenType type = static_cast<TokenType>(READ_BYTE());
                
                globals[name] = peek(0);
                globalTypes[name] = type;  // Store the type information
                pop();
                // Invalidate global cache — map may have rehashed
                std::memset(global_cache, 0, sizeof(global_cache));
                DISPATCH();
            }
            CASE(OP_VALIDATE_SAFE_FUNCTION) {
                // Validate that the function on top of stack has proper type annotations for safe block
                Value functionValue = peek(0);
                if (functionValue.type == ValueType::CALLABLE) {
                    Function* function = dynamic_cast<Function*>(functionValue.as.callable);
                    if (function && function->declaration) {
                        // Check that all parameters have type annotations
                        for (const auto& param : function->declaration->params) {
                            if (!param.typeAnnotation.has_value()) {
                                if (this->isSafeFile) {
                                    throw VMException(Value("Function parameter '" + param.name.lexeme + "' must have a type annotation in .ntsc files (Neutron Safe Code)."));
                                } else {
                                    throw VMException(Value("Function parameter '" + param.name.lexeme + "' must have a type annotation inside a safe block."));
                                }
                            }
                        }
                        
                        // Check that function has a return type annotation
                        if (!function->declaration->returnType.has_value()) {
                            throw VMException(Value("Function '" + function->declaration->name.lexeme + "' must have a return type annotation inside a safe block."));
                        }
                    }
                }
                DISPATCH();
            }
            CASE(OP_VALIDATE_SAFE_VARIABLE) {
                {
                // Validate that a variable has a type annotation in safe block
                std::string varName = READ_STRING();
                if (this->isSafeFile) {
                    throw VMException(Value("Variable '" + varName + "' must have a type annotation in .ntsc files (Neutron Safe Code)."));
                } else {
                    throw VMException(Value("Variable '" + varName + "' must have a type annotation inside a safe block."));
                }
                }
                DISPATCH();
            }
            CASE(OP_VALIDATE_SAFE_FILE_FUNCTION) {
                // Validate that the function on top of stack has proper type annotations for safe file
                Value functionValue = peek(0);
                if (functionValue.type == ValueType::CALLABLE) {
                    Function* function = dynamic_cast<Function*>(functionValue.as.callable);
                    if (function && function->declaration) {
                        // Check that all parameters have type annotations
                        for (const auto& param : function->declaration->params) {
                            if (!param.typeAnnotation.has_value()) {
                                throw VMException(Value("Function parameter '" + param.name.lexeme + "' must have a type annotation in safe file (.ntsc)."));
                            }
                        }
                        
                        // Check that function has a return type annotation
                        if (!function->declaration->returnType.has_value()) {
                            throw VMException(Value("Function '" + function->declaration->name.lexeme + "' must have a return type annotation in safe file (.ntsc)."));
                        }
                    }
                }
                DISPATCH();
            }
            CASE(OP_VALIDATE_SAFE_FILE_VARIABLE) {
                // Validate that a variable has a type annotation in safe file
                const std::string& varName = READ_STRING();
                throw VMException(Value("Variable '" + varName + "' must have a type annotation in safe file (.ntsc)."));
                DISPATCH();
            }
            CASE(OP_SET_GLOBAL) {
                uint8_t idx = READ_BYTE();
                ObjString* nameStr = frame->function->chunk->constants[idx].as.obj_string;
                uint8_t slot = idx & 127;
                GlobalCacheEntry& entry = global_cache[slot];
                if (__builtin_expect(entry.key == nameStr, 1)) {
                    *entry.value = peek(0);
                } else {
                    auto it = globals.find(nameStr->chars);
                    if (__builtin_expect(it == globals.end(), 0)) {
                        runtimeError(this, "Undefined variable '" + nameStr->chars + "'.", 
                                    frames.empty() ? -1 : frames.back().currentLine);
                    }
                    entry.key = nameStr;
                    entry.value = &(it->second);
                    it->second = peek(0);
                }
                DISPATCH();
            }
            CASE(OP_SET_GLOBAL_TYPED) {
                const std::string& name = READ_STRING();
                
                auto it = globals.find(name);
                if (it == globals.end()) {
                    runtimeError(this, "Undefined variable '" + name + "'.", 
                                frames.empty() ? -1 : frames.back().currentLine);
                }
                
                // Look up the expected type for this global variable
                auto typeIt = globalTypes.find(name);
                if (typeIt == globalTypes.end()) {
                    // If no type is stored, fall back to regular assignment
                    globals[name] = peek(0);
                    DISPATCH();
                }
                
                TokenType expectedType = typeIt->second;
                Value value = peek(0);
                
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
                DISPATCH();
            }
            CASE(OP_SET_LOCAL_TYPED) {
                uint8_t slot = READ_BYTE();
                TokenType expectedType = static_cast<TokenType>(READ_BYTE());
                
                Value value = peek(0);
                
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
                DISPATCH();
            }
            CASE(OP_GET_PROPERTY) {
                const uint8_t* prop_callsite = frame->ip;
                ObjString* propertyNameObj = READ_CONSTANT().as.obj_string;
                Value object = peek(0);
                
                if (__builtin_expect(object.type == ValueType::INSTANCE, 1)) {
                    // Handle instance properties and methods (most common case)
                    Instance* inst = object.as.instance;
                    
                    // Per-callsite property inline cache: skip getField() linear scan
                    size_t pc_idx = (reinterpret_cast<uintptr_t>(prop_callsite) >> 1) & (PROP_CACHE_SIZE - 1);
                    PropCacheEntry& pc = prop_cache[pc_idx];
                    if (__builtin_expect(pc.callsite_ip == prop_callsite && pc.klass == inst->klass, 1)) {
                        stack.back() = inst->inlineFields[pc.inline_index].value;
                        DISPATCH();
                    }
                    
                    // Check fields first using inline/overflow lookup
                    Value* fieldVal = inst->getField(propertyNameObj);
                    if (__builtin_expect(fieldVal != nullptr, 1)) {
                        // Populate property cache if field is inline
                        for (uint8_t fi = 0; fi < inst->inlineCount; ++fi) {
                            if (inst->inlineFields[fi].key == propertyNameObj) {
                                pc.callsite_ip = prop_callsite;
                                pc.klass = inst->klass;
                                pc.inline_index = fi;
                                break;
                            }
                        }
                        stack.back() = *fieldVal;
                    } else {
                        // Check methods in the class
                        auto methIt = inst->klass->methods.find(propertyNameObj);
                        if (methIt != inst->klass->methods.end()) {
                            // Create a bound method that captures the instance
                            Value methodValue = methIt->second;
                            if (methodValue.type == ValueType::CALLABLE) {
                                Callable* c = methodValue.as.callable;
                                if (c->obj_type == ObjType::OBJ_FUNCTION) {
                                    stack.back() = Value(allocate<BoundMethod>(object, static_cast<Function*>(c)));
                                } else {
                                    stack.back() = methodValue;
                                }
                            } else {
                                stack.back() = methodValue;
                            }
                        } else {
                            const std::string& propertyName = propertyNameObj->chars;
                            runtimeError(this, "Property '" + propertyName + "' not found on instance.",
                                        frames.empty() ? -1 : frames.back().currentLine);
                        }
                    }
                } else if (object.type == ValueType::MODULE) {
                    const std::string& propertyName = propertyNameObj->chars;
                    Module* module = object.as.module;
                    try {
                        Value property = module->get(propertyName);
                        stack.pop_back();
                        push(property);
                    } catch (const std::runtime_error& e) {
                        runtimeError(this, std::string(e.what()) + " Make sure the module is properly imported with 'use' statement.",
                                    frames.empty() ? -1 : frames.back().currentLine);
                    }
                } else if (object.type == ValueType::ARRAY) {
                    const std::string& propertyName = propertyNameObj->chars;
                    Array* arr = object.as.array;
                    
                    if (propertyName == "length") {
                        stack.pop_back();
                        push(Value(static_cast<double>(arr->size())));
                    } else if (propertyName == "push" || propertyName == "pop" || 
                               propertyName == "slice" || propertyName == "map" ||
                               propertyName == "filter" || propertyName == "find" ||
                               propertyName == "indexOf" || propertyName == "join" ||
                               propertyName == "reverse" || propertyName == "sort") {
                        stack.pop_back();
                        push(Value(allocate<BoundArrayMethod>(arr, propertyName)));
                    } else {
                        runtimeError(this, "Array does not have property '" + propertyName + "'.",
                                    frames.empty() ? -1 : frames.back().currentLine);
                    }
                } else if (object.type == ValueType::OBJ_STRING) {
                    const std::string& propertyName = propertyNameObj->chars;
                    ObjString* strObj = object.as.obj_string;
                    
                    if (propertyName == "length") {
                        stk.back() = Value(static_cast<double>(strObj->chars.length()));
                    } else if (propertyName == "chars") {
                        Array* charArray = allocate<Array>();
                        for (char c : strObj->chars) {
                            charArray->push(Value(internString(std::string(1, c))));
                        }
                        stack.pop_back();
                        push(Value(charArray));
                    } else if (StringMethodRegistry::getInstance().hasMethod(propertyName)) {
                        stack.pop_back();
                        push(Value(allocate<BoundStringMethod>(strObj->chars, propertyName)));
                    } else {
                        runtimeError(this, "String does not have property '" + propertyName + "'.",
                                    frames.empty() ? -1 : frames.back().currentLine);
                    }
                } else if (object.type == ValueType::OBJECT) {
                    const std::string& propertyName = propertyNameObj->chars;
                    Object* objPtr = object.as.object;
                    
                    if (objPtr->obj_type == ObjType::OBJ_JSON_OBJECT) {
                        JsonObject* obj = static_cast<JsonObject*>(objPtr);
                        auto it = obj->properties.find(propertyNameObj);
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
                DISPATCH();
            }
            CASE(OP_SET_PROPERTY) {
                ObjString* propertyNameObj = READ_CONSTANT().as.obj_string;
                Value& value = stk.back();
                Value& object = stk[stk.size() - 2];
                
                if (object.type == ValueType::INSTANCE) {
                    Instance* inst = object.as.instance;
                    inst->setField(propertyNameObj, value);
                    stk[stk.size() - 2] = value;
                    stk.pop_back();
                } else if (object.type == ValueType::OBJECT) {
                    Object* objPtr = object.as.object;
                    if (objPtr->obj_type == ObjType::OBJ_JSON_OBJECT) {
                        JsonObject* jsonObj = static_cast<JsonObject*>(objPtr);
                        jsonObj->properties[propertyNameObj] = value;
                        stk[stk.size() - 2] = value;
                        stk.pop_back();
                    } else {
                        runtimeError(this, "Cannot set property on this object type.", frames.empty() ? -1 : frames.back().currentLine);
                    }
                } else {
                    runtimeError(this, "Only instances and objects support property assignment.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
            CASE(OP_THIS) {
                // 'this' is stored as the first local variable (slot 0) in method calls
                FAST_PUSH(stk[frame->slot_offset]);
                DISPATCH();
            }
            CASE(OP_EQUAL) {
                Value& b = peek(0);
                Value& a = peek(1);
                
                // Fast path for numbers (most common in loops)
                if (__builtin_expect(a.type == ValueType::NUMBER && b.type == ValueType::NUMBER, 1)) {
                    bool result = (a.as.number == b.as.number);
                    a.type = ValueType::BOOLEAN;
                    a.as.boolean = result;
                    stack.pop_back();
                    DISPATCH();
                }
                
                bool result = false;
                if (a.type != b.type) {
                    result = false;
                } else if (a.type == ValueType::NIL) {
                    result = true;  // All nil values are equal
                } else if (a.type == ValueType::BOOLEAN) {
                    result = (a.as.boolean == b.as.boolean);
                } else if (a.type == ValueType::NUMBER) {
                    result = (a.as.number == b.as.number);
                } else if (a.type == ValueType::OBJ_STRING) {
                    // Fast pointer equality for interned strings
                    ObjString* sa = a.as.obj_string;
                    ObjString* sb = b.as.obj_string;
                    result = (sa == sb) || (sa->chars == sb->chars);
                } else {
                    // For complex types (OBJECT, ARRAY, etc.), fall back to string comparison
                    result = (a.toString() == b.toString());
                }
                
                // Reuse 'a' slot for result
                a = Value(result);
                stack.pop_back();
                DISPATCH();
            }
            CASE(OP_NOT_EQUAL) {
                Value& b = peek(0);
                Value& a = peek(1);
                
                bool result = false;
                if (a.type != b.type) {
                    result = true;
                } else if (a.type == ValueType::NIL) {
                    result = false;  // All nil values are equal
                } else if (a.type == ValueType::BOOLEAN) {
                    result = (a.as.boolean != b.as.boolean);
                } else if (a.type == ValueType::NUMBER) {
                    result = (a.as.number != b.as.number);
                } else if (a.type == ValueType::OBJ_STRING) {
                    ObjString* sa = a.as.obj_string;
                    ObjString* sb = b.as.obj_string;
                    result = (sa != sb) && (sa->chars != sb->chars);
                } else {
                    // For complex types (OBJECT, ARRAY, etc.), fall back to string comparison
                    result = (a.toString() != b.toString());
                }
                
                // Reuse 'a' slot for result
                a = Value(result);
                stack.pop_back();
                DISPATCH();
            }
            CASE(OP_GREATER) {
                size_t sz = stk.size();
                Value& b = stk[sz - 1];
                Value& a = stk[sz - 2];
                if (__builtin_expect(a.type == ValueType::NUMBER && b.type == ValueType::NUMBER, 1)) {
                    bool result = a.as.number > b.as.number;
                    a.type = ValueType::BOOLEAN;
                    a.as.boolean = result;
                    stk.pop_back();
                } else {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
            CASE(OP_LESS) {
                size_t sz = stk.size();
                Value& b = stk[sz - 1];
                Value& a = stk[sz - 2];
                if (__builtin_expect(a.type == ValueType::NUMBER && b.type == ValueType::NUMBER, 1)) {
                    bool result = a.as.number < b.as.number;
                    a.type = ValueType::BOOLEAN;
                    a.as.boolean = result;
                    stk.pop_back();
                } else {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
            CASE(OP_ADD) {
                size_t sz = stk.size();
                Value& b = stk[sz - 1];
                Value& a = stk[sz - 2];
                
                if (__builtin_expect(a.type == ValueType::NUMBER, 1)) {
                    if (__builtin_expect(b.type == ValueType::NUMBER, 1)) {
                        a.as.number += b.as.number;
                        stk.pop_back();
                        DISPATCH();
                    }
                }
                if (a.type == ValueType::OBJ_STRING && b.type == ValueType::OBJ_STRING) {
                    ObjString* strA = a.as.obj_string;
                    ObjString* strB = b.as.obj_string;
                    if (!strA->isInterned) {
                        // Safe in-place append: strA is a unique data string (from makeString),
                        // not shared through the intern table.
                        strA->chars.append(strB->chars);
                        strA->hashComputed = false;
                        stk.pop_back();
                    } else {
                        // strA is interned (shared), must allocate a new string
                        std::string result;
                        result.reserve(strA->chars.size() + strB->chars.size());
                        result.append(strA->chars);
                        result.append(strB->chars);
                        a = Value(makeString(std::move(result)));
                        stk.pop_back();
                    }
                } else {
                    // Fast path for string + number (very common in formatting)
                    if (a.type == ValueType::OBJ_STRING && b.type == ValueType::NUMBER) {
                        ObjString* strA = a.as.obj_string;
                        char buf[32];
                        double num = b.as.number;
                        int len;
                        // Format integer values without decimal point
                        if (num == static_cast<double>(static_cast<int64_t>(num)) && 
                            num >= -999999999999999LL && num <= 999999999999999LL) {
                            len = snprintf(buf, sizeof(buf), "%lld", (long long)static_cast<int64_t>(num));
                        } else {
                            len = snprintf(buf, sizeof(buf), "%.15g", num);
                        }
                        if (!strA->isInterned) {
                            strA->chars.append(buf, len);
                            strA->hashComputed = false;
                            stk.pop_back();
                        } else {
                            std::string result;
                            result.reserve(strA->chars.size() + len);
                            result.append(strA->chars);
                            result.append(buf, len);
                            a = Value(makeString(std::move(result)));
                            stk.pop_back();
                        }
                    } else if (a.type == ValueType::NUMBER && b.type == ValueType::OBJ_STRING) {
                        ObjString* strB = b.as.obj_string;
                        char buf[32];
                        double num = a.as.number;
                        int len;
                        if (num == static_cast<double>(static_cast<int64_t>(num)) && 
                            num >= -999999999999999LL && num <= 999999999999999LL) {
                            len = snprintf(buf, sizeof(buf), "%lld", (long long)static_cast<int64_t>(num));
                        } else {
                            len = snprintf(buf, sizeof(buf), "%.15g", num);
                        }
                        std::string result;
                        result.reserve(len + strB->chars.size());
                        result.append(buf, len);
                        result.append(strB->chars);
                        a = Value(makeString(std::move(result)));
                        stk.pop_back();
                    } else {
                        Value val_b = pop();
                        Value val_a = pop();
                        std::string str_a = val_a.toString();
                        std::string str_b = val_b.toString();
                        push(Value(makeString(str_a + str_b)));
                    }
                }
                DISPATCH();
            }
            CASE(OP_SUBTRACT) {
                size_t sz = stk.size();
                Value& b = stk[sz - 1];
                Value& a = stk[sz - 2];
                if (__builtin_expect(a.type == ValueType::NUMBER && b.type == ValueType::NUMBER, 1)) {
                    a.as.number -= b.as.number;
                    stk.pop_back();
                } else {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
            CASE(OP_MULTIPLY) {
                size_t sz = stk.size();
                Value& b = stk[sz - 1];
                Value& a = stk[sz - 2];
                
                // Fast path: number * number (most common)
                if (__builtin_expect(a.type == ValueType::NUMBER && b.type == ValueType::NUMBER, 1)) {
                    a.as.number *= b.as.number;
                    stk.pop_back();
                    DISPATCH();
                }
                
                // Handle string * int and int * string
                if (a.type == ValueType::OBJ_STRING && b.type == ValueType::NUMBER) {
                    // string * int
                    {
                        std::string str = a.asString()->chars;
                        int count = static_cast<int>(b.as.number);
                        
                        if (count < 0) count = 0;
                        
                        std::string result;
                        result.reserve(str.length() * count);
                        
                        for (int i = 0; i < count; i++) {
                            result += str;
                        }
                        
                        a = Value(internString(result));
                    }
                    stk.pop_back();
                    DISPATCH();
                } else if (a.type == ValueType::NUMBER && b.type == ValueType::OBJ_STRING) {
                    // int * string
                    {
                        int count = static_cast<int>(a.as.number);
                        std::string str = b.asString()->chars;
                        
                        if (count < 0) count = 0;
                        
                        std::string result;
                        result.reserve(str.length() * count);
                        
                        for (int i = 0; i < count; i++) {
                            result += str;
                        }
                        
                        a = Value(internString(result));
                    }
                    stk.pop_back();
                    DISPATCH();
                } else {
                    runtimeError(this, "Unsupported operand types for multiplication.", frames.empty() ? -1 : frames.back().currentLine);
                }
            }
            CASE(OP_DIVIDE) {
                size_t sz = stk.size();
                Value& b = stk[sz - 1];
                Value& a = stk[sz - 2];
                if (__builtin_expect(a.type == ValueType::NUMBER && b.type == ValueType::NUMBER, 1)) {
                    double val_b = b.as.number;
                    if (__builtin_expect(val_b != 0, 1)) {
                        a.as.number /= val_b;
                        stk.pop_back();
                    } else {
                        runtimeError(this, "Division by zero.", frames.empty() ? -1 : frames.back().currentLine);
                    }
                } else {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
            CASE(OP_MODULO) {
                size_t sz = stk.size();
                Value& b = stk[sz - 1];
                Value& a = stk[sz - 2];
                if (__builtin_expect(a.type == ValueType::NUMBER && b.type == ValueType::NUMBER, 1)) {
                    double val_b = b.as.number;
                    if (__builtin_expect(val_b != 0, 1)) {
                        a.as.number = fmod(a.as.number, val_b);
                        stk.pop_back();
                    } else {
                        runtimeError(this, "Modulo by zero.", frames.empty() ? -1 : frames.back().currentLine);
                    }
                } else {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
            CASE(OP_NOT) {
                Value& value = peek(0);
                value = Value(!isTruthy(value));
                DISPATCH();
            }
            CASE(OP_NEGATE) {
                Value& value = peek(0);
                if (__builtin_expect(value.type == ValueType::NUMBER, 1)) {
                    value.as.number = -value.as.number;
                } else {
                    runtimeError(this, "Operand must be a number.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
            CASE(OP_BITWISE_AND) {
                size_t sz = stk.size();
                Value& b = stk[sz - 1];
                Value& a = stk[sz - 2];
                if (__builtin_expect(a.type == ValueType::NUMBER && b.type == ValueType::NUMBER, 1)) {
                    int64_t ia = static_cast<int64_t>(a.as.number);
                    int64_t ib = static_cast<int64_t>(b.as.number);
                    a.as.number = static_cast<double>(ia & ib);
                    stk.pop_back();
                } else {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
            CASE(OP_BITWISE_OR) {
                size_t sz = stk.size();
                Value& b = stk[sz - 1];
                Value& a = stk[sz - 2];
                if (__builtin_expect(a.type == ValueType::NUMBER && b.type == ValueType::NUMBER, 1)) {
                    int64_t ia = static_cast<int64_t>(a.as.number);
                    int64_t ib = static_cast<int64_t>(b.as.number);
                    a.as.number = static_cast<double>(ia | ib);
                    stk.pop_back();
                } else {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
            CASE(OP_BITWISE_XOR) {
                size_t sz = stk.size();
                Value& b = stk[sz - 1];
                Value& a = stk[sz - 2];
                if (__builtin_expect(a.type == ValueType::NUMBER && b.type == ValueType::NUMBER, 1)) {
                    int64_t ia = static_cast<int64_t>(a.as.number);
                    int64_t ib = static_cast<int64_t>(b.as.number);
                    a.as.number = static_cast<double>(ia ^ ib);
                    stk.pop_back();
                } else {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
            CASE(OP_BITWISE_NOT) {
                Value& value = stk.back();
                if (__builtin_expect(value.type == ValueType::NUMBER, 1)) {
                    value.as.number = static_cast<double>(~static_cast<int64_t>(value.as.number));
                } else {
                    runtimeError(this, "Operand must be a number.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
            CASE(OP_LEFT_SHIFT) {
                size_t sz = stk.size();
                Value& b = stk[sz - 1];
                Value& a = stk[sz - 2];
                if (__builtin_expect(a.type == ValueType::NUMBER && b.type == ValueType::NUMBER, 1)) {
                    a.as.number = static_cast<double>(static_cast<int64_t>(a.as.number) << static_cast<int64_t>(b.as.number));
                    stk.pop_back();
                } else {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
            CASE(OP_RIGHT_SHIFT) {
                size_t sz = stk.size();
                Value& b = stk[sz - 1];
                Value& a = stk[sz - 2];
                if (__builtin_expect(a.type == ValueType::NUMBER && b.type == ValueType::NUMBER, 1)) {
                    a.as.number = static_cast<double>(static_cast<int64_t>(a.as.number) >> static_cast<int64_t>(b.as.number));
                    stk.pop_back();
                } else {
                    runtimeError(this, "Operands must be numbers.", frames.empty() ? -1 : frames.back().currentLine);
                }
                DISPATCH();
            }
#if !COMPUTED_GOTO
            default:
                runtimeError(this, "Unknown opcode.", frames.empty() ? -1 : frames.back().currentLine);
#else
            CASE_DEFAULT:
                runtimeError(this, "Unknown opcode.", frames.empty() ? -1 : frames.back().currentLine);
#endif
            CASE(OP_INVOKE) {
                // Optimized method call: combines GET_PROPERTY + CALL
                // Encoding: OP_INVOKE <property_name_constant> <arg_count>
                // Capture callsite IP before reading operands for per-callsite caching
                const uint8_t* callsite_ip = frame->ip;
                ObjString* methodNameObj = READ_CONSTANT().as.obj_string;
                uint8_t argCount = READ_BYTE();
                
                Value& receiver = stk[stk.size() - argCount - 1];
                
                if (__builtin_expect(receiver.type == ValueType::INSTANCE, 1)) {
                    Instance* inst = receiver.as.instance;
                    
                    // Per-callsite method cache lookup
                    size_t cache_idx = (reinterpret_cast<uintptr_t>(callsite_ip) >> 1) & (METHOD_CACHE_SIZE - 1);
                    MethodCacheEntry& mc = method_cache[cache_idx];
                    if (__builtin_expect(mc.callsite_ip == callsite_ip && mc.klass == inst->klass, 1)) {
                        Function* method = mc.method;
                        if (__builtin_expect(method->arity_val == argCount && frames.size() < 256, 1)) {
                            CallFrame* newFrame = &frames.emplace_back();
                            newFrame->function = method;
                            newFrame->ip = method->chunk->code.data();
                            newFrame->slot_offset = stk.size() - argCount - 1;
                            newFrame->fileName = &currentFileName;
                            newFrame->currentLine = -1;
                            newFrame->isBoundMethod = true;
                            frame = newFrame;
                            DISPATCH();
                        }
                    }
                    
                    // Check methods (most common case for OP_INVOKE)
                    auto methIt = inst->klass->methods.find(methodNameObj);
                    if (__builtin_expect(methIt != inst->klass->methods.end(), 1)) {
                        Value methodValue = methIt->second;
                        if (__builtin_expect(methodValue.type == ValueType::CALLABLE, 1)) {
                            Function* method = static_cast<Function*>(methodValue.as.callable);
                            
                            // Update per-callsite method cache
                            mc.callsite_ip = callsite_ip;
                            mc.klass = inst->klass;
                            mc.method_name = methodNameObj;
                            mc.method = method;
                            
                            if (__builtin_expect(method->arity_val == argCount, 1)) {
                                if (__builtin_expect(frames.size() < 256, 1)) {
                                    CallFrame* newFrame = &frames.emplace_back();
                                    newFrame->function = method;
                                    newFrame->ip = method->chunk->code.data();
                                    newFrame->slot_offset = stk.size() - argCount - 1;
                                    newFrame->fileName = &currentFileName;
                                    newFrame->currentLine = -1;
                                    newFrame->isBoundMethod = true;
                                    frame = newFrame;
                                    DISPATCH();
                                }
                                runtimeError(this, "Stack overflow.", frames.empty() ? -1 : frames.back().currentLine);
                                return;
                            }
                            
                            std::string funcName = method->declaration ? 
                                method->declaration->name.lexeme : "<method>";
                            runtimeError(this, "Expected " + std::to_string(method->arity_val) + 
                                " arguments but got " + std::to_string(argCount) + 
                                " for method '" + funcName + "'.", 
                                frames.empty() ? -1 : frames.back().currentLine);
                            return;
                        }
                    }
                    
                    // Fallback: check for field (could be a closure stored in a field)
                    Value* fieldVal = inst->getField(methodNameObj);
                    if (fieldVal != nullptr) {
                        stk[stk.size() - argCount - 1] = *fieldVal;
                        if (!callValue(*fieldVal, argCount)) {
                            return;
                        }
                        frame = &frames.back();
                        DISPATCH();
                    }
                    
                    runtimeError(this, "Method '" + methodNameObj->chars + "' not found on instance.",
                        frames.empty() ? -1 : frames.back().currentLine);
                    return;
                } else if (receiver.type == ValueType::ARRAY) {
                    // Array methods
                    Array* arr = receiver.as.array;
                    const std::string& methodName = methodNameObj->chars;
                    
                    if (methodName == "push" || methodName == "pop" || 
                        methodName == "slice" || methodName == "map" ||
                        methodName == "filter" || methodName == "find" ||
                        methodName == "indexOf" || methodName == "join" ||
                        methodName == "reverse" || methodName == "sort") {
                        BoundArrayMethod* bam = allocate<BoundArrayMethod>(arr, methodName);
                        stk[stk.size() - argCount - 1] = Value(static_cast<Object*>(bam));
                        if (!callArrayMethod(bam, argCount)) {
                            return;
                        }
                        frame = &frames.back();
                        DISPATCH();
                    }
                    runtimeError(this, "Array does not have method '" + methodName + "'.",
                        frames.empty() ? -1 : frames.back().currentLine);
                    return;
                } else if (receiver.type == ValueType::OBJ_STRING) {
                    // String methods
                    ObjString* strObj = receiver.as.obj_string;
                    const std::string& methodName = methodNameObj->chars;
                    
                    if (StringMethodRegistry::getInstance().hasMethod(methodName)) {
                        BoundStringMethod* bsm = allocate<BoundStringMethod>(strObj->chars, methodName);
                        stk[stk.size() - argCount - 1] = Value(static_cast<Object*>(bsm));
                        if (!callStringMethod(bsm, argCount)) {
                            return;
                        }
                        frame = &frames.back();
                        DISPATCH();
                    }
                    runtimeError(this, "String does not have method '" + methodName + "'.",
                        frames.empty() ? -1 : frames.back().currentLine);
                    return;
                } else {
                    // Fallback: do GET_PROPERTY + CALL the slow way
                    // This handles modules, JSON objects, etc.
                    
                    // First simulate GET_PROPERTY
                    Value callee = receiver;
                    if (callee.type == ValueType::MODULE) {
                        Module* module = callee.as.module;
                        try {
                            Value property = module->get(methodNameObj->chars);
                            stk[stk.size() - argCount - 1] = property;
                            if (!callValue(property, argCount)) {
                                return;
                            }
                            frame = &frames.back();
                            DISPATCH();
                        } catch (const std::runtime_error& e) {
                            runtimeError(this, std::string(e.what()), frames.empty() ? -1 : frames.back().currentLine);
                            return;
                        }
                    }
                    
                    runtimeError(this, "Cannot invoke method on this value type.", 
                        frames.empty() ? -1 : frames.back().currentLine);
                    return;
                }
            }
            CASE(OP_INCREMENT_LOCAL) {
                uint8_t slot = READ_BYTE();
                stk[frame->slot_offset + slot].as.number += 1.0;
                DISPATCH();
            }
            CASE(OP_DECREMENT_LOCAL) {
                uint8_t slot = READ_BYTE();
                stk[frame->slot_offset + slot].as.number -= 1.0;
                DISPATCH();
            }
            CASE(OP_LOOP_IF_LESS_LOCAL) {
                // Fused: if (local[slot] < constant) don't jump, else jump
                // Encoding: slot(1) + constant_idx(1) + jump_offset(2)
                uint8_t slot = READ_BYTE();
                uint8_t constIdx = READ_BYTE();
                uint16_t offset = READ_SHORT();
                double localVal = stk[frame->slot_offset + slot].as.number;
                double limit = frame->function->chunk->constants[constIdx].as.number;
                if (!(localVal < limit)) {
                    frame->ip += offset; // exit loop
                }
                DISPATCH();
            }
            CASE(OP_INCREMENT_GLOBAL) {
                // Fused: globals[name] += 1.0 (no stack operations)
                uint8_t idx = READ_BYTE();
                ObjString* nameStr = frame->function->chunk->constants[idx].as.obj_string;
                uint8_t slot = idx & 127;
                GlobalCacheEntry& entry = global_cache[slot];
                if (__builtin_expect(entry.key == nameStr, 1)) {
                    entry.value->as.number += 1.0;
                } else {
                    auto it = globals.find(nameStr->chars);
                    if (__builtin_expect(it != globals.end(), 1)) {
                        entry.key = nameStr;
                        entry.value = &(it->second);
                        it->second.as.number += 1.0;
                    }
                }
                DISPATCH();
            }
            CASE(OP_GET_GLOBAL_FAST) {
                // Cached global variable read: uses ObjString* identity check
                // instead of hash map lookup on every iteration.
                uint8_t idx = READ_BYTE();
                ObjString* nameStr = frame->function->chunk->constants[idx].as.obj_string;
                uint8_t slot = idx & 127;
                GlobalCacheEntry& entry = global_cache[slot];
                if (__builtin_expect(entry.key == nameStr, 1)) {
                    FAST_PUSH(*entry.value);
                } else {
                    auto it = globals.find(nameStr->chars);
                    if (__builtin_expect(it == globals.end(), 0)) {
                        runtimeError(this, "Undefined variable '" + nameStr->chars + "'.",
                                    frames.empty() ? -1 : frames.back().currentLine);
                    }
                    entry.key = nameStr;
                    entry.value = &(it->second);
                    FAST_PUSH(it->second);
                }
                DISPATCH();
            }
            CASE(OP_SET_GLOBAL_FAST) {
                // Cached global variable write
                uint8_t idx = READ_BYTE();
                ObjString* nameStr = frame->function->chunk->constants[idx].as.obj_string;
                uint8_t slot = idx & 127;
                GlobalCacheEntry& entry = global_cache[slot];
                if (__builtin_expect(entry.key == nameStr, 1)) {
                    *entry.value = peek(0);
                } else {
                    auto it = globals.find(nameStr->chars);
                    if (__builtin_expect(it == globals.end(), 0)) {
                        runtimeError(this, "Undefined variable '" + nameStr->chars + "'.",
                                    frames.empty() ? -1 : frames.back().currentLine);
                    }
                    entry.key = nameStr;
                    entry.value = &(it->second);
                    it->second = peek(0);
                }
                DISPATCH();
            }
            // ============================================================
            // Extended opcodes emitted by BytecodeOptimizer
            // ============================================================
            CASE(OP_LOAD_LOCAL_0) {
                FAST_PUSH(stk[frame->slot_offset]);
                DISPATCH();
            }
            CASE(OP_LOAD_LOCAL_1) {
                FAST_PUSH(stk[frame->slot_offset + 1]);
                DISPATCH();
            }
            CASE(OP_LOAD_LOCAL_2) {
                FAST_PUSH(stk[frame->slot_offset + 2]);
                DISPATCH();
            }
            CASE(OP_LOAD_LOCAL_3) {
                FAST_PUSH(stk[frame->slot_offset + 3]);
                DISPATCH();
            }
            CASE(OP_CONST_ZERO) {
                FAST_PUSH(Value(0.0));
                DISPATCH();
            }
            CASE(OP_CONST_ONE) {
                FAST_PUSH(Value(1.0));
                DISPATCH();
            }
            CASE(OP_CONST_INT8) {
                uint8_t raw = READ_BYTE();
                FAST_PUSH(small_int_table[raw]);
                DISPATCH();
            }
            CASE(OP_ADD_INT) {
                size_t sz = stk.size();
                stk[sz - 2].as.number += stk[sz - 1].as.number;
                stk.pop_back();
                DISPATCH();
            }
            CASE(OP_SUB_INT) {
                size_t sz = stk.size();
                stk[sz - 2].as.number -= stk[sz - 1].as.number;
                stk.pop_back();
                DISPATCH();
            }
            CASE(OP_MUL_INT) {
                size_t sz = stk.size();
                stk[sz - 2].as.number *= stk[sz - 1].as.number;
                stk.pop_back();
                DISPATCH();
            }
            CASE(OP_DIV_INT) {
                size_t sz = stk.size();
                stk[sz - 2].as.number /= stk[sz - 1].as.number;
                stk.pop_back();
                DISPATCH();
            }
            CASE(OP_MOD_INT) {
                size_t sz = stk.size();
                stk[sz - 2].as.number = std::fmod(stk[sz - 2].as.number, stk[sz - 1].as.number);
                stk.pop_back();
                DISPATCH();
            }
            CASE(OP_NEGATE_INT) {
                stk.back().as.number = -stk.back().as.number;
                DISPATCH();
            }
            CASE(OP_LESS_INT) {
                size_t sz = stk.size();
                bool result = stk[sz - 2].as.number < stk[sz - 1].as.number;
                stk.pop_back();
                stk.back() = Value(result);
                DISPATCH();
            }
            CASE(OP_GREATER_INT) {
                size_t sz = stk.size();
                bool result = stk[sz - 2].as.number > stk[sz - 1].as.number;
                stk.pop_back();
                stk.back() = Value(result);
                DISPATCH();
            }
            CASE(OP_EQUAL_INT) {
                size_t sz = stk.size();
                bool result = stk[sz - 2].as.number == stk[sz - 1].as.number;
                stk.pop_back();
                stk.back() = Value(result);
                DISPATCH();
            }
            CASE(OP_INC_LOCAL_INT) {
                uint8_t slot = READ_BYTE();
                stk[frame->slot_offset + slot].as.number += 1.0;
                DISPATCH();
            }
            CASE(OP_DEC_LOCAL_INT) {
                uint8_t slot = READ_BYTE();
                stk[frame->slot_offset + slot].as.number -= 1.0;
                DISPATCH();
            }
            CASE(OP_LESS_JUMP) {
                // Fused: LESS + JUMP_IF_FALSE
                uint16_t offset = READ_SHORT();
                size_t sz = stk.size();
                bool cond = stk[sz - 2].as.number < stk[sz - 1].as.number;
                stk.pop_back();
                stk.pop_back();
                if (!cond) {
                    frame->ip += offset;
                }
                DISPATCH();
            }
            CASE(OP_GREATER_JUMP) {
                // Fused: GREATER + JUMP_IF_FALSE
                uint16_t offset = READ_SHORT();
                size_t sz = stk.size();
                bool cond = stk[sz - 2].as.number > stk[sz - 1].as.number;
                stk.pop_back();
                stk.pop_back();
                if (!cond) {
                    frame->ip += offset;
                }
                DISPATCH();
            }
            CASE(OP_EQUAL_JUMP) {
                // Fused: EQUAL + JUMP_IF_FALSE
                uint16_t offset = READ_SHORT();
                size_t sz = stk.size();
                bool cond = stk[sz - 2].as.number == stk[sz - 1].as.number;
                stk.pop_back();
                stk.pop_back();
                if (!cond) {
                    frame->ip += offset;
                }
                DISPATCH();
            }
            CASE(OP_ADD_LOCAL_CONST) {
                // Fused: GET_LOCAL + CONSTANT + ADD
                uint8_t slot = READ_BYTE();
                uint8_t constIdx = READ_BYTE();
                double result = stk[frame->slot_offset + slot].as.number +
                                frame->function->chunk->constants[constIdx].as.number;
                FAST_PUSH(Value(result));
                DISPATCH();
            }
            CASE(OP_SAY) {
                Value& v = stk.back();
                if (__builtin_expect(v.type == ValueType::OBJ_STRING, 1)) {
                    std::cout << v.as.obj_string->chars << '\n';
                } else {
                    std::cout << v.toString() << '\n';
                }
                stk.pop_back();
                DISPATCH();
            }
            CASE(OP_JUMP) {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                DISPATCH();
            }
            CASE(OP_JUMP_IF_FALSE) {
                uint16_t offset = READ_SHORT();
                Value& condition = stk.back();
                // Fast path: boolean is the most common condition type in loops
                bool jump;
                if (__builtin_expect(condition.type == ValueType::BOOLEAN, 1)) {
                    jump = !condition.as.boolean;
                } else {
                    jump = (condition.type == ValueType::NIL);
                }
                stk.pop_back();
                if (jump) {
                    frame->ip += offset;
                }
                DISPATCH();
            }
            CASE(OP_LOOP) {
                uint16_t offset = READ_SHORT();

                if (__builtin_expect(jitEnabled, 1)) {
                    uint64_t loop_pc = static_cast<uint64_t>((frame->ip - offset) - frame->function->chunk->code.data());
                    uint64_t method_id = reinterpret_cast<uint64_t>(frame->function);
                    
                    // Fast path: check inline cache for compiled trace
                    size_t cache_slot = loop_pc & (JIT_LOOP_CACHE_SIZE - 1);
                    JITLoopCacheEntry& cached = jitLoopCache[cache_slot];
                    if (__builtin_expect(cached.loop_pc == loop_pc && cached.method_id == method_id, 1)) {
                        auto* tier2 = jitManager.getTier2Compiler();
                        if (tier2) {
                            jit::MultiTierJITManager::ExecutionFrame jitFrame;
                            jitFrame.method_id = method_id;
                            jitFrame.chunk = frame->function->chunk;
                            jitFrame.bytecode_pc = loop_pc;
                            jitFrame.stack_pointer = nullptr;
                            jitFrame.local_variables = &stk[frame->slot_offset];
                            jitFrame.current_tier = jit::CompilationTier::TIER2;
                            if (tier2->executeTrace(cached.trace_id, &jitFrame)) {
                                DISPATCH();
                            }
                        }
                    }

                    // Slow path: periodic check for compilation
                    if ((++jitLoopCounter & 15) == 0) {
                        auto* tier2 = jitManager.getTier2Compiler();
                        if (tier2) {
                            uint64_t trace_id = tier2->findTrace(method_id, loop_pc);
                            if (trace_id != 0) {
                                // Cache for future O(1) lookups
                                cached.loop_pc = loop_pc;
                                cached.method_id = method_id;
                                cached.trace_id = trace_id;
                                
                                jit::MultiTierJITManager::ExecutionFrame jitFrame;
                                jitFrame.method_id = method_id;
                                jitFrame.chunk = frame->function->chunk;
                                jitFrame.bytecode_pc = loop_pc;
                                jitFrame.stack_pointer = nullptr;
                                jitFrame.local_variables = &stk[frame->slot_offset];
                                jitFrame.current_tier = jit::CompilationTier::TIER2;
                                if (tier2->executeTrace(trace_id, &jitFrame)) {
                                    DISPATCH();
                                }
                            } else if (!tier2->isTraceFailed(method_id, loop_pc)) {
                                tier2->setGlobalsMap(&globals);
                                jit::HotSpotProfiler dummyProfiler;
                                auto trace = tier2->recordTrace(method_id, *frame->function->chunk, loop_pc, dummyProfiler);
                                if (trace) {
                                    auto optimized = tier2->optimizeTrace(*trace);
                                    if (optimized) {
                                        uint64_t compiled = tier2->compileTrace(*optimized);
                                        if (compiled != 0) {
                                            // Cache it
                                            cached.loop_pc = loop_pc;
                                            cached.method_id = method_id;
                                            cached.trace_id = compiled;
                                            
                                            jit::MultiTierJITManager::ExecutionFrame jitFrame2;
                                            jitFrame2.method_id = method_id;
                                            jitFrame2.chunk = frame->function->chunk;
                                            jitFrame2.bytecode_pc = loop_pc;
                                            jitFrame2.stack_pointer = nullptr;
                                            jitFrame2.local_variables = &stk[frame->slot_offset];
                                            jitFrame2.current_tier = jit::CompilationTier::TIER2;
                                            if (tier2->executeTrace(compiled, &jitFrame2)) {
                                                DISPATCH();
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                frame->ip -= offset;
                DISPATCH();
            }
                        CASE(OP_CALL) {
                                uint8_t argCount = READ_BYTE();
                                syncStack();
                                Value callee = peek(argCount);
                                
                                // Fast path for class instantiation (most common in OOP loops)
                                if (callee.type == ValueType::CLASS) {
                                    Class* klass = callee.as.klass;
                                    Instance* instance = allocateInstance(klass);
                                    if (klass->initializer != nullptr) {
                                        Function* initializer = klass->initializer;
                                        stk[stk.size() - argCount - 1] = Value(instance);
                                        CallFrame* newFrame = &frames.emplace_back();
                                        newFrame->function = initializer;
                                        newFrame->ip = initializer->chunk->code.data();
                                        newFrame->slot_offset = stk.size() - argCount - 1;
                                        newFrame->fileName = &currentFileName;
                                        newFrame->currentLine = -1;
                                        newFrame->isBoundMethod = true;
                                        newFrame->isInitializer = true;
                                        frame = newFrame;
                                        DISPATCH();
                                    }
                                    stk[stk.size() - argCount - 1] = Value(instance);
                                    stk.resize(stk.size() - argCount);
                                    reloadStack();
                                    frame = &frames.back();
                                    DISPATCH();
                                }
                                
                                if (!callValue(callee, argCount)) {
                                        return;
                                }
                                reloadStack();
                                frame = &frames.back();
                                DISPATCH();
            }
                        CASE(OP_ARRAY) {
                {
                                uint8_t count = READ_BYTE();
                                std::vector<Value> elements;
                                elements.reserve(count);
                                
                                // Read elements without popping to keep them reachable for GC
                                for (int i = 0; i < count; i++) {
                                    elements.push_back(peek(count - 1 - i));
                                }
                                
                                syncStack();
                                Array* array = allocate<Array>(std::move(elements));
                                
                                // Pop elements
                                stack.resize(stack.size() - count);
                                
                                push(Value(array));
                }
                                DISPATCH();
            }
                        CASE(OP_OBJECT) {
                uint8_t count = READ_BYTE();
                syncStack();
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
                    
                    obj->properties[key.asString()] = val;
                }
                
                push(Value(obj));
                DISPATCH();
            }
                        CASE(OP_INDEX_GET) {
                                Value index = pop();
                                Value object = pop();
                
                                if (object.type == ValueType::ARRAY) {
                                        if (index.type != ValueType::NUMBER) {
                        runtimeError(this, "Array index must be a number.", frames.empty() ? -1 : frames.back().currentLine);
                                            return;
                    }
                    
                                        int idx = static_cast<int>(index.as.number);
                    Array* array = object.as.array;
                    
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
                    
                    {
                        int idx = static_cast<int>(index.as.number);
                        std::string str = object.asString()->chars;
                        int strLen = static_cast<int>(str.length());
                        
                        // Handle negative indices (Python-style)
                        if (idx < 0) {
                            idx = strLen + idx;
                        }
                        
                        if (idx < 0 || idx >= strLen) {
                            std::string range = strLen == 0 ? "[]" : "[" + std::to_string(-strLen) + ", " + std::to_string(strLen-1) + "]";
                            std::string errorMsg = "String index out of bounds: index " + std::to_string(static_cast<int>(index.as.number)) + 
                                                  " is not within " + range;
                            runtimeError(this, errorMsg, 
                                        frames.empty() ? -1 : frames.back().currentLine);
                            return;
                        }
                        
                        // Return the character at the index as a string
                        push(Value(std::string(1, str[idx])));
                    }
                } else if (object.type == ValueType::BUFFER) {
                    if (index.type != ValueType::NUMBER) {
                        runtimeError(this, "Buffer index must be a number.", frames.empty() ? -1 : frames.back().currentLine);
                        return;
                    }
                    
                    int idx = static_cast<int>(index.as.number);
                    Buffer* buffer = object.asBuffer();
                    
                    if (idx < 0 || idx >= static_cast<int>(buffer->size())) {
                        runtimeError(this, "Buffer index out of bounds.", frames.empty() ? -1 : frames.back().currentLine);
                        return;
                    }
                    
                    push(Value(static_cast<double>(buffer->get(idx))));
                } else if (object.type == ValueType::OBJECT) {
                    Object* objPtr = object.as.object;
                    JsonObject* jsonObj = dynamic_cast<JsonObject*>(objPtr);
                    
                    if (jsonObj) {
                        if (index.type != ValueType::OBJ_STRING) {
                            runtimeError(this, "Object key must be a string.", frames.empty() ? -1 : frames.back().currentLine);
                            return;
                        }
                        ObjString* key = index.asString();
                        auto it = jsonObj->properties.find(key);
                        if (it != jsonObj->properties.end()) {
                            push(it->second);
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
                                DISPATCH();
            }
                        CASE(OP_INDEX_SET) {
                Value value = pop();
                                Value index = pop();
                                Value object = pop();
                
                                if (object.type == ValueType::ARRAY) {
                                        if (index.type != ValueType::NUMBER) {
                        runtimeError(this, "Array index must be a number.", frames.empty() ? -1 : frames.back().currentLine);
                                            return;
                    }
                    
                                        int idx = static_cast<int>(index.as.number);
                    Array* array = object.as.array;
                    
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
                    
                    int idx = static_cast<int>(index.as.number);
                    int val = static_cast<int>(value.as.number);
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
                    Object* objPtr = object.as.object;
                    JsonObject* jsonObj = dynamic_cast<JsonObject*>(objPtr);
                    
                    if (jsonObj) {
                        if (index.type != ValueType::OBJ_STRING) {
                            runtimeError(this, "Object key must be a string.", frames.empty() ? -1 : frames.back().currentLine);
                            return;
                        }
                        jsonObj->properties[index.asString()] = value;
                        push(value);
                    } else {
                         runtimeError(this, "This object type does not support index assignment.", frames.empty() ? -1 : frames.back().currentLine);
                         return;
                    }
                } else {
                    runtimeError(this, "Only arrays, buffers, and objects support index assignment.", frames.empty() ? -1 : frames.back().currentLine);
                                        return;
                }
                                DISPATCH();
            }
            CASE(OP_TRY) {
                // Set up an exception handler frame
                // Read handler information from bytecode
                uint16_t tryEnd = READ_SHORT(); // End of try block
                uint16_t catchStart = READ_SHORT(); // Start of catch block (-1 if none)
                uint16_t finallyStart = READ_SHORT(); // Start of finally block (-1 if none)
                
                ptrdiff_t currentIP = (frame->ip - 1) - frame->function->chunk->code.data(); // Position before reading shorts
                size_t currentFrameBase = stack.size();
                
                exceptionFrames.emplace_back(
                    static_cast<int>(currentIP), 
                    static_cast<int>(tryEnd), 
                    catchStart == 0xFFFF ? -1 : static_cast<int>(catchStart),  // 0xFFFF represents -1 (no handler)
                    finallyStart == 0xFFFF ? -1 : static_cast<int>(finallyStart), // 0xFFFF represents -1 (no handler)
                    currentFrameBase,
                    frame->getFileName(),
                    frame->currentLine
                );
                DISPATCH();
            }
            CASE(OP_END_TRY) {
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
                
                DISPATCH();
            }
            CASE(OP_THROW) {
                // A value has been pushed to the stack - this is the exception
                Value exception = pop();
                
                // Find the closest exception handler in the current call frame
                ExceptionFrame* handler = nullptr;
                for (size_t idx = exceptionFrames.size(); idx > 0; idx--) {
                    size_t i = idx - 1;
                    ExceptionFrame& frame_handler = exceptionFrames[i];
                    ptrdiff_t current_pos = frame->ip - frame->function->chunk->code.data() - 1; // -1 to account for the read byte
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
                    // DISPATCH(); // Exit OP_THROW processing - handled by outer loop continue
                    continue;
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
                
                // DISPATCH();
                continue;
            }
#if !COMPUTED_GOTO
        }
#endif
#if !COMPUTED_GOTO
        } // End of for(;;)
#endif
    } catch (const VMException& e) {
        // Handle caught runtime exception
        push(e.value); // Push exception value
        
        // We need to find the handler again because we unwound the C++ stack
        // but the VM state (frames, exceptionFrames) is preserved.
        
        CallFrame* currentFrame = &frames.back();
        ptrdiff_t current_pos = currentFrame->ip - currentFrame->function->chunk->code.data() - 1;
        
        ExceptionFrame* handler = nullptr;
        for (size_t idx = exceptionFrames.size(); idx > 0; idx--) {
            size_t i = idx - 1;
            ExceptionFrame& frame_handler = exceptionFrames[i];
            if (current_pos >= frame_handler.tryStart && current_pos <= frame_handler.tryEnd) {
                handler = &frame_handler;
                // Found handler, break to execute it
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
            currentFrame->ip = currentFrame->function->chunk->code.data() + handler->catchStart;
            
            // Refresh frame pointer
            currentFrame = &frames.back();
            (void)currentFrame; // Mark as used
            
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
    size_t initial_frames_size = frames.size();
    size_t initial_stack_size = stack.size();
    try {
        if (callee.type != ValueType::CALLABLE) {
            throw std::runtime_error("Can only call functions.");
        }
        
        Callable* function = callee.as.callable;
        
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
             frame->fileName = &currentFileName;
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
             
             call(func, static_cast<int>(arguments.size()));
             
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
        while (frames.size() > initial_frames_size) {
            frames.pop_back();
        }
        while (stack.size() > initial_stack_size) {
            pop();
        }
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

/**
 * @brief Load and register a module by name into the VM.
 *
 * Attempts to locate and load a module named `name` from built-in modules, Neutron source
 * (.nt) files in standard search paths, or native shared libraries. On success the module
 * is registered in the VM (globals and module registry) and cached to avoid future reloads.
 *
 * @param name Name of the module to load (e.g., "json", "fmt", or a user module name).
 *
 * Observable behavior:
 * - Initializes and registers built-in modules when `name` matches a builtin identifier.
 * - When a `.nt` source file is found, parses, compiles, and executes it in an isolated
 *   module environment; the module source is registered with the ErrorHandler for accurate
 *   error mapping.
 * - When a native library is found, loads it and calls its `neutron_module_init` (or
 *   `_neutron_module_init`) entry point to populate the module environment.
 * - On success the module is made available via the VM's module registry and cached.
 * - If the module cannot be found or is invalid (missing init symbol), a runtime error is
 *   reported with context suitable for stack traces and error reporting.
 */
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
    } else if (name == "process") {
        neutron_init_process_module(this);
        loadedModuleCache[name] = true;
        return;
    } else if (name == "crypto") {
        neutron_init_crypto_module(this);
        loadedModuleCache[name] = true;
        return;
    } else if (name == "path") {
        neutron_init_path_module(this);
        loadedModuleCache[name] = true;
        return;
    } else if (name == "random") {
        neutron_init_random_module(this);
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
        
        // Register source code with error handler
        ErrorHandler::addFileSource(found_nt_path, source);
        
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
            std::string previousFileName = currentFileName;
            currentFileName = found_nt_path;
            interpret(module_function);
            currentFileName = previousFileName;
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
        
        // Try to find the init function with various possible names
        void (*init_func)(NeutronVM*) = nullptr;
        
        // Try standard C name first
        init_func = (void (*)(NeutronVM*))dlsym(handle, "neutron_module_init");
        
        // If not found, try with underscore prefix (some Windows compilers add this)
        if (!init_func) {
            init_func = (void (*)(NeutronVM*))dlsym(handle, "_neutron_module_init");
        }
        
        if (!init_func) {
            // Get the last error message for debugging
            const char* error_msg = dlerror();
            dlclose(handle);
            std::string full_error = "Module '" + name + "' is not a valid Neutron module: missing neutron_module_init function.";
            if (error_msg) {
                full_error += " Error: " + std::string(error_msg);
            }
            runtimeError(this, full_error, frames.empty() ? -1 : frames.back().currentLine);
        }
        
        init_func(reinterpret_cast<NeutronVM*>(this));
        
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

/**
 * @brief Loads, compiles, and executes a Neutron source file into the VM's global scope.
 *
 * Searches embedded files first and then module search paths for the given filepath; when found,
 * registers the file source with the error handler, parses and compiles the source, and executes
 * the resulting code in the current global environment. Temporarily sets the VM's current file
 * context to the file being executed to improve error tracebacks and restores it after execution.
 *
 * If the file cannot be located, reports a runtime error and does not modify globals.
 *
 * @param filepath Path or module-relative name of the source file to load. Embedded files and
 *                 entries under the VM's module_search_paths are considered during lookup.
 */
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
    
    // Register source code with error handler
    ErrorHandler::addFileSource(filepath, source);
    
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
        std::string previousFileName = currentFileName;
        currentFileName = filepath;
        interpret(file_function);
        currentFileName = previousFileName;
        delete file_function;
    }
}

/**
 * @brief Load a Neutron source file, execute it in an isolated module environment, and produce a Module.
 *
 * Searches embedded files first and then module search paths for the given filepath, registers the file source
 * with the error handler, parses and executes the source in a dedicated environment, and returns a Module
 * wrapping that environment populated with the module's exported bindings.
 *
 * @param filepath Path or module-relative path to the Neutron source file to load.
 * @return Module* Pointer to the newly created Module containing the module environment and exports, or `nullptr`
 * if the file could not be found or loading failed (a runtime error is reported in that case).
 */
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
    
    // Register source code with error handler
    ErrorHandler::addFileSource(filepath, source);
    
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
    
    std::string previousFileName = currentFileName;
    currentFileName = filepath;
    interpret_module(statements, module_env);
    currentFileName = previousFileName;
    
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

ObjString* VM::internString(const std::string& str) {
    auto it = internedStrings.find(str);
    if (it != internedStrings.end()) {
        return it->second;
    }
    
    ObjString* newString = allocate<ObjString>(str);
    newString->isInterned = true;
    internedStrings[str] = newString;
    return newString;
}

// Fast string allocation without interning - for data strings (skip hash computation)
ObjString* VM::makeString(const std::string& str) {
    return allocate<ObjString>(str, false);  // Don't compute hash
}

ObjString* VM::makeString(std::string&& str) {
    return allocate<ObjString>(std::move(str), false);  // Don't compute hash
}

void VM::collectGarbage() {
    // Mark all reachable objects
    markRoots();
    
    // Trace references
    traceReferences();
    
    // Sweep all unreachable objects
    sweep();
    
    // Set next collection threshold (double the current live size, but at least 8192)
    nextGC = std::max(heap.size() * 2, (size_t)32768);
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

    // Mark interned strings
    for (const auto& pair : internedStrings) {
        markObject(pair.second);
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
    // Use obj_type tag + static_cast instead of dynamic_cast chain.
    // dynamic_cast involves expensive RTTI lookups; this is a simple switch.
    switch (obj->obj_type) {
        case ObjType::OBJ_ARRAY: {
            auto* arr = static_cast<Array*>(obj);
            for (const auto& element : arr->elements) markValue(element);
            break;
        }
        case ObjType::OBJ_INSTANCE: {
            auto* inst = static_cast<Instance*>(obj);
            for (uint8_t i = 0; i < inst->inlineCount; ++i) {
                markObject(inst->inlineFields[i].key);
                markValue(inst->inlineFields[i].value);
            }
            if (inst->overflowFields) {
                for (const auto& entry : *inst->overflowFields) {
                    markObject(entry.first);
                    markValue(entry.second);
                }
            }
            markObject(inst->klass);
            break;
        }
        case ObjType::OBJ_FUNCTION: {
            auto* func = static_cast<Function*>(obj);
            if (func->closure) {
                auto env = func->closure;
                while (env) {
                    for (const auto& pair : env->values) markValue(pair.second);
                    env = env->enclosing;
                }
            }
            if (func->chunk) {
                for (const auto& constant : func->chunk->constants) markValue(constant);
            }
            break;
        }
        case ObjType::OBJ_CLASS: {
            auto* klass = static_cast<Class*>(obj);
            if (klass->class_env) {
                for (const auto& pair : klass->class_env->values) markValue(pair.second);
            }
            for (const auto& pair : klass->methods) {
                markObject(pair.first);
                markValue(pair.second);
            }
            break;
        }
        case ObjType::OBJ_BOUND_METHOD: {
            auto* bm = static_cast<BoundMethod*>(obj);
            markValue(bm->receiver);
            markObject(bm->method);
            break;
        }
        case ObjType::OBJ_BOUND_ARRAY_METHOD: {
            auto* bam = static_cast<BoundArrayMethod*>(obj);
            markObject(bam->array);
            break;
        }
        case ObjType::OBJ_JSON_OBJECT: {
            auto* json_obj = static_cast<JsonObject*>(obj);
            for (const auto& prop : json_obj->properties) {
                markObject(prop.first);
                markValue(prop.second);
            }
            break;
        }
        case ObjType::OBJ_JSON_ARRAY: {
            auto* json_arr = static_cast<JsonArray*>(obj);
            for (const auto& element : json_arr->elements) markValue(element);
            break;
        }
        case ObjType::OBJ_MODULE: {
            auto* module = static_cast<Module*>(obj);
            if (module->env) {
                for (const auto& pair : module->env->values) markValue(pair.second);
            }
            break;
        }
        case ObjType::OBJ_BUFFER:
        case ObjType::OBJ_STRING:
        case ObjType::OBJ_NATIVE_FN:
        case ObjType::OBJ_BOUND_STRING_METHOD:
        case ObjType::OBJ_GENERIC:
            // No references to trace
            break;
    }
}

void VM::markValue(const Value& value) {
    Object* obj = nullptr;
    switch (value.type) {
        case ValueType::OBJ_STRING: obj = value.as.obj_string; break;
        case ValueType::ARRAY: obj = value.as.array; break;
        case ValueType::OBJECT: obj = value.as.object; break;
        case ValueType::CALLABLE: obj = value.as.callable; break;
        case ValueType::MODULE: obj = value.as.module; break;
        case ValueType::CLASS: obj = value.as.klass; break;
        case ValueType::INSTANCE: obj = value.as.instance; break;
        default: return;
    }
    markObject(obj);
}

void VM::sweep() {
    // Use swap-and-pop (partition) to avoid O(n²) vector erasure
    size_t write = 0;
    for (size_t read = 0; read < heap.size(); ++read) {
        Object* obj = heap[read];
        if (!obj->is_marked) {
            // Pool instances for reuse instead of deleting
            if (obj->obj_type == ObjType::OBJ_INSTANCE && instancePool.size() < INSTANCE_POOL_MAX) {
                instancePool.push_back(static_cast<Instance*>(obj));
            } else {
                delete obj;
            }
        } else {
            obj->is_marked = false;
            heap[write++] = obj;
        }
    }
    heap.resize(write);
}

bool VM::handleException(const Value& exception) {
    (void)exception; // May be unused in some paths
    // Find the most recent handler that covers the current IP position
    for (size_t idx = exceptionFrames.size(); idx > 0; idx--) {
        size_t i = idx - 1;
        ExceptionFrame& handler = exceptionFrames[i];
        CallFrame* frame = &frames.back();
        ptrdiff_t current_pos = frame->ip - frame->function->chunk->code.data() - 1; // -1 to account for the read byte
        
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