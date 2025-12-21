#pragma once

#include <string>
#include <unordered_map>
#include "types/object.h"
#include "types/value.h"

namespace neutron {

class JsonObject : public Object {
public:
    std::unordered_map<std::string, Value> properties;
    std::string toString() const override;
};

}
