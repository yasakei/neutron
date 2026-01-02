#pragma once

#include <string>
#include <vector>
#include <memory>
#include "types/callable.h"
#include "runtime/environment.h"
#include "types/obj_string.h"

namespace neutron {

class Class : public Callable {
public:
    Class(const std::string& name);
    Class(const std::string& name, std::shared_ptr<Environment> class_env);
    int arity() override;
    Value call(VM& vm, std::vector<Value> arguments) override;
    std::string toString() const override;
    
    std::string name;
    std::shared_ptr<Environment> class_env;  // Store the environment for this class
    std::unordered_map<ObjString*, Value> methods;  // Store method definitions
    class Function* initializer = nullptr; // Cached initializer
};

}
