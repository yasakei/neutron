#pragma once

#include <string>
#include <unordered_map>
#include "types/object.h"
#include "types/value.h"
#include "types/obj_string.h"

namespace neutron {

class JsonObject : public Object {
public:
    std::unordered_map<ObjString*, Value> properties;
    std::string toString() const override;
};

}
