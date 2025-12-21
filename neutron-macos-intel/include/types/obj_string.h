#pragma once

#include "object.h"
#include <string>

namespace neutron {

class ObjString : public Object {
public:
    std::string chars;

    ObjString(const std::string& chars);
    virtual ~ObjString() = default;

    std::string toString() const override;
};

}
