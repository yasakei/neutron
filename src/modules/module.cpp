#include "modules/module.h"

namespace neutron {

Module::Module(const std::string& name, std::shared_ptr<Environment> env)
    : name(name), env(env) {}

Value Module::get(const std::string& name) {
    return env->get(name);
}

void Module::define(const std::string& name, const Value& value) {
    env->define(name, value);
}

}
