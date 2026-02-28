#pragma once

#include <string>
#include <cstdint>

namespace neutron {

// Object type tags for fast type identification without dynamic_cast/RTTI
enum class ObjType : uint8_t {
    OBJ_GENERIC = 0,
    OBJ_FUNCTION,
    OBJ_NATIVE_FN,
    OBJ_BOUND_METHOD,
    OBJ_BOUND_ARRAY_METHOD,
    OBJ_BOUND_STRING_METHOD,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_JSON_OBJECT,
    OBJ_JSON_ARRAY,
    OBJ_MODULE,
    OBJ_BUFFER,
};

class Object {
public:
    ObjType obj_type = ObjType::OBJ_GENERIC;
    bool is_marked = false;
    virtual ~Object() = default;
    virtual std::string toString() const = 0;
    virtual void mark() { is_marked = true; }
    virtual void sweep() {} // Default implementation, can be overridden
};

}
