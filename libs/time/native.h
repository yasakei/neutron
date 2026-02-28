#ifndef NEUTRON_TIME_NATIVE_H
#define NEUTRON_TIME_NATIVE_H

#include "vm.h"
#include "expr.h"

namespace neutron {
    void register_time_functions(VM& vm, std::shared_ptr<Environment> env);

extern "C" {
    void neutron_init_time_module(VM* vm);
}
}

#endif