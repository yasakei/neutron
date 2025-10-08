#include "vm.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include "compiler/scanner.h"
#include "compiler/parser.h"
#include <cmath>

namespace neutron {

// Helper to check if a value is "truthy"






NativeFn::NativeFn(NativeFnPtr function, int arity) : function(std::move(function)), _arity(arity), _needsVM(false) {}

NativeFn::NativeFn(NativeFnPtrWithVM function, int arity, bool needsVM) 
    : functionWithVM(std::move(function)), _arity(arity), _needsVM(needsVM) {}

int NativeFn::arity() {
    return _arity;
}

Value NativeFn::call(VM& vm, std::vector<Value> arguments) {
    if (_needsVM) {
        return functionWithVM(vm, arguments);
    }
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
    // This shouldn't be called directly - the VM's callValue handles BoundMethod
    // But if it is, just delegate to the underlying method
    return method->call(vm, arguments);
}

std::string BoundMethod::toString() {
    return "<bound method>";
}

Class::Class(const std::string& name)
    : name(name), class_env(nullptr) {}

Class::Class(const std::string& name, std::shared_ptr<Environment> class_env)
    : name(name), class_env(class_env) {}

int Class::arity() {
    // For now, return 0 - the constructor implementation would define this properly
    return 0;
}

Value Class::call(VM& vm, std::vector<Value> arguments) {
    (void)vm; // Unused parameter
    (void)arguments; // Unused parameter
    // Create a new instance of this class
    Instance* instance = new Instance(this);
    return Value(instance);
}

std::string Class::toString() {
    return name;
}

Instance::Instance(Class* klass)
    : klass(klass) {}

std::string Instance::toString() const {
    return "<" + klass->name + " instance>";
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
