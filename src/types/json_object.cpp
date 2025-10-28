#include "types/json_object.h"
#include <sstream>
#include <string>

namespace neutron {

std::string JsonObject::toString() const {
    if (properties.empty()) {
        return "{}";
    }
    
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& pair : properties) {
        if (!first) {
            oss << ", ";
        }
        oss << pair.first << ":" << pair.second.toString();
        first = false;
    }
    oss << "}";
    return oss.str();
}

}
