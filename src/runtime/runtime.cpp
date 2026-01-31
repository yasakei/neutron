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

/*
 * NativeFn - Native Function Wrapper
 * 
 * Provides a unified interface for calling C++ functions from Neutron code.
 * Supports two modes:
 * 1. Simple functions: Take only arguments, return a value
 * 2. VM-aware functions: Also receive VM reference for advanced operations
 * 
 * This allows native extensions to interact with the VM state when needed
 * (e.g., for memory allocation, accessing globals) while keeping simple
 * functions lightweight and portable.
 */

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

/*
 * BoundMethod - Method-Receiver Binding
 * 
 * Represents a method bound to a specific instance (the receiver).
 * When a method is accessed on an object, it becomes a bound method
 * that remembers both the method implementation and the instance.
 * 
 * Example in Neutron:
 *   obj.method()  // Creates BoundMethod(obj, method_function)
 * 
 * This ensures 'this' (or 'self') refers to the correct object when
 * the method is called, even if passed around as a first-class value.
 */

BoundMethod::BoundMethod(Value receiver, Function* method) 
    : receiver(receiver), method(method) {}

int BoundMethod::arity() {
    return method->arity();
}

Value BoundMethod::call(VM& vm, std::vector<Value> arguments) {
    /*
     * Delegation to underlying method:
     * The VM's callValue handles BoundMethod specially by setting up
     * the receiver in the call frame. This fallback exists for edge cases
     * but shouldn't typically be reached during normal execution.
     */
    return method->call(vm, arguments);
}

std::string BoundMethod::toString() const {
    return "<bound method>";
}

/*
 * Class - Object-Oriented Programming Support
 * 
 * Represents a class definition in Neutron, serving as:
 * 1. A blueprint for creating instances
 * 2. A namespace for class-level methods and fields
 * 3. A callable that constructs new instances
 * 
 * Classes can have an associated environment (class_env) that stores
 * class-level variables and methods, supporting inheritance and
 * method resolution.
 */

Class::Class(const std::string& name)
    : name(name), class_env(nullptr) {}

Class::Class(const std::string& name, std::shared_ptr<Environment> class_env)
    : name(name), class_env(class_env) {}

int Class::arity() {
    /*
     * Default constructor arity:
     * Currently returns 0 (no-arg constructor). In a full implementation,
     * this would be determined by the constructor method's arity if defined.
     */
    return 0;
}

Value Class::call(VM& vm, std::vector<Value> arguments) {
    (void)arguments;
    /*
     * Instance creation:
     * Calling a class constructs a new instance using the VM's allocator.
     * The allocator ensures proper garbage collection tracking.
     */
    Instance* instance = vm.allocate<Instance>(this);
    return Value(instance);
}

std::string Class::toString() const {
    return name;
}

/*
 * Instance - Object Instance with Optimized Field Storage
 * 
 * Implements a hybrid storage strategy for instance fields:
 * 
 * Inline Storage (Fast Path):
 * - First 8 fields stored directly in the object
 * - Zero overhead for field access
 * - Cache-friendly memory layout
 * - Ideal for small objects (most common case)
 * 
 * Overflow Storage (Flexible Path):
 * - Additional fields stored in a hash map
 * - Used when object has more than 8 fields
 * - Trades some performance for unlimited fields
 * - Allocated on-demand to save memory
 * 
 * This design optimizes for the common case (few fields) while
 * gracefully handling objects with many fields.
 */

Instance::Instance(Class* klass)
    : klass(klass), inlineCount(0), overflowFields(nullptr) {
}

Instance::~Instance() {
    delete overflowFields;
}

Value* Instance::getField(ObjString* key) {
    /*
     * Field Access - Two-tier Lookup Strategy
     * 
     * Fast Path (Inline Fields):
     * - Linear search through first 8 fields
     * - Very fast for small objects (common case)
     * - O(n) but n ≤ 8, so effectively constant time
     * - No memory allocations or hash computations
     * 
     * Slow Path (Overflow Map):
     * - Hash table lookup for fields beyond the first 8
     * - O(1) average case but with hash overhead
     * - Only used when inline storage is full
     * 
     * Returns nullptr if field doesn't exist.
     */
    for (uint8_t i = 0; i < inlineCount; ++i) {
        if (inlineFields[i].key == key) {
            return &inlineFields[i].value;
        }
    }
    
    if (overflowFields) {
        auto it = overflowFields->find(key);
        if (it != overflowFields->end()) {
            return &it->second;
        }
    }
    
    return nullptr;
}

void Instance::setField(ObjString* key, const Value& value) {
    /*
     * Field Assignment - Optimized Storage Allocation
     * 
     * Update Strategy (Check existing fields first):
     * 1. Search inline fields for existing key
     * 2. Search overflow map if it exists
     * 3. Update in-place if found (avoids allocation)
     * 
     * Insertion Strategy (For new fields):
     * 1. Try inline storage if not full (< 8 fields)
     * 2. Allocate overflow map if inline is full
     * 3. Insert into overflow map
     * 
     * This ensures:
     * - Existing fields are updated efficiently
     * - New fields use inline storage when possible
     * - Overflow is allocated lazily only when needed
     */
    for (uint8_t i = 0; i < inlineCount; ++i) {
        if (inlineFields[i].key == key) {
            inlineFields[i].value = value;
            return;
        }
    }
    
    if (overflowFields) {
        auto it = overflowFields->find(key);
        if (it != overflowFields->end()) {
            it->second = value;
            return;
        }
    }
    
    if (inlineCount < INLINE_FIELD_COUNT) {
        inlineFields[inlineCount].key = key;
        inlineFields[inlineCount].value = value;
        ++inlineCount;
        return;
    }
    
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
