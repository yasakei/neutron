#include <neutron.h>

// Simple function that adds two numbers
Value addTwoNumbers(VM* vm, int argCount, Value* args) {
    if (argCount != 2) {
        vm->runtimeError("addTwoNumbers() takes exactly 2 arguments");
        return NULL_VAL;
    }
    
    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
        vm->runtimeError("Arguments must be numbers");
        return NULL_VAL;
    }
    
    double a = AS_NUMBER(args[0]);
    double b = AS_NUMBER(args[1]);
    
    return NUMBER_VAL(a + b);
}

// Module initialization function
extern "C" void initModule(VM* vm) {
    // Define the module name
    vm->defineNative("test_module", "addTwoNumbers", addTwoNumbers);
}