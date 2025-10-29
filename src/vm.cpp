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
#include "types/json_object.h"
#include "types/instance.h"
#include "types/bound_method.h"
#include "types/bound_array_method.h"
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
#include "modules/module_registry.h"
#include "utils/component_interface.h"

namespace neutron {
    
// External symbols needed by bytecode_runner
extern "C" {
    [[maybe_unused]] const unsigned char bytecode_data[] = {0};  // Empty bytecode array
    [[maybe_unused]] const unsigned int bytecode_size = 0;       // Size is 0
}

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

// Simple overload for legacy error calls (backward compatibility)
[[noreturn]] void runtimeError(const std::string& message) {
    ErrorHandler::reportRuntimeError(message, "", -1, {});
    exit(1);
}

VM::VM() : ip(nullptr), nextGC(1024), currentFileName("<stdin>"), hasException(false), pendingException(Value()) {  // Start GC at 1024 bytes
    // Initialize error handler
    ErrorHandler::setColorEnabled(true);
    ErrorHandler::setStackTraceEnabled(true);
    
    globals["say"] = Value(new NativeFn(std::function<Value(std::vector<Value>)>(native_say), 1));
    
    // Register array functions
    // Built-in modules (sys, math, json, http, time, convert) are now loaded on-demand
    // when explicitly imported with "use modulename;"
    
    // External modules are registered but not initialized until imported
    run_external_module_initializers(this);
    
    // Set up module search paths
    module_search_paths.push_back(".");
    module_search_paths.push_back("lib/");
    module_search_paths.push_back("box/");
}

Function::Function(const FunctionStmt* declaration, std::shared_ptr<Environment> closure) 
    : name(), arity_val(0), chunk(new Chunk()), declaration(declaration), closure(closure) {}

Value Function::call(VM& /*vm*/, std::vector<Value> /*arguments*/) {
    // This is now handled by the VM's call stack
    return Value();
}

std::string Function::toString() {
    if (declaration) {
        return "<fn " + declaration->name.lexeme + ">";
    } else if (!name.empty()) {
        return "<fn " + name + ">";
    }
    return "<script>";
}

int Function::arity() { return arity_val; }

void VM::interpret(Function* function) {
    push(Value(function));
    call(function, 0);
    run();
}

void VM::push(const Value& value) {
    if (stack.size() >= 256) {
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
        ErrorHandler::argumentError(function->arity_val, argCount, funcName, currentFileName, 
                                   frames.empty() ? -1 : frames.back().currentLine);
        exit(1);
    }

    if (frames.size() == 256) {
        ErrorHandler::stackOverflowError(currentFileName, frames.empty() ? -1 : frames.back().currentLine);
        exit(1);
    }

    CallFrame* frame = &frames.emplace_back();
    frame->function = function;
    if (function->chunk->code.empty()) {
        frame->ip = nullptr;
    } else {
        frame->ip = function->chunk->code.data();
    }
    frame->slot_offset = stack.size() - argCount;  // Store offset instead of pointer
    frame->fileName = currentFileName;
    frame->currentLine = -1;

    return true;
}

bool VM::callValue(Value callee, int argCount) {
    // Handle BoundArrayMethod
    if (callee.type == ValueType::OBJECT) {
        Object* obj = std::get<Object*>(callee.as);
        BoundArrayMethod* arrayMethod = dynamic_cast<BoundArrayMethod*>(obj);
        if (arrayMethod != nullptr) {
            return callArrayMethod(arrayMethod, argCount);
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
                ErrorHandler::argumentError(boundMethod->method->arity_val, argCount, funcName, 
                                          currentFileName, frames.empty() ? -1 : frames.back().currentLine);
                exit(1);
            }
            // Manually set up the call frame to include receiver at slot 0
            if (frames.size() == 256) {
                ErrorHandler::stackOverflowError(currentFileName, frames.empty() ? -1 : frames.back().currentLine);
                exit(1);
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
            return true;
        } else if (Function* function = dynamic_cast<Function*>(callable)) {
            return call(function, argCount);
        } else if (NativeFn* native = dynamic_cast<NativeFn*>(callable)) {
            if (native->arity() != -1 && native->arity() != argCount) {
                ErrorHandler::argumentError(native->arity(), argCount, "<native function>", 
                                          currentFileName, frames.empty() ? -1 : frames.back().currentLine);
                exit(1);
            }
            std::vector<Value> args;
            for (int i = 0; i < argCount; i++) {
                args.push_back(stack[stack.size() - argCount + i]);
            }
            try {
                Value result = native->call(*this, args);
                stack.resize(stack.size() - argCount - 1);
                push(result);
            } catch (const std::runtime_error& e) {
                runtimeError(this, e.what(), frames.empty() ? -1 : frames.back().currentLine);
            }
            return true;
        } else if (callable->isCNativeFn()) {
            if (callable->arity() != -1 && callable->arity() != argCount) {
                ErrorHandler::argumentError(callable->arity(), argCount, "<native function>",
                                          currentFileName, frames.empty() ? -1 : frames.back().currentLine);
                exit(1);
            }
            std::vector<Value> args;
            for (int i = 0; i < argCount; i++) {
                args.push_back(stack[stack.size() - argCount + i]);
            }
            try {
                Value result = callable->call(*this, args);
                stack.resize(stack.size() - argCount - 1);
                push(result);
            } catch (const std::runtime_error& e) {
                runtimeError(this, e.what(), frames.empty() ? -1 : frames.back().currentLine);
            }
            return true;
        }
    }

    // Handle CLASS type
    if (callee.type == ValueType::CLASS) {
        Class* klass = std::get<Class*>(callee.as);
        if (klass->arity() != -1 && klass->arity() != argCount) {
            ErrorHandler::argumentError(klass->arity(), argCount, klass->name,
                                      currentFileName, frames.empty() ? -1 : frames.back().currentLine);
            exit(1);
        }
        std::vector<Value> args;
        for (int i = 0; i < argCount; i++) {
            args.push_back(stack[stack.size() - argCount + i]);
        }
        try {
            Value result = klass->call(*this, args);
            stack.resize(stack.size() - argCount - 1);
            push(result);
        } catch (const std::runtime_error& e) {
            runtimeError(this, e.what(), frames.empty() ? -1 : frames.back().currentLine);
        }
        return true;
    }

    ErrorHandler::reportRuntimeError("Can only call functions and classes", currentFileName,
                                    frames.empty() ? -1 : frames.back().currentLine, {});
    exit(1);
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
        if (methodName == "push") {
            // push(value) - add element to end
            if (argCount != 1) {
                ErrorHandler::argumentError(1, argCount, "Array.push", currentFileName, 
                                          frames.empty() ? -1 : frames.back().currentLine);
                exit(1);
            }
            arr->push(args[0]);
            result = Value(); // nil
        } else if (methodName == "pop") {
            // pop() - remove and return last element
            if (argCount != 0) {
                ErrorHandler::argumentError(0, argCount, "Array.pop", currentFileName,
                                          frames.empty() ? -1 : frames.back().currentLine);
                exit(1);
            }
            result = arr->pop();
        } else if (methodName == "slice") {
            // slice(start, end) - return subarray
            if (argCount != 2) {
                ErrorHandler::argumentError(2, argCount, "Array.slice", currentFileName,
                                          frames.empty() ? -1 : frames.back().currentLine);
                exit(1);
            }
            if (args[0].type != ValueType::NUMBER || args[1].type != ValueType::NUMBER) {
                ErrorHandler::typeError("numbers", "other type", currentFileName,
                                      frames.empty() ? -1 : frames.back().currentLine);
                exit(1);
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
            result = Value(new Array(sliced));
        } else if (methodName == "indexOf") {
            // indexOf(value) - return index of first occurrence or -1
            if (argCount != 1) {
                runtimeError("indexOf() expects 1 argument.");
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
                runtimeError("join() expects 1 argument.");
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
                runtimeError("reverse() expects 0 arguments.");
                return false;
            }
            std::reverse(arr->elements.begin(), arr->elements.end());
            result = Value(); // nil
        } else if (methodName == "sort") {
            // sort() - sort array in place (numbers then strings)
            if (argCount != 0) {
                runtimeError("sort() expects 0 arguments.");
                return false;
            }
            std::sort(arr->elements.begin(), arr->elements.end(), [](const Value& a, const Value& b) {
                // Numbers come before strings
                if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) {
                    return std::get<double>(a.as) < std::get<double>(b.as);
                } else if (a.type == ValueType::STRING && b.type == ValueType::STRING) {
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
                runtimeError("map() expects 1 argument (function).");
                return false;
            }
            if (args[0].type != ValueType::CALLABLE) {
                runtimeError("map() argument must be a function.");
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
            result = Value(new Array(mapped));
        } else if (methodName == "filter") {
            // filter(function) - filter elements by predicate
            if (argCount != 1) {
                runtimeError("filter() expects 1 argument (function).");
                return false;
            }
            if (args[0].type != ValueType::CALLABLE) {
                runtimeError("filter() argument must be a function.");
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
            result = Value(new Array(filtered));
        } else if (methodName == "find") {
            // find(function) - find first element matching predicate
            if (argCount != 1) {
                runtimeError("find() expects 1 argument (function).");
                return false;
            }
            if (args[0].type != ValueType::CALLABLE) {
                runtimeError("find() argument must be a function.");
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
            runtimeError("Unknown array method: " + methodName);
            return false;
        }
        
        // Restore stack to original size and push result
        stack.resize(stackBase);
        push(result);
        return true;
        
    } catch (const std::runtime_error& e) {
        runtimeError(e.what());
        return false;
    }
}

void VM::run(size_t minFrameDepth) {
    CallFrame* frame = &frames.back();

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk->constants[READ_BYTE()])
#define READ_STRING() (std::get<std::string>(READ_CONSTANT().as))

    for (;;) {
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case (uint8_t)OpCode::OP_RETURN: {
                Value result = pop();
                size_t return_slot_offset = frame->slot_offset;  // Save before popping
                frames.pop_back();
                if (frames.empty() || frames.size() <= minFrameDepth) {
                    // We are done executing - either the main script or we've returned to target depth
                    if (!frames.empty()) {
                        // Returning to a specific frame depth (from array method)
                        stack.resize(return_slot_offset);
                        push(result);
                    }
                    return;
                }

                frame = &frames.back();
                stack.resize(return_slot_offset);
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
                    runtimeError("OP_CLOSURE constant must be a function.");
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
                        runtimeError("Undefined variable '" + name + "'. Did you forget to import it? Use 'use " + name + ";' at the top of your file.");
                    } else {
                        runtimeError("Undefined variable '" + name + "'.");
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
                    runtimeError("Undefined variable '" + name + "'.");
                }
                globals[name] = stack.back();
                break;
            }
            case (uint8_t)OpCode::OP_SET_GLOBAL_TYPED: {
                std::string name = READ_STRING();
                
                auto it = globals.find(name);
                if (it == globals.end()) {
                    runtimeError("Undefined variable '" + name + "'.");
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
                        isValid = value.type == ValueType::STRING;
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
                                                  value.type == ValueType::STRING ? "string" :
                                                  value.type == ValueType::ARRAY ? "array" :
                                                  value.type == ValueType::OBJECT ? "object" :
                                                  "callable";
                    
                    runtimeError("Type mismatch: Cannot assign value of type '" + actualTypeName + 
                                "' to variable of type '" + expectedTypeName + "'");
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
                        isValid = value.type == ValueType::STRING;
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
                                                  value.type == ValueType::STRING ? "string" :
                                                  value.type == ValueType::ARRAY ? "array" :
                                                  value.type == ValueType::OBJECT ? "object" :
                                                  "callable";
                    
                    runtimeError("Type mismatch: Cannot assign value of type '" + actualTypeName + 
                                "' to variable of type '" + expectedTypeName + "'");
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
                        runtimeError(std::string(e.what()) + " Make sure the module is properly imported with 'use' statement.");
                    }
                } else if (object.type == ValueType::ARRAY) {
                    // Handle array properties and methods
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
                        push(Value(new BoundArrayMethod(arr, propertyName)));
                    } else {
                        runtimeError("Array does not have property '" + propertyName + "'.");
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
                                    push(Value(new BoundMethod(object, func)));
                                } else {
                                    stack.pop_back();
                                    push(methodValue);
                                }
                            } else {
                                stack.pop_back();
                                push(methodValue);
                            }
                        } else {
                            runtimeError("Property '" + propertyName + "' not found on instance.");
                        }
                    }
                } else if (object.type == ValueType::OBJECT) {
                    Object* objPtr = std::get<Object*>(object.as);
                    
                    // Check if it's an Array
                    Array* arr = dynamic_cast<Array*>(objPtr);
                    if (arr != nullptr) {
                        // Handle array methods
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
                            push(Value(new BoundArrayMethod(arr, propertyName)));
                        } else {
                            runtimeError("Array does not have property '" + propertyName + "'.");
                        }
                    } else {
                        // Check if it's an Instance
                        Instance* inst = dynamic_cast<Instance*>(objPtr);
                        if (inst != nullptr) {
                            // Check fields first
                            auto it = inst->fields.find(propertyName);
                            if (it != inst->fields.end()) {
                                stack.pop_back();
                                push(it->second);
                            } else {
                                // Check methods in the class
                                auto methIt = inst->klass->methods.find(propertyName);
                                if (methIt != inst->klass->methods.end()) {
                                    stack.pop_back();
                                    push(methIt->second);
                                } else {
                                    runtimeError("Property '" + propertyName + "' not found on instance.");
                                }
                            }
                        } else {
                            // Check if it's a JsonObject
                            JsonObject* obj = dynamic_cast<JsonObject*>(objPtr);
                            if (obj != nullptr) {
                                auto it = obj->properties.find(propertyName);
                                if (it != obj->properties.end()) {
                                    stack.pop_back();
                                    push(it->second);
                                } else {
                                    runtimeError("Property '" + propertyName + "' not found on object.");
                                }
                            } else {
                                runtimeError("Object does not support property access.");
                            }
                        }
                    }
                } else {
                    runtimeError("Only modules, arrays, and objects have properties. Cannot use dot notation on this value type.");
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
                        runtimeError("Cannot set property on this object type.");
                    }
                } else {
                    runtimeError("Only instances and objects support property assignment.");
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
                } else if (a.type == ValueType::STRING) {
                    result = (std::get<std::string>(a.as) == std::get<std::string>(b.as));
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
                } else if (a.type == ValueType::STRING) {
                    result = (std::get<std::string>(a.as) != std::get<std::string>(b.as));
                } else {
                    // For complex types (OBJECT, ARRAY, etc.), fall back to string comparison
                    result = (a.toString() != b.toString());
                }
                push(Value(result));
                break;
            }
            case (uint8_t)OpCode::OP_GREATER: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError("Operands must be numbers.");
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                push(Value(a > b));
                break;
            }
            case (uint8_t)OpCode::OP_LESS: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError("Operands must be numbers.");
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
                    runtimeError("Operands must be numbers.");
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                push(Value(a - b));
                break;
            }
            case (uint8_t)OpCode::OP_MULTIPLY: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError("Operands must be numbers.");
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                push(Value(a * b));
                break;
            }
            case (uint8_t)OpCode::OP_DIVIDE: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError("Operands must be numbers.");
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                if (b == 0) {
                    runtimeError("Division by zero.");
                }
                push(Value(a / b));
                break;
            }
            case (uint8_t)OpCode::OP_MODULO: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError("Operands must be numbers.");
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                if (b == 0) {
                    runtimeError("Modulo by zero.");
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
                    runtimeError("Operand must be a number.");
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
                                if (!callValue(stack[stack.size() - argCount - 1], argCount)) {
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
                
                                push(Value(new Array(std::move(elements))));
                                break;
            }
                        case (uint8_t)OpCode::OP_INDEX_GET: {
                                Value index = pop();
                                Value object = pop();
                
                                if (object.type == ValueType::ARRAY) {
                                        if (index.type != ValueType::NUMBER) {
                        runtimeError("Array index must be a number.");
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
                }                 else {
                    runtimeError("Only arrays support index access.");
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
                        runtimeError("Array index must be a number.");
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
                }                 else {
                    runtimeError("Only arrays support index assignment.");
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
                    if (exception.type == ValueType::STRING) {
                        errorMsg += std::get<std::string>(exception.as);
                    } else {
                        errorMsg += exception.toString();
                    }
                    runtimeError(errorMsg.c_str());
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
                    runtimeError("Internal error: Exception processing state inconsistent");
                } else {
                    // No catch and no finally - runtime error (shouldn't happen due to parser requirement)
                    runtimeError("Exception occurred but no handler available");
                }
                
                break;
            }
        }
    }
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
    if (callee.type != ValueType::CALLABLE) {
        throw std::runtime_error("Can only call functions.");
    }
    
    Callable* function = std::get<Callable*>(callee.as);
    if (function->arity() != -1 && function->arity() != (int)arguments.size()) {
        throw std::runtime_error("Expected " + std::to_string(function->arity()) + " arguments but got " + std::to_string(arguments.size()) + ".");
    }
    
    // Save current stack state
    size_t stack_size = stack.size();
    
    // Push arguments onto the stack
    for (const auto& arg : arguments) {
        push(arg);
    }
    
    // Call the function
    Value result = function->call(*this, arguments);
    
    // Restore stack state (in case the function didn't clean up properly)
    while (stack.size() > stack_size) {
        pop();
    }
    
    return result;
}

