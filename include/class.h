#pragma once

#include <string>
#include <vector>
#include <memory>
#include "callable.h"
#include "environment.h"

namespace neutron {

class Class : public Callable {
public:
    Class(const std::string& name);
    Class(const std::string& name, std::shared_ptr<Environment> class_env);
    int arity() override;
    Value call(VM& vm, std::vector<Value> arguments) override;
    std::string toString() override;
    
    std::string name;
    std::shared_ptr<Environment> class_env;  // Store the environment for this class
    std::unordered_map<std::string, Value> methods;  // Store method definitions
};

}
