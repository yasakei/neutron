#pragma once

#include "types/object.h"
#include "types/value.h"

namespace neutron {

class UpValue : public Object {
public:
    Value* location;
    Value closed;
    UpValue* next;

    UpValue(Value* slot);
    std::string toString() const override;
    void mark() override;
};

}
