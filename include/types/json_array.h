#pragma once

#include <string>
#include <vector>
#include "types/object.h"
#include "types/value.h"

namespace neutron {

class JsonArray : public Object {
public:
    std::vector<Value> elements;
    std::string toString() const override;
};

}
