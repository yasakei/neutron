#pragma once

#include <string>
#include <unordered_map>
#include "types/object.h"
#include "types/value.h"
#include "types/obj_string.h"

namespace neutron {

class JsonObject : public Object {
public:
    JsonObject() { obj_type = ObjType::OBJ_JSON_OBJECT; }
    std::unordered_map<ObjString*, Value, ObjStringHash, ObjStringEqual> properties;
    std::string toString() const override;
};

}
