#include "vm.h"
#include "compiler.h"
#include "bytecode.h"
#include "debug.h"
#include <iostream>
#include <stdexcept>

// Include module registration functions
#include "libs/sys/native.h"
#include "libs/json/native.h"
#include "libs/convert/native.h"
#include "libs/time/native.h"

namespace neutron {

// Forward declaration of isTruthy function
bool isTruthy(const Value& value);

// Helper function for error reporting
void runtimeError(const std::string& message) {
    std::cerr << "Runtime error: " << message << std::endl;
    // In a real implementation, we might want to unwind the call stack here
    exit(1);
}

VM::VM() : chunk(nullptr), ip(nullptr) {
    globals["say"] = Value(new NativeFn(native_say, 1));
    
    // Create a shared environment for global functions
    auto globalEnv = std::make_shared<Environment>();
    
    // Register module functions
    register_sys_functions(globalEnv);
    register_json_functions(globalEnv);
    register_convert_functions(globalEnv);
    register_time_functions(globalEnv);
    
    // Copy registered functions to globals
    for (const auto& pair : globalEnv->values) {
        globals[pair.first] = pair.second;
    }
}

void VM::interpret(Function* function) {
    std::cout << "interpret" << std::endl;
    this->chunk = function->chunk;
    this->ip = &this->chunk->code[0];
    run();
}

void VM::push(const Value& value) {
    if (stack.size() >= 256) {
        runtimeError("Stack overflow.");
    }
    stack.push_back(value);
}

Value VM::pop() {
    if (stack.empty()) {
        runtimeError("Stack underflow.");
    }
    Value value = stack.back();
    stack.pop_back();
    return value;
}

void VM::run() {
    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("ip: %p, stack size: %zu\n", ip, stack.size());
        std::cout << "          ";
        for (const auto& value : stack) {
            printf("[ ");
            std::cout << value.toString();
            printf(" ]");
        }
        std::cout << std::endl;
        disassembleInstruction(chunk, (int)(ip - &chunk->code[0]));
#endif
        switch ((OpCode)*ip++) {
            case OpCode::OP_RETURN: {
                // For now, we'll just return
                // A complete implementation would handle return values properly
                return;
            }
            case OpCode::OP_CONSTANT: {
                Value constant = chunk->constants[*ip++];
                push(constant);
                break;
            }
            case OpCode::OP_NIL: {
                push(Value());
                break;
            }
            case OpCode::OP_TRUE: {
                push(Value(true));
                break;
            }
            case OpCode::OP_FALSE: {
                push(Value(false));
                break;
            }
            case OpCode::OP_POP: {
                pop();
                break;
            }
            case OpCode::OP_GET_LOCAL: {
                uint8_t slot = *ip++;
                push(stack[slot]);
                break;
            }
            case OpCode::OP_SET_LOCAL: {
                uint8_t slot = *ip++;
                stack[slot] = stack.back();
                break;
            }
            case OpCode::OP_GET_GLOBAL: {
                std::string name = std::get<std::string>(chunk->constants[*ip++].as);
                auto it = globals.find(name);
                if (it == globals.end()) {
                    runtimeError("Undefined variable '" + name + "'.");
                }
                push(globals[name]);
                break;
            }
            case OpCode::OP_DEFINE_GLOBAL: {
                std::string name = std::get<std::string>(chunk->constants[*ip++].as);
                globals[name] = stack.back();
                pop();
                break;
            }
            case OpCode::OP_SET_GLOBAL: {
                std::string name = std::get<std::string>(chunk->constants[*ip++].as);
                auto it = globals.find(name);
                if (it == globals.end()) {
                    runtimeError("Undefined variable '" + name + "'.");
                }
                globals[name] = stack.back();
                break;
            }
            case OpCode::OP_GET_PROPERTY: {
                std::string propertyName = std::get<std::string>(chunk->constants[*ip++].as);
                Value object = stack.back();
                
                // Check if the object is a module
                if (object.type == ValueType::MODULE) {
                    Module* module = std::get<Module*>(object.as);
                    try {
                        Value property = module->get(propertyName);
                        // Replace the object on the stack with the property
                        stack.pop_back();
                        push(property);
                    } catch (const std::runtime_error& e) {
                        runtimeError(e.what());
                    }
                } else {
                    runtimeError("Only modules have properties.");
                }
                break;
            }
            case OpCode::OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(Value(a.type == b.type && a.toString() == b.toString()));
                break;
            }
            case OpCode::OP_GREATER: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError("Operands must be numbers.");
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                push(Value(a > b));
                break;
            }
            case OpCode::OP_LESS: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError("Operands must be numbers.");
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                push(Value(a < b));
                break;
            }
            case OpCode::OP_ADD: {
                Value b = pop();
                Value a = pop();
                
                if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) {
                    double num_a = std::get<double>(a.as);
                    double num_b = std::get<double>(b.as);
                    push(Value(num_a + num_b));
                } else {
                    // Convert both operands to strings and concatenate
                    std::string str_a = a.toString();
                    std::string str_b = b.toString();
                    push(Value(str_a + str_b));
                }
                break;
            }
            case OpCode::OP_SUBTRACT: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError("Operands must be numbers.");
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                push(Value(a - b));
                break;
            }
            case OpCode::OP_MULTIPLY: {
                if (stack.back().type != ValueType::NUMBER || stack[stack.size() - 2].type != ValueType::NUMBER) {
                    runtimeError("Operands must be numbers.");
                }
                double b = std::get<double>(pop().as);
                double a = std::get<double>(pop().as);
                push(Value(a * b));
                break;
            }
            case OpCode::OP_DIVIDE: {
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
            case OpCode::OP_NOT: {
                Value value = pop();
                push(Value(!isTruthy(value)));
                break;
            }
            case OpCode::OP_NEGATE: {
                if (stack.back().type != ValueType::NUMBER) {
                    runtimeError("Operand must be a number.");
                }
                double value = std::get<double>(pop().as);
                push(Value(-value));
                break;
            }
            case OpCode::OP_SAY: {
                std::cout << pop().toString() << std::endl;
                break;
            }
            case OpCode::OP_JUMP: {
                uint16_t offset = (uint16_t)(ip[0] << 8) | ip[1];
                ip += 2; // Skip the offset
                ip += offset;
                break;
            }
            case OpCode::OP_JUMP_IF_FALSE: {
                uint16_t offset = (uint16_t)(ip[0] << 8) | ip[1];
                ip += 2; // Skip the offset
                Value condition = pop(); // Pop the condition value
                if (condition.type == ValueType::NIL || (condition.type == ValueType::BOOLEAN && !std::get<bool>(condition.as))) {
                    ip += offset;
                }
                break;
            }
            case OpCode::OP_LOOP: {
                uint16_t offset = (uint16_t)(ip[0] << 8) | ip[1];
                ip += 2; // Skip the offset
                ip -= offset;
                break;
            }
            case OpCode::OP_CALL: {
                uint8_t argCount = *ip++;
                // The last value on the stack is the function to call
                // The values before that are the arguments
                Value callee = stack[stack.size() - 1 - argCount];
                
                if (callee.type != ValueType::FUNCTION) {
                    runtimeError("Can only call functions and classes.");
                    break;
                }
                
                Callable* function = std::get<Callable*>(callee.as);
                if (function->arity() != -1 && function->arity() != argCount) {
                    runtimeError("Expected " + std::to_string(function->arity()) + " arguments but got " + std::to_string(argCount) + ".");
                    break;
                }
                
                // Extract arguments
                std::vector<Value> arguments;
                for (int i = 0; i < argCount; i++) {
                    arguments.push_back(stack[stack.size() - argCount + i]);
                }
                
                // Remove the arguments and the function from the stack
                for (int i = 0; i < argCount + 1; i++) {
                    pop();
                }
                
                // Call the function and push the result
                Value result = function->call(*this, arguments);
                push(result);
                break;
            }
        }
    }
}

} // namespace neutron
