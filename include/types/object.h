#pragma once

#include <string>

namespace neutron {

class Object {
public:
    bool is_marked = false;
    virtual ~Object() = default;
    virtual std::string toString() const = 0;
    virtual void mark() { is_marked = true; }
    virtual void sweep() {} // Default implementation, can be overridden
};

}
