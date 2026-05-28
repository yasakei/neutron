#pragma once

#include "types/callable.h"
#include "types/function.h"
#include <vector>

namespace neutron {

class UpValue;

class ObjClosure : public Callable {
    friend class VM;
public:
    ObjClosure(Function* fn);
    ~ObjClosure();

    int arity() override;
    Value call(VM& vm, std::vector<Value> arguments) override;
    std::string toString() const override;
    void mark() override;

    Function* function;
    std::vector<UpValue*> upvalues;
};

}
