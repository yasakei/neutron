#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "value.h"

namespace neutron {

class Environment {
public:
    std::shared_ptr<Environment> enclosing;
    std::unordered_map<std::string, Value> values;

    Environment();
    Environment(std::shared_ptr<Environment> enclosing);
    
    void define(const std::string& name, const Value& value);
    Value get(const std::string& name);
    void assign(const std::string& name, const Value& value);
};

}
