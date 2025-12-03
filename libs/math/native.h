#ifndef NEUTRON_LIBS_MATH_NATIVE_H
#define NEUTRON_LIBS_MATH_NATIVE_H

#include "vm.h"
#include "expr.h"

namespace neutron {

void register_math_functions(VM& vm, std::shared_ptr<Environment> env);

extern "C" {
    void neutron_init_math_module(VM* vm);
}

}

#endif // NEUTRON_LIBS_MATH_NATIVE_H
