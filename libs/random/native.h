#ifndef NEUTRON_RANDOM_NATIVE_H
#define NEUTRON_RANDOM_NATIVE_H

#include "vm.h"
#include "expr.h"

namespace neutron {
    void register_random_functions(VM& vm, std::shared_ptr<Environment> env);

extern "C" {
    void neutron_init_random_module(VM* vm);
}
}

#endif