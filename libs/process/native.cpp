/*
 * Neutron Programming Language - Process Module Implementation
 */

#include "process/native.h"
#include "runtime/process.h"
#include "types/native_fn.h"
#include "modules/module.h"

namespace neutron {

void register_process_functions(VM& vm, std::shared_ptr<Environment> env) {
    ProcessVM::registerFunctions(vm, env);
}

} // namespace neutron

extern "C" void neutron_init_process_module(neutron::VM* vm) {
    auto env = std::make_shared<neutron::Environment>();
    neutron::register_process_functions(*vm, env);
    
    auto module = vm->allocate<neutron::Module>("process", env);
    vm->define_module("process", module);
}
