#pragma once

#include "types/object.h"
#include "types/string_method_registry.h"
#include <string>

namespace neutron {

class BoundStringMethod : public Object {
public:
    std::string stringValue;
    std::string methodName;
    StringMethodCategory category;
    
    BoundStringMethod(const std::string& str, const std::string& method) 
        : stringValue(str), methodName(method) {
        obj_type = ObjType::OBJ_BOUND_STRING_METHOD;
        // Get category from registry
        category = StringMethodRegistry::getInstance().getMethodCategory(method);
    }
    
    BoundStringMethod(const std::string& str, const std::string& method, StringMethodCategory cat)
        : stringValue(str), methodName(method), category(cat) {
        obj_type = ObjType::OBJ_BOUND_STRING_METHOD;
    }
    
    std::string toString() const override {
        return "<bound string method '" + methodName + "'>";
    }
    
    StringMethodCategory getCategory() const {
        return category;
    }
};

}
