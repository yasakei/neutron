#include "modules/module.h"
#include "cross-platfrom/dlfcn_compat.h"

namespace neutron {

Module::Module(const std::string& name, std::shared_ptr<Environment> env, void* handle)
    : name(name), env(env), handle(handle) {}

Module::~Module() {
    // NOTE: We intentionally do NOT call dlclose here.
    // On Linux, calling dlclose can cause "free(): invalid pointer" crashes
    // if the shared library allocated memory that's still referenced.
    // The OS will clean up when the process exits anyway.
    // This is a known issue with dynamic loading on Linux.
    // 
    // if (handle) {
    //     dlclose(handle);
    // }
    (void)handle; // Suppress unused warning
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
