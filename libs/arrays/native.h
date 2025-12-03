#ifndef NEUTRON_ARRAYS_NATIVE_H
#define NEUTRON_ARRAYS_NATIVE_H

#include "vm.h"
#include "expr.h"
#include <vector>

namespace neutron {

    // Core Array Functions
    Value native_arrays_create(VM& vm, std::vector<Value> arguments);
    Value native_arrays_length(VM& vm, std::vector<Value> arguments);
    Value native_arrays_push(VM& vm, std::vector<Value> arguments);
    Value native_arrays_pop(VM& vm, std::vector<Value> arguments);
    Value native_arrays_at(VM& vm, std::vector<Value> arguments);
    Value native_arrays_set(VM& vm, std::vector<Value> arguments);
    
    // Advanced Array Methods
    Value native_arrays_slice(VM& vm, std::vector<Value> arguments);
    Value native_arrays_join(VM& vm, std::vector<Value> arguments);
    Value native_arrays_reverse(VM& vm, std::vector<Value> arguments);
    Value native_arrays_sort(VM& vm, std::vector<Value> arguments);
    Value native_arrays_index_of(VM& vm, std::vector<Value> arguments);
    Value native_arrays_contains(VM& vm, std::vector<Value> arguments);
    Value native_arrays_remove(VM& vm, std::vector<Value> arguments);
    Value native_arrays_remove_at(VM& vm, std::vector<Value> arguments);
    Value native_arrays_clear(VM& vm, std::vector<Value> arguments);
    Value native_arrays_clone(VM& vm, std::vector<Value> arguments);
    Value native_arrays_to_string(VM& vm, std::vector<Value> arguments);
    
    // Functional Programming Methods
    Value native_arrays_map(VM& vm, std::vector<Value> arguments);
    Value native_arrays_filter(VM& vm, std::vector<Value> arguments);
    Value native_arrays_find(VM& vm, std::vector<Value> arguments);
    Value native_arrays_reduce(VM& vm, std::vector<Value> arguments);
    Value native_arrays_every(VM& vm, std::vector<Value> arguments);
    Value native_arrays_some(VM& vm, std::vector<Value> arguments);
    Value native_arrays_flat(VM& vm, std::vector<Value> arguments);
    Value native_arrays_flat_map(VM& vm, std::vector<Value> arguments);
    
    // Utility Methods
    Value native_arrays_fill(VM& vm, std::vector<Value> arguments);
    Value native_arrays_range(VM& vm, std::vector<Value> arguments);
    Value native_arrays_shuffle(VM& vm, std::vector<Value> arguments);


    void register_arrays_functions(VM& vm, std::shared_ptr<Environment> env);

extern "C" {
    void neutron_init_arrays_module(VM* vm);
}

}

#endif