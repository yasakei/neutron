#pragma once

#include <string>
#include <vector>
#include "types/object.h"
#include "types/value.h"

namespace neutron {

class JsonArray : public Object {
public:
    JsonArray() { obj_type = ObjType::OBJ_JSON_ARRAY; }
    std::vector<Value> elements;
    std::string toString() const override;
};

}
