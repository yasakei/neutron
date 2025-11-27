#pragma once

#include "types/object.h"
#include <string>

namespace neutron {

class BoundStringMethod : public Object {
public:
    std::string stringValue;
    std::string methodName;
    
    BoundStringMethod(const std::string& str, const std::string& method) 
        : stringValue(str), methodName(method) {}
    
    std::string toString() const override {
        return "<bound string method '" + methodName + "'>";
    }
};

}
