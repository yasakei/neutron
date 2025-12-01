#include "modules/module.h"
#include "cross-platfrom/dlfcn_compat.h"

namespace neutron {

Module::Module(const std::string& name, std::shared_ptr<Environment> env, void* handle)
    : name(name), env(env), handle(handle) {}

Module::~Module() {
    if (handle) {
        dlclose(handle);
    }
}

Value Module::get(const std::string& name) {
    return env->get(name);
}

void Module::define(const std::string& name, const Value& value) {
    env->define(name, value);
}

std::string Module::toString() const {
    return "<module " + name + ">";
}

void Module::mark() {
    Object::mark();
}

}
