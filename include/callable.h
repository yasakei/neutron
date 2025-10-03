#pragma once

#include <string>
#include <vector>
#include "value.h"

namespace neutron {

class VM;

class Callable {
public:
    virtual ~Callable() = default;
    virtual int arity() = 0;
    virtual Value call(VM& vm, std::vector<Value> arguments) = 0;
    virtual std::string toString() = 0;
    virtual bool isCNativeFn() const { return false; }
};

}
