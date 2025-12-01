#ifndef NEUTRON_ARRAYS_NATIVE_H
#define NEUTRON_ARRAYS_NATIVE_H

#include "vm.h"
#include "expr.h"
#include <vector>

namespace neutron {

    // Core Array Functions
    Value native_arrays_create(std::vector<Value> arguments);
    Value native_arrays_length(std::vector<Value> arguments);
    Value native_arrays_push(std::vector<Value> arguments);
    Value native_arrays_pop(std::vector<Value> arguments);
    Value native_arrays_at(std::vector<Value> arguments);
    Value native_arrays_set(std::vector<Value> arguments);
    
    // Advanced Array Methods
    Value native_arrays_slice(std::vector<Value> arguments);
    Value native_arrays_join(std::vector<Value> arguments);
    Value native_arrays_reverse(std::vector<Value> arguments);
    Value native_arrays_sort(std::vector<Value> arguments);
    Value native_arrays_index_of(std::vector<Value> arguments);
    Value native_arrays_contains(std::vector<Value> arguments);
    Value native_arrays_remove(std::vector<Value> arguments);
    Value native_arrays_remove_at(std::vector<Value> arguments);
    Value native_arrays_clear(std::vector<Value> arguments);
    Value native_arrays_clone(std::vector<Value> arguments);
    Value native_arrays_to_string(std::vector<Value> arguments);
    
    // Functional Programming Methods
    Value native_arrays_map(std::vector<Value> arguments);
    Value native_arrays_filter(std::vector<Value> arguments);
    Value native_arrays_find(std::vector<Value> arguments);
    Value native_arrays_reduce(std::vector<Value> arguments);
    Value native_arrays_every(std::vector<Value> arguments);
    Value native_arrays_some(std::vector<Value> arguments);
    Value native_arrays_flat(std::vector<Value> arguments);
    Value native_arrays_flat_map(std::vector<Value> arguments);
    
    // Utility Methods
    Value native_arrays_fill(std::vector<Value> arguments);
    Value native_arrays_range(std::vector<Value> arguments);
    Value native_arrays_shuffle(std::vector<Value> arguments);

    void register_arrays_functions(std::shared_ptr<Environment> env);

extern "C" {
    void neutron_init_arrays_module(VM* vm);
}

}

#endif