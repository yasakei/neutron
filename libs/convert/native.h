#ifndef NEUTRON_CONVERT_NATIVE_H
#define NEUTRON_CONVERT_NATIVE_H

#include "vm.h"
#include "expr.h"
#include <vector>

namespace neutron {

    Value native_char_to_int(std::vector<Value> arguments);
    Value native_int_to_char(std::vector<Value> arguments);
    Value native_string_get_char_at(std::vector<Value> arguments);
    Value native_string_length(std::vector<Value> arguments);
    Value native_string_to_int(std::vector<Value> arguments);
    Value native_int_to_string(std::vector<Value> arguments);  // This is the 'str' function
    Value native_bin_to_int(std::vector<Value> arguments);
    Value native_int_to_bin(std::vector<Value> arguments);

    void register_convert_functions(std::shared_ptr<Environment> env);

extern "C" {
    void neutron_init_convert_module(VM* vm);
}

}

#endif