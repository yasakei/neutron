#pragma once

#include "types/value.h"
#include <vector>

namespace neutron {

// Native runtime functions
Value native_say(std::vector<Value> arguments);
Value native_array_new(std::vector<Value> arguments);
Value native_array_push(std::vector<Value> arguments);
Value native_array_pop(std::vector<Value> arguments);
Value native_array_length(std::vector<Value> arguments);
Value native_array_at(std::vector<Value> arguments);
Value native_array_set(std::vector<Value> arguments);
Value native_string_length(std::vector<Value> arguments);
Value native_type_of(std::vector<Value> arguments);

}
