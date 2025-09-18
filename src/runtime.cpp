#include "vm.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include "scanner.h"
#include "parser.h"
#include <cmath>

namespace neutron {

// Helper to check if a value is "truthy"
bool isTruthy(const Value& value) {
    if (value.type == ValueType::NIL) return false;
    if (value.type == ValueType::BOOLEAN) return std::get<bool>(value.as);
    return true; // All other types are truthy
}

Value::Value() : type(ValueType::NIL), as(nullptr) {}

Value::Value(std::nullptr_t) : type(ValueType::NIL), as(nullptr) {}

Value::Value(bool value) : type(ValueType::BOOLEAN), as(value) {}

Value::Value(double value) : type(ValueType::NUMBER), as(value) {}

Value::Value(const std::string& value) : type(ValueType::STRING), as(value) {}

Value::Value(Object* object) : type(ValueType::OBJECT), as(object) {}

Value::Value(Callable* function) : type(ValueType::FUNCTION), as(function) {}

Value::Value(Module* module) : type(ValueType::MODULE), as(module) {}

std::string Value::toString() const {
    switch (type) {
        case ValueType::NIL:
            return "nil";
        case ValueType::BOOLEAN:
            return std::get<bool>(as) ? "true" : "false";
        case ValueType::NUMBER: {
            // Convert double to string, removing trailing zeros
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%.15g", std::get<double>(as));
            return std::string(buffer);
        }
        case ValueType::STRING:
            return std::get<std::string>(as);
        case ValueType::OBJECT:
            return std::get<Object*>(as)->toString();
        case ValueType::FUNCTION:
            return std::get<Callable*>(as)->toString();
        case ValueType::MODULE:
            return "<module>";
    }
    return "";
}

Environment::Environment() : enclosing(nullptr) {}

Environment::Environment(std::shared_ptr<Environment> enclosing) : enclosing(enclosing) {}

void Environment::define(const std::string& name, const Value& value) {
    values[name] = value;
}

Value Environment::get(const std::string& name) {
    auto it = values.find(name);
    if (it != values.end()) {
        return it->second;
    }

    if (enclosing != nullptr) {
        return enclosing->get(name);
    }

    throw std::runtime_error("Undefined variable '" + name + "'.");
}

void Environment::assign(const std::string& name, const Value& value) {
    auto it = values.find(name);
    if (it != values.end()) {
        values[name] = value;
        return;
    }

    if (enclosing != nullptr) {
        enclosing->assign(name, value);
        return;
    }

    throw std::runtime_error("Undefined variable '" + name + "'.");
}

Function::Function(const FunctionStmt* declaration, std::shared_ptr<Environment> closure)
    : declaration(declaration), closure(closure) {}

int Function::arity() {
    if (declaration) {
        return declaration->params.size();
    }
    return 0;
}

Value Function::call(VM& vm, std::vector<Value> arguments) {
    // For now, we'll just execute the function and return nil
    // A complete implementation would require more complex changes to handle return values properly
    return Value(nullptr);
}

std::string Function::toString() {
    if (declaration) {
        return "<fn " + declaration->name.lexeme + ">";
    }
    return "<script>";
}

NativeFn::NativeFn(NativeFnPtr function, int arity) : function(std::move(function)), _arity(arity) {}

int NativeFn::arity() {
    return _arity;
}

Value NativeFn::call(VM& /*vm*/, std::vector<Value> arguments) {
    return function(arguments);
}

std::string NativeFn::toString() {
    return "<native fn>";
}

Module::Module(std::string name, std::shared_ptr<Environment> environment, std::vector<std::unique_ptr<Stmt>> statements)
    : name(name), environment(environment), statements(std::move(statements)) {}

Value Module::get(const std::string& name) {
    return environment->get(name);
}

Value native_say(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        // error
    }
    std::cout << arguments[0].toString() << std::endl;
    return Value();
}

} // namespace neutron
