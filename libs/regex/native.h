#pragma once

#include "vm.h"
#include "runtime/environment.h"

namespace neutron {

void register_regex_functions(std::shared_ptr<Environment> env);

} // namespace neutron

extern "C" void neutron_init_regex_module(neutron::VM* vm);
