#ifndef NEUTRON_PATH_NATIVE_H
#define NEUTRON_PATH_NATIVE_H

#include "vm.h"
#include "expr.h"

namespace neutron {
    void register_path_functions(VM& vm, std::shared_ptr<Environment> env);

extern "C" {
    void neutron_init_path_module(VM* vm);
}
}

#endif