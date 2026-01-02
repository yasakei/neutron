#pragma once

#include <string>
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

union ValueUnion {
    bool boolean;
    double number;
    ObjString* obj_string;
    Array* array;
    Object* object;
    Callable* callable;
    Module* module;
    Class* klass;
    Instance* instance;
    Buffer* buffer;
};

struct Value {
    ValueType type;
    ValueUnion as;

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
