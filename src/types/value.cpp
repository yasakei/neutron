#include "types/value.h"
#include "types/array.h"
#include "types/object.h"
#include "types/obj_string.h"
#include "types/callable.h"
#include "modules/module.h"
#include "types/class.h"
#include "types/instance.h"
#include "types/buffer.h"
#include <sstream>

namespace neutron {

Value::Value() : type(ValueType::NIL) { as.object = nullptr; }

Value::Value(std::nullptr_t) : type(ValueType::NIL) { as.object = nullptr; }

Value::Value(bool value) : type(ValueType::BOOLEAN) { as.boolean = value; }

Value::Value(double value) : type(ValueType::NUMBER) { as.number = value; }

Value::Value(ObjString* string) : type(ValueType::OBJ_STRING) { as.obj_string = string; }

Value::Value(const std::string& value) : type(ValueType::OBJ_STRING) { as.obj_string = new ObjString(value); }

Value::Value(Array* array) : type(ValueType::ARRAY) { as.array = array; }

Value::Value(Object* object) : type(ValueType::OBJECT) { as.object = object; }

Value::Value(Callable* callable) : type(ValueType::CALLABLE) { as.callable = callable; }

Value::Value(Module* module) : type(ValueType::MODULE) { as.module = module; }

Value::Value(Class* klass) : type(ValueType::CLASS) { as.klass = klass; }

Value::Value(Instance* instance) : type(ValueType::INSTANCE) { as.instance = instance; }

Value::Value(Buffer* buffer) : type(ValueType::BUFFER) { as.buffer = buffer; }

std::string Value::toString() const {
    switch (type) {
        case ValueType::NIL:
            return "nil";
        case ValueType::BOOLEAN:
            return as.boolean ? "true" : "false";
        case ValueType::NUMBER: {
            // Convert double to string, removing trailing zeros
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%.15g", as.number);
            return std::string(buffer);
        }
        case ValueType::OBJ_STRING:
            return as.obj_string->toString();
        case ValueType::ARRAY: {
            if (!as.array) return "<null array>";
            return as.array->toString();
        }
        case ValueType::OBJECT: {
            if (!as.object) return "<null object>";
            return as.object->toString();
        }
        case ValueType::CALLABLE: {
            if (as.callable == nullptr) {
                return "<null callable>";
            }
            return as.callable->toString();
        }
        case ValueType::MODULE:
            return "<module>";
        case ValueType::CLASS:
            return as.klass->toString();
        case ValueType::INSTANCE:
            return as.instance->toString();
        case ValueType::BUFFER:
            return as.buffer->toString();
    }
    return "";
}

bool Value::isModule() const {
    return type == ValueType::MODULE;
}

Module* Value::asModule() const {
    if (isModule()) {
        return as.module;
    }
    return nullptr;
}

bool Value::isString() const {
    return type == ValueType::OBJ_STRING;
}

ObjString* Value::asString() const {
    if (isString()) {
        return as.obj_string;
    }
    return nullptr;
}

bool Value::isBuffer() const {
    return type == ValueType::BUFFER;
}

Buffer* Value::asBuffer() const {
    if (isBuffer()) {
        return as.buffer;
    }
    return nullptr;
}

}
