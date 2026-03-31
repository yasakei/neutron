#ifndef NEUTRON_STRINGS_NATIVE_H
#define NEUTRON_STRINGS_NATIVE_H

#include "vm.h"
#include <vector>

namespace neutron {
    void register_strings_functions(VM& vm, std::shared_ptr<Environment> env);
    extern "C" void neutron_init_strings_module(VM* vm);
}

#endif
