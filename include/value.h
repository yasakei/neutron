#pragma once

#include <string>
#include <variant>
#include <vector>

namespace neutron {

class Array;
class Object;
class Callable;
class Module;
class Class;
class Instance;

enum class ValueType {
    NIL,
    BOOLEAN,
    NUMBER,
    STRING,
    ARRAY,
    OBJECT,
    CALLABLE,
    MODULE,
    CLASS,
    INSTANCE
};

using Literal = std::variant<std::nullptr_t, bool, double, std::string, Array*, Object*, Callable*, Module*, class Class*, class Instance*>;

struct Value {
    ValueType type;
    Literal as;

    Value();
    Value(std::nullptr_t);
    Value(bool value);
    Value(double value);
    Value(const std::string& value);
    Value(Array* array);
    Value(Object* object);
    Value(Callable* callable);
    Value(Module* module);
    Value(Class* klass);
    Value(Instance* instance);

    std::string toString() const;
};

}
