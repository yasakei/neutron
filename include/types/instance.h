#pragma once

#include <string>
#include <unordered_map>
#include "types/object.h"
#include "types/class.h"
#include "types/value.h"

namespace neutron {

class Instance : public Object {
public:
    Instance(Class* klass);
    std::string toString() const override;
    
    Class* klass;
    std::unordered_map<std::string, Value> fields;
};

}
