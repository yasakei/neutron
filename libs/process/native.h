/*
 * Neutron Programming Language - Process Module
 * Erlang-style lightweight processes with message passing
 */

#ifndef NEUTRON_PROCESS_NATIVE_H
#define NEUTRON_PROCESS_NATIVE_H

#include "core/vm.h"
#include "runtime/environment.h"

namespace neutron {

void register_process_functions(VM& vm, std::shared_ptr<Environment> env);

} // namespace neutron

extern "C" void neutron_init_process_module(neutron::VM* vm);

#endif // NEUTRON_PROCESS_NATIVE_H
