#include "types/upvalue.h"

namespace neutron {

UpValue::UpValue(Value* slot)
    : location(slot), next(nullptr) {
    closed = Value();
    obj_type = ObjType::OBJ_UPVALUE;
}

std::string UpValue::toString() const {
    return "<upvalue>";
}

void UpValue::mark() {
    Object::mark();
    // Note: closed values are marked by the VM's GC mark phase
    // through the frames and openUpvalues tracking
}

}
