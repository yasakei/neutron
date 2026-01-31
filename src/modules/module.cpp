/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, for both open source and commercial purposes.
 * 
 * Conditions:
 * 
 * 1. The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 * 
 * 2. Attribution is appreciated but NOT required.
 *    Suggested (optional) credit:
 *    "Built using Neutron Programming Language (c) yasakei"
 * 
 * 3. The name "Neutron" and the name of the copyright holder may not be used
 *    to endorse or promote products derived from this Software without prior
 *    written permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
