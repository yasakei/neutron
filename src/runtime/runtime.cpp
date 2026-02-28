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






NativeFn::NativeFn(NativeFnPtr function, int arity) : function(std::move(function)), _arity(arity), _needsVM(false) { obj_type = ObjType::OBJ_NATIVE_FN; }

NativeFn::NativeFn(NativeFnPtrWithVM function, int arity, bool needsVM) 
    : functionWithVM(std::move(function)), _arity(arity), _needsVM(needsVM) { obj_type = ObjType::OBJ_NATIVE_FN; }

int NativeFn::arity() {
    return _arity;
}

Value NativeFn::call(VM& vm, std::vector<Value> arguments) {
    if (_needsVM) {
        return functionWithVM(vm, arguments);
    }
    return function(arguments);
}

std::string NativeFn::toString() const {
    return "<native fn>";
}

BoundMethod::BoundMethod(Value receiver, Function* method) 
    : receiver(receiver), method(method) { obj_type = ObjType::OBJ_BOUND_METHOD; }

int BoundMethod::arity() {
    return method->arity();
}

Value BoundMethod::call(VM& vm, std::vector<Value> arguments) {
    // This shouldn't be called directly - the VM's callValue handles BoundMethod
    // But if it is, just delegate to the underlying method
    return method->call(vm, arguments);
}

std::string BoundMethod::toString() const {
    return "<bound method>";
}

Class::Class(const std::string& name)
    : name(name), class_env(nullptr) { obj_type = ObjType::OBJ_CLASS; }

Class::Class(const std::string& name, std::shared_ptr<Environment> class_env)
    : name(name), class_env(class_env) { obj_type = ObjType::OBJ_CLASS; }

int Class::arity() {
    // For now, return 0 - the constructor implementation would define this properly
    return 0;
}

Value Class::call(VM& vm, std::vector<Value> arguments) {
    (void)arguments; // Unused parameter
    // Create a new instance of this class
    Instance* instance = vm.allocate<Instance>(this);
    return Value(instance);
}

std::string Class::toString() const {
    return name;
}

Instance::Instance(Class* klass)
    : klass(klass), inlineCount(0), overflowFields(nullptr) {
    obj_type = ObjType::OBJ_INSTANCE;
}

Instance::~Instance() {
    delete overflowFields;
}

void Instance::reset(Class* newKlass) {
    klass = newKlass;
    // Clear inline fields
    for (uint8_t i = 0; i < inlineCount; ++i) {
        inlineFields[i].key = nullptr;
    }
    inlineCount = 0;
    // Clear overflow if present
    if (overflowFields) {
        delete overflowFields;
        overflowFields = nullptr;
    }
}

// getField and setField fast paths are inlined in instance.h
// This is the slow path for setField: handles new fields, overflow map, etc.
void Instance::setFieldSlow(ObjString* key, const Value& value) {
    // Check overflow map if it exists
    if (overflowFields) {
        auto it = overflowFields->find(key);
        if (it != overflowFields->end()) {
            it->second = value;
            return;
        }
    }
    
    // New field - try inline first
    if (inlineCount < INLINE_FIELD_COUNT) {
        inlineFields[inlineCount].key = key;
        inlineFields[inlineCount].value = value;
        ++inlineCount;
        return;
    }
    
    // Overflow to hash map
    if (!overflowFields) {
        overflowFields = new std::unordered_map<ObjString*, Value, ObjStringHash, ObjStringPtrEqual>();
    }
    (*overflowFields)[key] = value;
}

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



} // namespace neutron
