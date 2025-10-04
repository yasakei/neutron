#include "vm.h"
#include "runtime/native_functions.h"
#include "sys/native.h"
#include "compiler/compiler.h"
#include "compiler/bytecode.h"
#include "runtime/debug.h"
#include "compiler/scanner.h"
#include "compiler/parser.h"
#include "types/json_object.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <dlfcn.h>
#include <cmath>

#include "capi.h"
#include "json/native.h"
#include "convert/native.h"
#include "time/native.h"
#include "math/native.h"
#include "http/native.h"
#include "modules/module_registry.h"

namespace neutron {
    
// External symbols needed by bytecode_runner
extern "C" {
    const unsigned char bytecode_data[] = {0};  // Empty bytecode array
    const unsigned int bytecode_size = 0;       // Size is 0
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

// Helper function for error reporting
void runtimeError(const std::string& message) {
    std::cerr << "Runtime error: " << message << std::endl;
    // In a real implementation, we might want to unwind the call stack here
    exit(1);
}

VM::VM() : ip(nullptr), nextGC(1024) {  // Start GC at 1024 bytes
    globals["say"] = Value(new NativeFn(std::function<Value(std::vector<Value>)>(native_say), 1));
    
    // Register array functions
    globals["array_new"] = Value(new NativeFn(std::function<Value(std::vector<Value>)>(native_array_new), 0));
    globals["array_push"] = Value(new NativeFn(std::function<Value(std::vector<Value>)>(native_array_push), 2));
    globals["array_pop"] = Value(new NativeFn(std::function<Value(std::vector<Value>)>(native_array_pop), 1));
    globals["array_length"] = Value(new NativeFn(std::function<Value(std::vector<Value>)>(native_array_length), 1));
    globals["array_at"] = Value(new NativeFn(std::function<Value(std::vector<Value>)>(native_array_at), 2));
    globals["array_set"] = Value(new NativeFn(std::function<Value(std::vector<Value>)>(native_array_set), 3));
    
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

bool VM::call(Function* function, int argCount) {
    if (argCount != function->arity_val) {
        runtimeError("Expected " + std::to_string(function->arity_val) + " arguments but got " + std::to_string(argCount) + ".");
        return false;
    }

    if (frames.size() == 256) {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame* frame = &frames.emplace_back();
    frame->function = function;
    if (function->chunk->code.empty()) {
        frame->ip = nullptr;
    } else {
        frame->ip = function->chunk->code.data();
    }
    frame->slot_offset = stack.size() - argCount;  // Store offset instead of pointer

    return true;
}

bool VM::callValue(Value callee, int argCount) {
    if (callee.type == ValueType::CALLABLE) {
        Callable* callable = std::get<Callable*>(callee.as);
        if (Function* function = dynamic_cast<Function*>(callable)) {
            return call(function, argCount);
        } else if (NativeFn* native = dynamic_cast<NativeFn*>(callable)) {
            if (native->arity() != -1 && native->arity() != argCount) {
                runtimeError("Expected " + std::to_string(native->arity()) + " arguments but got " + std::to_string(argCount) + ".");
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
            } catch (const std::runtime_error& e) {
                runtimeError(e.what());
                return false;
            }
            return true;
        } else if (callable->isCNativeFn()) {
            if (callable->arity() != -1 && callable->arity() != argCount) {
                runtimeError("Expected " + std::to_string(callable->arity()) + " arguments but got " + std::to_string(argCount) + ".");
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
            } catch (const std::runtime_error& e) {
                runtimeError(e.what());
                return false;
            }
            return true;
        }
    }

    runtimeError("Can only call functions and classes.");
    return false;
}

void VM::run() {
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
                if (frames.empty()) {
                    // We are done executing the main script. The result is on the stack.
                    // The extra pop is not needed.
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
                        name == "time" || name == "convert") {
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
            case (uint8_t)OpCode::OP_SET_GLOBAL: {
                std::string name = READ_STRING();
                auto it = globals.find(name);
                if (it == globals.end()) {
                    runtimeError("Undefined variable '" + name + "'.");
                }
                globals[name] = stack.back();
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
                } else if (object.type == ValueType::OBJECT) {
                    Object* objPtr = std::get<Object*>(object.as);
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
                } else {
                    runtimeError("Only modules and objects have properties. Cannot use dot notation on this value type.");
                }
                break;
            }
            case (uint8_t)OpCode::OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(Value(a.type == b.type && a.toString() == b.toString()));
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
                        runtimeError("Array index out of bounds.");
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
                        runtimeError("Array index out of bounds.");
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
    } else if (name == "convert") {
        neutron_init_convert_module(this);
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
    }

    std::string nt_path = "box/" + name + ".nt";
    
#ifdef __APPLE__
    std::string shared_lib_path = "box/" + name + "/" + name + ".dylib";
#else
    std::string shared_lib_path = "box/" + name + "/" + name + ".so";
#endif
    
    std::string module_nt_path = "box/" + name + "/" + name + ".nt";

    // Search paths for .nt modules
    std::vector<std::string> search_paths = {
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
    
    // Mark all objects in environments
    // This would require recursively marking values in all environments
    // For simplicity, we'll just mark the globals
}

void VM::markValue(const Value& value) {
    if (value.type == ValueType::OBJECT) {
        Object* obj = std::get<Object*>(value.as);
        if (obj && !obj->is_marked) {
            obj->mark();
        }
    } else if (value.type == ValueType::ARRAY) {
        Array* arr = std::get<Array*>(value.as);
        if (arr && !arr->is_marked) {
            arr->mark();
        }
    }
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

} // namespace neutron