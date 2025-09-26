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


Value::Value() : type(ValueType::NIL), as(nullptr) {}

Value::Value(std::nullptr_t) : type(ValueType::NIL), as(nullptr) {}

Value::Value(bool value) : type(ValueType::BOOLEAN), as(value) {}

Value::Value(double value) : type(ValueType::NUMBER), as(value) {}

Value::Value(const std::string& value) : type(ValueType::STRING), as(value) {}

Value::Value(Array* array) : type(ValueType::ARRAY), as(array) {}

Value::Value(Object* object) : type(ValueType::OBJECT), as(object) {}

Value::Value(Module* module) : type(ValueType::MODULE), as(module) {}
Value::Value(Class* klass) : type(ValueType::CLASS), as(klass) {}
Value::Value(Instance* instance) : type(ValueType::INSTANCE), as(instance) {}

std::string Array::toString() const {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << elements[i].toString();
    }
    oss << "]";
    return oss.str();
}

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
        case ValueType::ARRAY:
            return std::get<Array*>(as)->toString();
        case ValueType::OBJECT:
            return std::get<Object*>(as)->toString();
        case ValueType::CALLABLE: {
            Callable* callable = std::get<Callable*>(as);
            if (callable == nullptr) {
                return "<null callable>";
            }
            return callable->toString();
        }
        case ValueType::MODULE:
            return "<module>";
        case ValueType::CLASS:
            return std::get<Class*>(as)->toString();
        case ValueType::INSTANCE:
            return std::get<Instance*>(as)->toString();
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

BoundMethod::BoundMethod(Value receiver, Function* method) 
    : receiver(receiver), method(method) {}

int BoundMethod::arity() {
    return method->arity();
}

Value BoundMethod::call(VM& vm, std::vector<Value> arguments) {
    // For now, just call the underlying method without 'this' context
    // Proper 'this' implementation would require more complex changes
    return method->call(vm, arguments);
}

std::string BoundMethod::toString() {
    return "<bound method>";
}

Module::Module(std::string name, std::shared_ptr<Environment> environment, std::vector<std::unique_ptr<Stmt>> statements)
    : name(name), environment(environment), statements(std::move(statements)) {}

Module::Module(std::string name, std::shared_ptr<Environment> environment)
    : name(name), environment(environment) {}

Value Module::get(const std::string& name) {
    return environment->get(name);
}

Value native_say(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("say expects exactly 1 argument");
    }
    std::cout << arguments[0].toString() << std::endl;
    return Value();
}

Value native_array_new(std::vector<Value> /*arguments*/) {
    return Value(new Array(std::vector<Value>()));
}

Value native_array_push(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("array_push expects exactly 2 arguments: array and value");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("First argument must be an array");
    }
    
    Array* array = std::get<Array*>(arguments[0].as);
    array->push(arguments[1]);
    
    return arguments[1]; // Return the pushed value
}

Value native_array_pop(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("array_pop expects exactly 1 argument: array");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Argument must be an array");
    }
    
    Array* array = std::get<Array*>(arguments[0].as);
    if (array->size() == 0) {
        throw std::runtime_error("Cannot pop from empty array");
    }
    
    return array->pop();
}

Value native_array_length(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("array_length expects exactly 1 argument: array");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Argument must be an array");
    }
    
    Array* array = std::get<Array*>(arguments[0].as);
    return Value(static_cast<double>(array->size()));
}

Value native_array_at(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("array_at expects exactly 2 arguments: array and index");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("First argument must be an array");
    }
    
    if (arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Second argument must be a number (index)");
    }
    
    Array* array = std::get<Array*>(arguments[0].as);
    int index = static_cast<int>(std::get<double>(arguments[1].as));
    
    if (index < 0 || static_cast<size_t>(index) >= array->size()) {
        throw std::runtime_error("Array index out of bounds");
    }
    
    return array->at(index);
}

Value native_array_set(std::vector<Value> arguments) {
    if (arguments.size() != 3) {
        throw std::runtime_error("array_set expects exactly 3 arguments: array, index, and value");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("First argument must be an array");
    }
    
    if (arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Second argument must be a number (index)");
    }
    
    Array* array = std::get<Array*>(arguments[0].as);
    int index = static_cast<int>(std::get<double>(arguments[1].as));
    const Value& value = arguments[2];
    
    if (index < 0 || static_cast<size_t>(index) >= array->size()) {
        throw std::runtime_error("Array index out of bounds");
    }
    
    array->set(index, value);
    return value; // Return the set value
}

} // namespace neutron
