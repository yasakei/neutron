#pragma once

#include <string>
#include <variant>
#include <vector>

namespace neutron {

class Array;
class Object;
class ObjString;
class Callable;
class Module;
class Class;
class Instance;
class Buffer;

enum class ValueType {
    NIL,
    BOOLEAN,
    NUMBER,
    OBJ_STRING,
    ARRAY,
    OBJECT,
    CALLABLE,
    MODULE,
    CLASS,
    INSTANCE,
    BUFFER
};

using Literal = std::variant<std::nullptr_t, bool, double, ObjString*, Array*, Object*, Callable*, Module*, class Class*, class Instance*, class Buffer*>;

struct Value {
    ValueType type;
    Literal as;

    Value();
    Value(std::nullptr_t);
    Value(bool value);
    Value(double value);
    Value(ObjString* string);
    // Keep this for convenience, but it will allocate
    Value(const std::string& value);
    Value(Array* array);
    Value(Object* object);
    Value(Callable* callable);
    Value(Module* module);
    Value(Class* klass);
    Value(Instance* instance);
    Value(Buffer* buffer);

    std::string toString() const;
    
    bool isString() const;
    ObjString* asString() const;

    bool isModule() const;
    Module* asModule() const;
    
    bool isBuffer() const;
    Buffer* asBuffer() const;
};

}
