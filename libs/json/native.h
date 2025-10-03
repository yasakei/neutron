#ifndef NEUTRON_JSON_NATIVE_H
#define NEUTRON_JSON_NATIVE_H

#include "vm.h"
#include "expr.h"

namespace neutron {
    void register_json_functions(std::shared_ptr<Environment> env);

extern "C" {
    void neutron_init_json_module(VM* vm);
}
}

#endif