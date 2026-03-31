/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 *
 * collections module — set, stack, queue
 *
 * Sets are stored as JsonObject with a special "__set__" marker.
 * Stacks and queues are thin wrappers around Array with restricted access.
 */
#include "native.h"
#include "vm.h"
#include "types/obj_string.h"
#include "types/json_object.h"
#include "types/array.h"
#include <stdexcept>
#include <string>

using namespace neutron;

// ── helpers ───────────────────────────────────────────────────────────────────

static bool valEq(const Value& a, const Value& b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case ValueType::NIL:     return true;
        case ValueType::BOOLEAN: return a.as.boolean == b.as.boolean;
        case ValueType::NUMBER:  return a.as.number  == b.as.number;
        case ValueType::OBJ_STRING: return a.asString()->chars == b.asString()->chars;
        default: return false;
    }
}

// ── SET ───────────────────────────────────────────────────────────────────────
// Represented as an Array with unique elements.

// set.new() → array (used as set)
Value set_new(VM& vm, std::vector<Value> args) {
    (void)args;
    return Value(vm.allocate<Array>());
}

// set.add(set, value) → bool (true if added, false if already present)
Value set_add(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("set.add(set, value) expects array and value.");
    auto* arr = args[0].as.array;
    for (size_t i = 0; i < arr->size(); i++)
        if (valEq(arr->at(i), args[1])) return Value(false);
    arr->push(args[1]);
    return Value(true);
}

// set.has(set, value) → bool
Value set_has(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("set.has(set, value) expects array and value.");
    auto* arr = args[0].as.array;
    for (size_t i = 0; i < arr->size(); i++)
        if (valEq(arr->at(i), args[1])) return Value(true);
    return Value(false);
}

// set.remove(set, value) → bool
Value set_remove(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("set.remove(set, value) expects array and value.");
    auto* arr = args[0].as.array;
    for (auto it = arr->elements.begin(); it != arr->elements.end(); ++it) {
        if (valEq(*it, args[1])) { arr->elements.erase(it); return Value(true); }
    }
    return Value(false);
}

// set.size(set) → number
Value set_size(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("set.size(set) expects array.");
    return Value(static_cast<double>(args[0].as.array->size()));
}

