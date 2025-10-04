#pragma once

#include "types/callable.h"
#include "types/function.h"

namespace neutron {

class BoundMethod : public Callable {
public:
    BoundMethod(Value receiver, Function* method);
    int arity() override;
    Value call(VM& vm, std::vector<Value> arguments) override;
    std::string toString() override;
    
    Value receiver;
    Function* method;
};

}
