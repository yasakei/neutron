#ifndef NEUTRON_CONVERT_NATIVE_H
#define NEUTRON_CONVERT_NATIVE_H

#include "vm.h"

namespace neutron {
    void register_convert_functions(std::shared_ptr<Environment> env);
}

#endif