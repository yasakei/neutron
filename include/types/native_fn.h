#pragma once

#include "types/callable.h"
#include <functional>
#include <vector>

namespace neutron {

class NativeFn : public Callable {
public:
    using NativeFnPtr = std::function<Value(std::vector<Value>)>;

    NativeFn(NativeFnPtr function, int arity);
    int arity() override;
    Value call(VM& vm, std::vector<Value> arguments) override;
    std::string toString() override;

private:
    NativeFnPtr function;
    int _arity;
};

}
