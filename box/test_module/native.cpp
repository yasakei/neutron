#include <neutron.h>

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

// Module initialization function
extern "C" void initModule(NeutronVM* vm) {
    // Define the module name and function
    neutron_define_native(vm, "addTwoNumbers", addTwoNumbers, 2);
}