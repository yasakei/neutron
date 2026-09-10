#ifndef NEUTRON_CORO_NATIVE_H
#define NEUTRON_CORO_NATIVE_H

#include "vm.h"
#include <vector>

namespace neutron {
    void register_coro_functions(VM& vm, std::shared_ptr<Environment> env);
    extern "C" void neutron_init_coro_module(VM* vm);
}

#endif
