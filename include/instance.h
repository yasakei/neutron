#pragma once

#include <string>
#include <unordered_map>
#include "object.h"
#include "class.h"
#include "value.h"

namespace neutron {

class Instance : public Object {
public:
    Instance(Class* klass);
    std::string toString() const override;
    
    Class* klass;
    std::unordered_map<std::string, Value> fields;
};

}
