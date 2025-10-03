#ifndef NEUTRON_VM_H
#define NEUTRON_VM_H

#include "expr.h"
#include <vector>
#include <unordered_map>
#include <stack>
#include <memory>
#include <cstdint>
#include <variant>
#include <functional>

// Forward declarations
class Environment;

class Environment;

// Forward declarations for module registration functions
namespace neutron {
    void register_sys_functions(std::shared_ptr<Environment> env);
    void register_json_functions(std::shared_ptr<Environment> env);
    void register_convert_functions(std::shared_ptr<Environment> env);
    void register_time_functions(std::shared_ptr<Environment> env);
    void register_math_functions(std::shared_ptr<Environment> env);
}

namespace neutron {

// Forward declarations
class VM;
class Callable;
class Module;
class Object;
class Array;
class Chunk;

struct Value;
class Function;

struct CallFrame {
    Function* function;
    uint8_t* ip;
    size_t slot_offset;  // Store offset instead of raw pointer
};

enum class ValueType {
    NIL,
    BOOLEAN,
    NUMBER,
    STRING,
    ARRAY,
    OBJECT,
    CALLABLE,
    MODULE,
    CLASS,
    INSTANCE
};

using Literal = std::variant<std::nullptr_t, bool, double, std::string, Array*, Object*, Callable*, Module*, class Class*, class Instance*>;

struct Value {
    ValueType type;
    Literal as;

    Value();
    Value(std::nullptr_t);
    Value(bool value);
    Value(double value);
    Value(const std::string& value);
    Value(Array* array);
    Value(Object* object);
    Value(Callable* callable);
    Value(Module* module);
    Value(Class* klass);
    Value(Instance* instance);

    std::string toString() const;
};

class Object {
public:
    bool is_marked = false;
    virtual ~Object() = default;
    virtual std::string toString() const = 0;
    virtual void mark() { is_marked = true; }
    virtual void sweep() {} // Default implementation, can be overridden
};

class JsonObject : public Object {
public:
    std::unordered_map<std::string, Value> properties;
    std::string toString() const override {
        return "<json object>";
    }
};

class Array : public Object {
public:
    std::vector<Value> elements;
    
    Array() = default;
    Array(std::vector<Value> elements) : elements(std::move(elements)) {}
    
    std::string toString() const override;
    
    size_t size() const { return elements.size(); }
    void push(const Value& value) { elements.push_back(value); }
    Value pop() { 
        if (elements.empty()) {
            throw std::runtime_error("Cannot pop from empty array");
        }
        Value value = elements.back();
        elements.pop_back();
        return value;
    }
    Value& at(size_t index) { 
        if (index >= elements.size()) {
            throw std::runtime_error("Array index out of bounds");
        }
        return elements[index]; 
    }
    const Value& at(size_t index) const { 
        if (index >= elements.size()) {
            throw std::runtime_error("Array index out of bounds");
        }
        return elements[index]; 
    }
    void set(size_t index, const Value& value) {
        if (index >= elements.size()) {
            throw std::runtime_error("Array index out of bounds");
        }
        elements[index] = value;
    }
    
    void mark() override {
        Object::mark(); // Mark this object
        // Mark all elements in the array
        for (auto& element : elements) {
            if (element.type == ValueType::OBJECT) {
                Object* obj = std::get<Object*>(element.as);
                if (obj && !obj->is_marked) {
                    obj->mark();
                }
            } else if (element.type == ValueType::ARRAY) {
                Array* arr = std::get<Array*>(element.as);
                if (arr && !arr->is_marked) {
                    arr->mark();
                }
            }
        }
    }
};

class JsonArray : public Object {
public:
    std::vector<Value> elements;
    std::string toString() const override {
        return "<json array>";
    }
};

// Native primitive functions for string/char manipulation
Value native_char_to_int(std::vector<Value> arguments);
Value native_int_to_char(std::vector<Value> arguments);
Value native_string_get_char_at(std::vector<Value> arguments);
Value native_string_length(std::vector<Value> arguments);
Value native_say(std::vector<Value> arguments);

// Native functions for arrays
Value native_array_new(std::vector<Value> arguments);
Value native_array_push(std::vector<Value> arguments);
Value native_array_pop(std::vector<Value> arguments);
Value native_array_length(std::vector<Value> arguments);
Value native_array_at(std::vector<Value> arguments);
Value native_array_set(std::vector<Value> arguments);

class Environment {
public:
    std::shared_ptr<Environment> enclosing;
    std::unordered_map<std::string, Value> values;

