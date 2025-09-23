#include "vm.h"
#include "compiler.h"
#include "bytecode.h"
#include "debug.h"
#include "scanner.h"
#include "parser.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <dlfcn.h>

// Include module registration functions
#include "libs/sys/native.h"
#include "libs/json/native.h"
#include "libs/convert/native.h"
#include "libs/time/native.h"
#include "libs/math/native.h"
#include "module_registry.h"

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
    globals["say"] = Value(new NativeFn(std::function<Value(std::vector<Value>)>(native_say), 1));
    
    // Create a shared environment for global functions
    auto globalEnv = std::make_shared<Environment>();
    
    // Register module functions
    register_sys_functions(globalEnv);
    register_json_functions(globalEnv);
    register_convert_functions(globalEnv);
    register_time_functions(globalEnv);
    
    // Create math module environment and register math functions
    auto mathEnv = std::make_shared<Environment>();
    register_math_functions(mathEnv);
    auto mathModule = new Module("math", mathEnv);
    globals["math"] = Value(mathModule);
    
    run_external_module_initializers(this);
    
    // Copy registered functions to globals
    for (const auto& pair : globalEnv->values) {
        globals[pair.first] = pair.second;
    }
}

void VM::interpret(Function* function) {
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

void neutron::VM::define_native(const std::string& name, Callable* function) {
    globals[name] = Value(function);
}

void neutron::VM::define_module(const std::string& name, Module* module) {
    globals[name] = Value(module);
}

neutron::Value neutron::VM::call(const neutron::Value& callee, const std::vector<neutron::Value>& arguments) {
    if (callee.type != ValueType::FUNCTION) {
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

neutron::Value neutron::VM::execute_string(const std::string& source) {
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

void neutron::VM::load_module(const std::string& name) {
    std::string nt_path = "box/" + name + ".nt";
    std::string so_path = "box/" + name + "/" + name + ".so";
    std::string module_nt_path = "box/" + name + "/" + name + ".nt";

    // First, check for a module-specific .nt file
    std::ifstream module_nt_file(module_nt_path);
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

    // Check for a standalone .nt file
    std::ifstream nt_file(nt_path);
    if (nt_file.is_open()) {
        // It's a Neutron module, we need to execute it.
        std::string source((std::istreambuf_iterator<char>(nt_file)),
                            std::istreambuf_iterator<char>());
        nt_file.close();
        
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

    void* handle = dlopen(so_path.c_str(), RTLD_LAZY);
    if (handle) {
        void (*init_func)(VM*) = (void (*)(VM*))dlsym(handle, "neutron_module_init");
        if (init_func) {
            init_func(this);
        } else {
            dlclose(handle);
            // Handle error: init function not found
        }
    } else {
        // Handle error: module not found
    }
}


