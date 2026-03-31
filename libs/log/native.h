#ifndef NEUTRON_LOG_NATIVE_H
#define NEUTRON_LOG_NATIVE_H

#include "vm.h"
#include <vector>

namespace neutron {
    void register_log_functions(VM& vm, std::shared_ptr<Environment> env);
    extern "C" void neutron_init_log_module(VM* vm);
}

#endif
