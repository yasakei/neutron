#pragma once

#include <string>
#include <memory>
#include <vector>
#include "types/value.h"
#include "types/object.h"
#include "runtime/environment.h"
#include "stmt.h"

namespace neutron {

class Module : public Object {
public:
    std::string name;
    std::shared_ptr<Environment> env;
    void* handle;

    Module(const std::string& name, std::shared_ptr<Environment> env, void* handle = nullptr);
    ~Module() override;

    Value get(const std::string& name);

    void define(const std::string& name, const Value& value);
    
    std::string toString() const override;
    void mark() override;
};

}
