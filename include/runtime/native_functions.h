#pragma once

#include "types/value.h"
#include <vector>

namespace neutron {

// Native runtime functions
Value native_say(std::vector<Value> arguments);

Value native_string_length(std::vector<Value> arguments);
Value native_type_of(std::vector<Value> arguments);

}
