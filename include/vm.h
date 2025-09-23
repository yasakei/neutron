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

// Forward declarations for module registration functions
namespace neutron {
    void register_sys_functions(std::shared_ptr<Environment> env);
    void register_json_functions(std::shared_ptr<Environment> env);
    void register_convert_functions(std::shared_ptr<Environment> env);
}

namespace neutron {

// Forward declarations
class VM;
class Callable;
class Module;
class Object;
class Chunk;

enum class ValueType {
    NIL,
    BOOLEAN,
    NUMBER,
    STRING,
    OBJECT,
    FUNCTION,
    MODULE
};

using Literal = std::variant<std::nullptr_t, bool, double, std::string, Object*, Callable*, Module*>;

struct Value {
    ValueType type;
    Literal as;

    Value();
    Value(std::nullptr_t);
    Value(bool value);
    Value(double value);
    Value(const std::string& value);
    Value(Object* object);
    Value(Callable* function);
    Value(Module* module);

    std::string toString() const;
};

class Object {
public:
    virtual ~Object() = default;
    virtual std::string toString() const = 0;
};

class JsonObject : public Object {
public:
    std::unordered_map<std::string, Value> properties;
    std::string toString() const override {
        return "<json object>";
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
    std::shared_ptr<Environment> environment;
    std::vector<std::unique_ptr<Stmt>> statements;

    Module(std::string name, std::shared_ptr<Environment> environment, std::vector<std::unique_ptr<Stmt>> statements);
    Module(std::string name, std::shared_ptr<Environment> environment);  // New constructor
    Value get(const std::string& name);
};

class Callable {
public:
    virtual ~Callable() = default;
    virtual int arity() = 0;
    virtual Value call(VM& vm, std::vector<Value> arguments) = 0;
    virtual std::string toString() = 0;
};

class Function : public Callable {
public:
    Function(const FunctionStmt* declaration, std::shared_ptr<Environment> closure);
    int arity() override;
    Value call(VM& vm, std::vector<Value> arguments) override;
    std::string toString() override;

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
    void load_module(const std::string& name);
    Value call(const Value& callee, const std::vector<Value>& arguments);
    Value execute_string(const std::string& source);

private:
    void run();
    void interpret_module(const std::vector<std::unique_ptr<Stmt>>& statements, std::shared_ptr<Environment> module_env);

    Chunk* chunk;
    uint8_t* ip;
    std::vector<Value> stack;
    std::unordered_map<std::string, Value> globals;
};

} // namespace neutron

#endif // NEUTRON_VM_H