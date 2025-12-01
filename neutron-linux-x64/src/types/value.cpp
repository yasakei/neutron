#include "types/value.h"
#include "types/array.h"
#include "types/object.h"
#include "types/callable.h"
#include "modules/module.h"
#include "types/class.h"
#include "types/instance.h"
#include <sstream>

namespace neutron {

Value::Value() : type(ValueType::NIL), as(nullptr) {}

Value::Value(std::nullptr_t) : type(ValueType::NIL), as(nullptr) {}

Value::Value(bool value) : type(ValueType::BOOLEAN), as(value) {}

Value::Value(double value) : type(ValueType::NUMBER), as(value) {}

Value::Value(const std::string& value) : type(ValueType::STRING), as(value) {}

Value::Value(Array* array) : type(ValueType::ARRAY), as(array) {}

Value::Value(Object* object) : type(ValueType::OBJECT), as(object) {}

Value::Value(Callable* callable) : type(ValueType::CALLABLE), as(callable) {}

Value::Value(Module* module) : type(ValueType::MODULE), as(module) {}

Value::Value(Class* klass) : type(ValueType::CLASS), as(klass) {}

Value::Value(Instance* instance) : type(ValueType::INSTANCE), as(instance) {}

std::string Value::toString() const {
    switch (type) {
        case ValueType::NIL:
            return "nil";
        case ValueType::BOOLEAN:
            return std::get<bool>(as) ? "true" : "false";
        case ValueType::NUMBER: {
            // Convert double to string, removing trailing zeros
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%.15g", std::get<double>(as));
            return std::string(buffer);
        }
        case ValueType::STRING:
            return std::get<std::string>(as);
        case ValueType::ARRAY: {
            Array* arr = std::get<Array*>(as);
            if (!arr) return "<null array>";
            return arr->toString();
        }
        case ValueType::OBJECT: {
            Object* obj = std::get<Object*>(as);
            if (!obj) return "<null object>";
            return obj->toString();
        }
        case ValueType::CALLABLE: {
            Callable* callable = std::get<Callable*>(as);
            if (callable == nullptr) {
                return "<null callable>";
            }
            return callable->toString();
        }
        case ValueType::MODULE:
            return "<module>";
        case ValueType::CLASS:
            return std::get<Class*>(as)->toString();
        case ValueType::INSTANCE:
            return std::get<Instance*>(as)->toString();
    }
    return "";
}

bool Value::isModule() const {
    return type == ValueType::MODULE;
}

Module* Value::asModule() const {
    if (isModule()) {
        return std::get<Module*>(as);
    }
    return nullptr;
}

}
