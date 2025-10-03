#include "native.h"
#include "vm.h"
#include "environment.h"
#include "value.h"
#include "expr.h"
#include <iostream>

using namespace neutron;

static Value input(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("input() expects 1 argument.");
    }
    std::cout << args[0].toString();
    std::string line;
    std::getline(std::cin, line);
    return Value(line);
}

namespace neutron {
    void register_sys_functions(std::shared_ptr<Environment> env) {
        env->define("input", Value(new NativeFn(input, 1)));
    }
}

extern "C" void neutron_init_sys_module(VM* vm) {
    auto sys_env = std::make_shared<neutron::Environment>();
    neutron::register_sys_functions(sys_env);
    auto sys_module = new neutron::Module("sys", sys_env);
    vm->define_module("sys", sys_module);
}