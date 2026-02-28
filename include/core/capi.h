#ifndef NEUTRON_CAPI_H
#define NEUTRON_CAPI_H

#include "neutron.h"

// A C++ class that wraps a C native function
class CNativeFn : public neutron::Callable {
public:
    CNativeFn(NeutronNativeFn function, int arity)
        : function(function), _arity(arity) {
    }

    ~CNativeFn() {
    }

    int arity() override { return _arity; }

    neutron::Value call(neutron::VM& vm, std::vector<neutron::Value> args) override;

    std::string toString() const override { return "<native fn>"; }
    bool isCNativeFn() const override { return true; }

private:
    NeutronNativeFn function;
    int _arity;
};

#endif // NEUTRON_CAPI_H