Value VM::execute_string(const std::string& source) {
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
        
        return result;
    }
    
    return Value(); // Return nil if compilation failed
}

void VM::load_module(const std::string& name) {
    // Check if module is already loaded
    if (globals.find(name) != globals.end()) {
        return; // Module already loaded
    }

    // Check for built-in modules first
    if (name == "json") {
        neutron_init_json_module(this);
        return;
    } else if (name == "fmt") {
        neutron_init_fmt_module(this);
        return;
    } else if (name == "arrays") {
        neutron_init_arrays_module(this);
        return;
    } else if (name == "time") {
        neutron_init_time_module(this);
        return;
    } else if (name == "http") {
        neutron_init_http_module(this);
        return;
    } else if (name == "math") {
        neutron_init_math_module(this);
        return;
    } else if (name == "sys") {
        neutron_init_sys_module(this);
        return;
    } else if (name == "async") {
        neutron_init_async_module(this);
        return;
    }

    std::string nt_path = "box/" + name + ".nt";
    
#ifdef __APPLE__
    std::string shared_lib_path = ".box/modules/" + name + "/" + name + ".dylib";
#else
    std::string shared_lib_path = ".box/modules/" + name + "/" + name + ".so";
#endif
    
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
        auto module = new Module(name, module_env);
        define_module(name, module);
        return;
    }

    // Try to load as a native shared library module
    void* handle = dlopen(shared_lib_path.c_str(), RTLD_LAZY);
    
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
            runtimeError("Module '" + name + "' is not a valid Neutron module: missing neutron_module_init function.");
        }
        init_func(this);
        
        // Copy the defined values from globals to the module environment
        for (const auto& pair : globals) {
            module_env->define(pair.first, pair.second);
        }
        
        // Restore globals
        globals = saved_globals;
        
        // Create the module with the populated environment
        auto module = new Module(name, module_env);
        define_module(name, module);
        return;
    } else {
        // Handle error: module not found
        runtimeError("Module '" + name + "' not found. Make sure to use 'use " + name + ";' before accessing it.");
    }
}

