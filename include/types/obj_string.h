#pragma once

#include "object.h"
#include <string>
#include <cstdint>

namespace neutron {

class ObjString : public Object {
public:
    std::string chars;
    mutable uint32_t hash;
    mutable bool hashComputed;

    ObjString(const std::string& chars);
    ObjString(std::string&& chars);
    ObjString(const std::string& chars, bool computeHash);  // For data strings
    ObjString(std::string&& chars, bool computeHash);
    virtual ~ObjString() = default;

    std::string toString() const override;
    
    uint32_t getHash() const;
    static uint32_t hashString(const std::string& str);
};

struct ObjStringHash {
    size_t operator()(const ObjString* str) const {
        return str->getHash();
    }
};

struct ObjStringEqual {
    bool operator()(const ObjString* a, const ObjString* b) const {
        return a->chars == b->chars;
    }
};

// Fast pointer equality for interned strings
struct ObjStringPtrEqual {
    bool operator()(const ObjString* a, const ObjString* b) const {
        return a == b;  // O(1) pointer comparison
    }
};

}
