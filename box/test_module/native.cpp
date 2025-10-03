#include <neutron.h>
#include "module_registry.h"
#include "vm.h"
#include "capi.h"
#include <functional>

// Simple function that adds two numbers
NeutronValue* addTwoNumbers(NeutronVM* vm, int argCount, NeutronValue** args) {
    if (argCount != 2) {
        // In a real implementation, this would call a proper error reporting function
        return neutron_new_number(0.0); // Return a default value on error
    }
    
    if (!neutron_is_number(args[0]) || !neutron_is_number(args[1])) {
        // In a real implementation, this would call a proper error reporting function
        return neutron_new_number(0.0); // Return a default value on error
    }
    
    double a = neutron_get_number(args[0]);
    double b = neutron_get_number(args[1]);
    
    return neutron_new_number(a + b);
}

// Module initialization function - for dynamic loading
extern "C" void neutron_module_init(NeutronVM* vm) {
    // This function is used for dynamic loading
    neutron_define_native(vm, "addTwoNumbers", addTwoNumbers, 2);
}

// Static module initialization function - for binary embedding
void static_test_module_init(neutron::VM* vm) {
    // Create a new environment for the module
    auto env = std::make_shared<neutron::Environment>();
    
    // Create the native function wrapper
    CNativeFn* fn = new CNativeFn(addTwoNumbers, 2);
    
    // Add the function to the module's environment
    env->define("addTwoNumbers", neutron::Value(fn));
    
    // Create and register the module
    neutron::Module* module = new neutron::Module("test_module", env);
    vm->define_module("test_module", module);
}

// Static registration using constructor attribute
__attribute__((constructor)) void register_test_module() {
    register_external_module([](neutron::VM* vm) {
        static_test_module_init(vm);
    });
}