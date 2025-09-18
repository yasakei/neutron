#include "native.h"
#include "neutron.h"
#include <iostream>

static double fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

extern "C" NeutronValue* native_fib(NeutronVM* vm, int arg_count, NeutronValue** args) {
    if (arg_count != 1 || neutron_get_type(args[0]) != NEUTRON_NUMBER) {
        return neutron_new_nil();
    }
    int n = (int)neutron_get_number(args[0]);
    double result = fib(n);
    return neutron_new_number(result);
}

extern "C" void neutron_module_init(neutron::VM* vm) {
    neutron_define_native(reinterpret_cast<NeutronVM*>(vm), "fib", native_fib, 1);
}
