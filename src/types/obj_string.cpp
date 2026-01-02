#include "types/obj_string.h"

namespace neutron {

ObjString::ObjString(const std::string& chars) : chars(chars), hash(0), hashComputed(false) {
    hash = hashString(chars);
    hashComputed = true;
}

ObjString::ObjString(std::string&& chars) : chars(std::move(chars)), hash(0), hashComputed(false) {
    hash = hashString(this->chars);
    hashComputed = true;
}

// Constructors for data strings - skip hash computation
ObjString::ObjString(const std::string& chars, bool computeHash) 
    : chars(chars), hash(0), hashComputed(false) {
    if (computeHash) {
        hash = hashString(this->chars);
        hashComputed = true;
    }
}

ObjString::ObjString(std::string&& chars, bool computeHash) 
    : chars(std::move(chars)), hash(0), hashComputed(false) {
    if (computeHash) {
        hash = hashString(this->chars);
        hashComputed = true;
    }
}

std::string ObjString::toString() const {
    return chars;
}

uint32_t ObjString::getHash() const {
    if (!hashComputed) {
        hash = hashString(chars);
        hashComputed = true;
    }
    return hash;
}

uint32_t ObjString::hashString(const std::string& key) {
    uint32_t hash = 2166136261u;
    for (char c : key) {
        hash ^= (uint8_t)c;
        hash *= 16777619;
    }
    return hash;
}

}
