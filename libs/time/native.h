#ifndef NEUTRON_TIME_NATIVE_H
#define NEUTRON_TIME_NATIVE_H

#include "vm.h"

namespace neutron {
    void register_time_functions(std::shared_ptr<Environment> env);
}

#endif