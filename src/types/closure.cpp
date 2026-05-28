#include "types/closure.h"
#include "types/upvalue.h"
#include "core/vm.h"

namespace neutron {

ObjClosure::ObjClosure(Function* fn)
    : function(fn) {
    obj_type = ObjType::OBJ_CLOSURE;
}

ObjClosure::~ObjClosure() {}

int ObjClosure::arity() {
    return function->arity_val;
}

Value ObjClosure::call(VM& vm, std::vector<Value> arguments) {
    return function->call(vm, std::move(arguments));
}

std::string ObjClosure::toString() const {
    return function->toString();
}

void ObjClosure::mark() {
    Object::mark();
    if (function) function->mark();
    for (auto* uv : upvalues) {
        if (uv) uv->mark();
    }
}

}
