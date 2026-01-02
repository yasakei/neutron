#pragma once

#include <string>
#include <unordered_map>
#include "types/object.h"
#include "types/class.h"
#include "types/value.h"
#include "types/obj_string.h"

namespace neutron {

// Inline storage for small objects - avoids hash map overhead
constexpr size_t INLINE_FIELD_COUNT = 8;

struct InlineField {
    ObjString* key = nullptr;
    Value value;
};

class Instance : public Object {
public:
    Instance(Class* klass);
    ~Instance();
    std::string toString() const override;
    
    // Field access methods
    Value* getField(ObjString* key);
    void setField(ObjString* key, const Value& value);
    
    Class* klass;
    
private:
    // Inline storage for first N fields (fast path)
    InlineField inlineFields[INLINE_FIELD_COUNT];
    uint8_t inlineCount = 0;
    
    // Overflow to hash map for objects with many fields
    std::unordered_map<ObjString*, Value, ObjStringHash, ObjStringPtrEqual>* overflowFields = nullptr;
    
    friend class VM;  // For GC access
};

}
