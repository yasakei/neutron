#pragma once

#include "types/callable.h"
#include <functional>
#include <vector>

namespace neutron {

class NativeFn : public Callable {
public:
    using NativeFnPtr = std::function<Value(std::vector<Value>)>;
    using NativeFnPtrWithVM = std::function<Value(VM&, std::vector<Value>)>;

    NativeFn(NativeFnPtr function, int arity);
    NativeFn(NativeFnPtrWithVM function, int arity, bool needsVM);
    int arity() override;
    Value call(VM& vm, std::vector<Value> arguments) override;
    std::string toString() const override;

private:
    NativeFnPtr function;
    NativeFnPtrWithVM functionWithVM;
    int _arity;
    bool _needsVM;
};

}
