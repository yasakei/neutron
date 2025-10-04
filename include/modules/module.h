#pragma once

#include <string>
#include <memory>
#include <vector>
#include "types/value.h"
#include "runtime/environment.h"
#include "stmt.h"

namespace neutron {

class Module {
public:
    std::string name;
    std::shared_ptr<Environment> env;

    Module(const std::string& name, std::shared_ptr<Environment> env);

    Value get(const std::string& name);

    void define(const std::string& name, const Value& value);
};

}
