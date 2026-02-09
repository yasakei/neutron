#pragma once

#include <string>
#include <unordered_map>
#include "types/object.h"
#include "types/class.h"
#include "types/value.h"
#include "types/obj_string.h"

namespace neutron {

// Inline storage for small objects - avoids hash map overhead
constexpr size_t INLINE_FIELD_COUNT = 4;

struct InlineField {
    ObjString* key = nullptr;
    Value value;
};

class Instance : public Object {
public:
    Instance(Class* klass);
    ~Instance();
    std::string toString() const override;
    
    // Field access methods - inlined for performance in hot VM loops
    inline Value* getField(ObjString* key) {
        // Fast path: check inline fields first
        for (uint8_t i = 0; i < inlineCount; ++i) {
            if (inlineFields[i].key == key) {
                return &inlineFields[i].value;
            }
        }
        // Slow path: check overflow map
        if (__builtin_expect(overflowFields != nullptr, 0)) {
            auto it = overflowFields->find(key);
            if (it != overflowFields->end()) {
                return &it->second;
            }
        }
        return nullptr;
    }
    
    inline void setField(ObjString* key, const Value& value) {
        // Fast path: check if key already exists in inline fields
        for (uint8_t i = 0; i < inlineCount; ++i) {
            if (inlineFields[i].key == key) {
                inlineFields[i].value = value;
                return;
            }
        }
        setFieldSlow(key, value);
    }
    
    // Slow path for setField (new field or overflow)
    void setFieldSlow(ObjString* key, const Value& value);
    
    // Reset for pool reuse
    void reset(Class* newKlass);
    
    Class* klass;
    
    // Inline storage for first N fields (fast path)
    InlineField inlineFields[INLINE_FIELD_COUNT];
    uint8_t inlineCount = 0;
    
    // Overflow to hash map for objects with many fields
    std::unordered_map<ObjString*, Value, ObjStringHash, ObjStringPtrEqual>* overflowFields = nullptr;
    
    friend class VM;  // For GC access
};

}
