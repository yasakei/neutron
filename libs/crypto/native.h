#ifndef NEUTRON_CRYPTO_NATIVE_H
#define NEUTRON_CRYPTO_NATIVE_H

#include "vm.h"
#include "expr.h"

namespace neutron {
    void register_crypto_functions(VM& vm, std::shared_ptr<Environment> env);

extern "C" {
    void neutron_init_crypto_module(VM* vm);
}
}

#endif