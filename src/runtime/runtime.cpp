/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, for both open source and commercial purposes.
 * 
 * Conditions:
 * 
 * 1. The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 * 
 * 2. Attribution is appreciated but NOT required.
 *    Suggested (optional) credit:
 *    "Built using Neutron Programming Language (c) yasakei"
 * 
 * 3. The name "Neutron" and the name of the copyright holder may not be used
 *    to endorse or promote products derived from this Software without prior
 *    written permission.
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

std::string NativeFn::toString() const {
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

std::string BoundMethod::toString() const {
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
    // Inline fields are already zero-initialized
}

Instance::~Instance() {
    delete overflowFields;
}

Value* Instance::getField(ObjString* key) {
    // Fast path: check inline fields first
    for (uint8_t i = 0; i < inlineCount; ++i) {
        if (inlineFields[i].key == key) {
            return &inlineFields[i].value;
        }
    }
    
    // Slow path: check overflow map
    if (overflowFields) {
        auto it = overflowFields->find(key);
        if (it != overflowFields->end()) {
            return &it->second;
        }
    }
    
    return nullptr;
}

void Instance::setField(ObjString* key, const Value& value) {
    // Fast path: check if key already exists in inline fields
    for (uint8_t i = 0; i < inlineCount; ++i) {
        if (inlineFields[i].key == key) {
            inlineFields[i].value = value;
            return;
        }
    }
    
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
