#pragma once

#include "vm.h"
#include "runtime/environment.h"

namespace neutron {
    void register_sys_functions(std::shared_ptr<Environment> env);
}

extern "C" void neutron_init_sys_module(neutron::VM* vm);
