#pragma once
#include <functional>

namespace neutron {
    class VM;
}

using ModuleRegisterFunc = std::function<void(neutron::VM*)>;

void register_external_module(ModuleRegisterFunc func);
void run_external_module_initializers(neutron::VM* vm);