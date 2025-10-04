#pragma once

#include "types/object.h"
#include "types/array.h"
#include <string>

namespace neutron {

class BoundArrayMethod : public Object {
public:
    Array* array;
    std::string methodName;
    
    BoundArrayMethod(Array* arr, const std::string& method) 
        : array(arr), methodName(method) {}
    
    std::string toString() const override {
        return "<bound array method '" + methodName + "'>";
    }
    
    void mark() override {
        is_marked = true;
        if (array) {
            array->mark();
        }
    }
};

}
