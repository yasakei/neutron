#ifndef NEUTRON_COLLECTIONS_NATIVE_H
#define NEUTRON_COLLECTIONS_NATIVE_H

#include "vm.h"
#include <vector>

namespace neutron {
    void register_collections_functions(VM& vm, std::shared_ptr<Environment> env);
    extern "C" void neutron_init_collections_module(VM* vm);
}

#endif
