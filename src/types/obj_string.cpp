#include "types/obj_string.h"

namespace neutron {

ObjString::ObjString(const std::string& chars) : chars(chars) {}

std::string ObjString::toString() const {
    return chars;
}

}