    Environment();
    Environment(std::shared_ptr<Environment> enclosing);
    
    void define(const std::string& name, const Value& value);
    Value get(const std::string& name);
    void assign(const std::string& name, const Value& value);
};

class Module {
public:
    std::string name;
    std::shared_ptr<Environment> env;

    Module(const std::string& name, std::shared_ptr<Environment> env)
        : name(name), env(env) {}

    Value get(const std::string& name) {
        return env->get(name);
    }

    void define(const std::string& name, const Value& value) {
        env->define(name, value);
    }
};

class Callable {
public:
    virtual ~Callable() = default;
    virtual int arity() = 0;
    virtual Value call(VM& vm, std::vector<Value> arguments) = 0;
    virtual std::string toString() = 0;
    virtual bool isCNativeFn() const { return false; }
};

class Class;
class Instance;

class BoundMethod : public Callable {
public:
    BoundMethod(Value receiver, Function* method);
    int arity() override;
    Value call(VM& vm, std::vector<Value> arguments) override;
    std::string toString() override;
    
    Value receiver;
    Function* method;
};

class Class : public Callable {
public:
    Class(const std::string& name);
    Class(const std::string& name, std::shared_ptr<Environment> class_env);
    int arity() override;
    Value call(VM& vm, std::vector<Value> arguments) override;
    std::string toString() override;
    
    std::string name;
    std::shared_ptr<Environment> class_env;  // Store the environment for this class
    std::unordered_map<std::string, Value> methods;  // Store method definitions
};

class Instance : public Object {
public:
    Instance(Class* klass);
    std::string toString() const override;
    
    Class* klass;
    std::unordered_map<std::string, Value> fields;
};

class Function : public Callable {
public:
    Function(const FunctionStmt* declaration, std::shared_ptr<Environment> closure);
    int arity() override;
    Value call(VM& vm, std::vector<Value> arguments) override;
    std::string toString() override;

    std::string name;
    int arity_val;
    Chunk* chunk;
private:
    const FunctionStmt* declaration;
    std::shared_ptr<Environment> closure;
};

class NativeFn : public Callable {
public:
    using NativeFnPtr = std::function<Value(std::vector<Value>)>;

    NativeFn(NativeFnPtr function, int arity);
    int arity() override;
    Value call(VM& vm, std::vector<Value> arguments) override;
    std::string toString() override;

private:
    NativeFnPtr function;
    int _arity;
};

class Return {
public:
    Value value;
    Return(Value value) : value(value) {}
};

class VM {
public:
    VM();
    void interpret(Function* function);
    void push(const Value& value);
    Value pop();
    void define_native(const std::string& name, Callable* function);
    void define_module(const std::string& name, Module* module);
    void define(const std::string& name, const Value& value);
    void init_module(const std::string& name, std::function<void(Module*)> init_fn);
    void load_module(const std::string& name);
    Value call(const Value& callee, const std::vector<Value>& arguments);
    Value execute_string(const std::string& source);
    void add_module_search_path(const std::string& path);
    
    // Memory management functions
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        T* obj = new T(std::forward<Args>(args)...);
        heap.push_back(obj);
        return obj;
    }

private:
    bool call(Function* function, int argCount);
    bool callValue(Value callee, int argCount);
    void run();
    void interpret_module(const std::vector<std::unique_ptr<Stmt>>& statements, std::shared_ptr<Environment> module_env);
    
    // Garbage collection methods
    void markRoots();
    void markValue(const Value& value);
    void collectGarbage();
    void sweep();

    std::vector<CallFrame> frames;
    Chunk* chunk;
    uint8_t* ip;
    std::vector<Value> stack;
    std::unordered_map<std::string, Value> globals;
    
    std::vector<std::string> module_search_paths;
    
    // Memory management
    std::vector<Object*> heap;
    size_t nextGC;
};

} // namespace neutron

#endif // NEUTRON_VM_H