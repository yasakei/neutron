#ifndef NEUTRON_SYS_NATIVE_H
#define NEUTRON_SYS_NATIVE_H

#include "vm.h"

namespace neutron {
    void register_sys_functions(std::shared_ptr<Environment> env);
}

#endif