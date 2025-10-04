#include "module_registry.h"
#include "vm.h"
#include <vector>

static std::vector<ModuleRegisterFunc>& get_module_registry() {
    static std::vector<ModuleRegisterFunc> registry;
    return registry;
}

void register_external_module(ModuleRegisterFunc func) {
    get_module_registry().push_back(func);
}

void run_external_module_initializers(neutron::VM* vm) {
    for (auto& func : get_module_registry()) {
        func(vm);
    }
}