void VM::load_file(const std::string& filepath) {
    // Try to open the file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        // Try with module search paths
        for (const auto& search_path : module_search_paths) {
            std::string full_path = search_path + filepath;
            file.open(full_path);
            if (file.is_open()) {
                break;
            }
        }
        
        if (!file.is_open()) {
            runtimeError("File '" + filepath + "' not found.");
            return;
        }
    }
    
    // Read the file content
    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
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
            if (frame.function->chunk) {
                // Mark constants in the function's chunk
                for (const auto& constant : frame.function->chunk->constants) {
                    markValue(constant);
                }
            }
        }
    }
}

void VM::markValue(const Value& value) {
    if (value.type == ValueType::OBJECT) {
        Object* obj = std::get<Object*>(value.as);
        if (obj && !obj->is_marked) {
            obj->mark();
            
            // For specific object types, mark their internal values
            // Array - already handled below
            // Instance
            Instance* inst = dynamic_cast<Instance*>(obj);
            if (inst) {
                for (const auto& field : inst->fields) {
                    markValue(field.second);
                }
            }
            
            // JsonObject
            JsonObject* json_obj = dynamic_cast<JsonObject*>(obj);
            if (json_obj) {
                for (const auto& prop : json_obj->properties) {
                    markValue(prop.second);
                }
            }
            
            // JsonArray
            JsonArray* json_arr = dynamic_cast<JsonArray*>(obj);
            if (json_arr) {
                for (const auto& element : json_arr->elements) {
                    markValue(element);
                }
            }
            
            // BoundArrayMethod - doesn't hold other values that need marking
        }
    } else if (value.type == ValueType::ARRAY) {
        Array* arr = std::get<Array*>(value.as);
        if (arr && !arr->is_marked) {
            arr->mark();
            
            // Mark all elements in the array recursively
            for (const auto& element : arr->elements) {
                markValue(element);
            }
        }
    } else if (value.type == ValueType::INSTANCE) {
        // This case handles instances that might be stored as ValueType::INSTANCE
        // but accessed directly (they should be handled in the OBJECT case)
        Instance* inst = std::get<Instance*>(value.as);
        if (inst && !inst->is_marked) {
            inst->mark();
            
            // Mark all fields in the instance
            for (const auto& field : inst->fields) {
                markValue(field.second);
            }
        }
    } 
    // Note: Functions, Classes, and other Callables are not marked in this implementation
    // as they don't inherit from Object and marking them requires more complex implementation
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

} // namespace neutron