/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, for both open source and commercial purposes.
 * 
 * Conditions:
 * 
 * 1. The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 * 
 * 2. Attribution is appreciated but NOT required.
 *    Suggested (optional) credit:
 *    "Built using Neutron Programming Language (c) yasakei"
 * 
 * 3. The name "Neutron" and the name of the copyright holder may not be used
 *    to endorse or promote products derived from this Software without prior
 *    written permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
