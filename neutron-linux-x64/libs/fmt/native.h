#ifndef NEUTRON_FMT_NATIVE_H
#define NEUTRON_FMT_NATIVE_H

#include "vm.h"
#include "expr.h"
#include <vector>

namespace neutron {

    // Type conversion functions with dynamic type checking
    Value native_fmt_to_int(std::vector<Value> arguments);   // Dynamic int conversion
    Value native_fmt_to_str(std::vector<Value> arguments);   // Dynamic string conversion  
    Value native_fmt_to_bin(std::vector<Value> arguments);   // Dynamic binary conversion
    Value native_fmt_to_float(std::vector<Value> arguments); // Dynamic float conversion
    Value native_fmt_type(std::vector<Value> arguments);     // Type detection function

    void register_fmt_functions(std::shared_ptr<Environment> env);

extern "C" {
    void neutron_init_fmt_module(VM* vm);
}

}

#endif