// set.to_array(set) → array (clone)
Value set_to_array(VM& vm, std::vector<Value> args) {
    if (args.size() != 1 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("set.to_array(set) expects array.");
    return Value(vm.allocate<Array>(args[0].as.array->elements));
}

// set.union(a, b) → new set
Value set_union(VM& vm, std::vector<Value> args) {
    if (args.size() != 2 || args[0].type != ValueType::ARRAY || args[1].type != ValueType::ARRAY)
        throw std::runtime_error("set.union(a, b) expects two arrays.");
    auto* result = vm.allocate<Array>();
    auto addUniq = [&](Array* src) {
        for (size_t i = 0; i < src->size(); i++) {
            bool found = false;
            for (size_t j = 0; j < result->size(); j++)
                if (valEq(result->at(j), src->at(i))) { found = true; break; }
            if (!found) result->push(src->at(i));
        }
    };
    addUniq(args[0].as.array);
    addUniq(args[1].as.array);
    return Value(result);
}

// set.intersection(a, b) → new set
Value set_intersection(VM& vm, std::vector<Value> args) {
    if (args.size() != 2 || args[0].type != ValueType::ARRAY || args[1].type != ValueType::ARRAY)
        throw std::runtime_error("set.intersection(a, b) expects two arrays.");
    auto* a = args[0].as.array;
    auto* b = args[1].as.array;
    auto* result = vm.allocate<Array>();
    for (size_t i = 0; i < a->size(); i++) {
        for (size_t j = 0; j < b->size(); j++) {
            if (valEq(a->at(i), b->at(j))) { result->push(a->at(i)); break; }
        }
    }
    return Value(result);
}

// set.difference(a, b) → elements in a not in b
Value set_difference(VM& vm, std::vector<Value> args) {
    if (args.size() != 2 || args[0].type != ValueType::ARRAY || args[1].type != ValueType::ARRAY)
        throw std::runtime_error("set.difference(a, b) expects two arrays.");
    auto* a = args[0].as.array;
    auto* b = args[1].as.array;
    auto* result = vm.allocate<Array>();
    for (size_t i = 0; i < a->size(); i++) {
        bool inB = false;
        for (size_t j = 0; j < b->size(); j++)
            if (valEq(a->at(i), b->at(j))) { inB = true; break; }
        if (!inB) result->push(a->at(i));
    }
    return Value(result);
}

// ── STACK ─────────────────────────────────────────────────────────────────────
// stack.new() → array
Value stack_new(VM& vm, std::vector<Value> args) {
    (void)args;
    return Value(vm.allocate<Array>());
}

// stack.push(stack, value)
Value stack_push(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("stack.push(stack, value) expects array and value.");
    args[0].as.array->push(args[1]);
    return Value();
}

// stack.pop(stack) → value
Value stack_pop(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("stack.pop(stack) expects array.");
    auto* arr = args[0].as.array;
    if (arr->size() == 0) throw std::runtime_error("stack.pop: stack is empty.");
    return arr->pop();
}

// stack.peek(stack) → value (no removal)
Value stack_peek(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("stack.peek(stack) expects array.");
    auto* arr = args[0].as.array;
    if (arr->size() == 0) throw std::runtime_error("stack.peek: stack is empty.");
    return arr->at(arr->size() - 1);
}

// stack.size(stack) → number
Value stack_size(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("stack.size(stack) expects array.");
    return Value(static_cast<double>(args[0].as.array->size()));
}

// stack.is_empty(stack) → bool
Value stack_is_empty(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("stack.is_empty(stack) expects array.");
    return Value(args[0].as.array->size() == 0);
}

// ── QUEUE ─────────────────────────────────────────────────────────────────────
// queue.new() → array
Value queue_new(VM& vm, std::vector<Value> args) {
    (void)args;
    return Value(vm.allocate<Array>());
}

// queue.enqueue(queue, value)
Value queue_enqueue(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("queue.enqueue(queue, value) expects array and value.");
    args[0].as.array->push(args[1]);
    return Value();
}

// queue.dequeue(queue) → value (removes from front)
Value queue_dequeue(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("queue.dequeue(queue) expects array.");
    auto* arr = args[0].as.array;
    if (arr->size() == 0) throw std::runtime_error("queue.dequeue: queue is empty.");
    Value front = arr->at(0);
    arr->elements.erase(arr->elements.begin());
    return front;
}

// queue.peek(queue) → value (front, no removal)
Value queue_peek(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("queue.peek(queue) expects array.");
    auto* arr = args[0].as.array;
    if (arr->size() == 0) throw std::runtime_error("queue.peek: queue is empty.");
    return arr->at(0);
}

// queue.size / queue.is_empty — reuse stack versions (same logic)
Value queue_size(VM& vm, std::vector<Value> args) { return stack_size(vm, args); }
Value queue_is_empty(VM& vm, std::vector<Value> args) { return stack_is_empty(vm, args); }

// ── registration ──────────────────────────────────────────────────────────────
namespace neutron {
    void register_collections_functions(VM& vm, std::shared_ptr<Environment> env) {
        // Set
        env->define("set_new",          Value(vm.allocate<NativeFn>(set_new,          0,  true)));
        env->define("set_add",          Value(vm.allocate<NativeFn>(set_add,          2,  true)));
        env->define("set_has",          Value(vm.allocate<NativeFn>(set_has,          2,  true)));
        env->define("set_remove",       Value(vm.allocate<NativeFn>(set_remove,       2,  true)));
        env->define("set_size",         Value(vm.allocate<NativeFn>(set_size,         1,  true)));
        env->define("set_to_array",     Value(vm.allocate<NativeFn>(set_to_array,     1,  true)));
        env->define("set_union",        Value(vm.allocate<NativeFn>(set_union,        2,  true)));
        env->define("set_intersection", Value(vm.allocate<NativeFn>(set_intersection, 2,  true)));
        env->define("set_difference",   Value(vm.allocate<NativeFn>(set_difference,   2,  true)));
        // Stack
        env->define("stack_new",        Value(vm.allocate<NativeFn>(stack_new,        0,  true)));
        env->define("stack_push",       Value(vm.allocate<NativeFn>(stack_push,       2,  true)));
        env->define("stack_pop",        Value(vm.allocate<NativeFn>(stack_pop,        1,  true)));
        env->define("stack_peek",       Value(vm.allocate<NativeFn>(stack_peek,       1,  true)));
        env->define("stack_size",       Value(vm.allocate<NativeFn>(stack_size,       1,  true)));
        env->define("stack_is_empty",   Value(vm.allocate<NativeFn>(stack_is_empty,   1,  true)));
        // Queue
        env->define("queue_new",        Value(vm.allocate<NativeFn>(queue_new,        0,  true)));
        env->define("queue_enqueue",    Value(vm.allocate<NativeFn>(queue_enqueue,    2,  true)));
        env->define("queue_dequeue",    Value(vm.allocate<NativeFn>(queue_dequeue,    1,  true)));
        env->define("queue_peek",       Value(vm.allocate<NativeFn>(queue_peek,       1,  true)));
        env->define("queue_size",       Value(vm.allocate<NativeFn>(queue_size,       1,  true)));
        env->define("queue_is_empty",   Value(vm.allocate<NativeFn>(queue_is_empty,   1,  true)));
    }
}

extern "C" void neutron_init_collections_module(neutron::VM* vm) {
    auto env = std::make_shared<neutron::Environment>();
    neutron::register_collections_functions(*vm, env);
    auto mod = vm->allocate<neutron::Module>("collections", env);
    vm->define_module("collections", mod);
}
