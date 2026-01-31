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

#include "types/array.h"
#include <sstream>

namespace neutron {

std::string Array::toString() const {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << elements[i].toString();
    }
    oss << "]";
    return oss.str();
}

Value Array::pop() {
    if (elements.empty()) {
        throw std::runtime_error("Cannot pop from empty array");
    }
    Value value = elements.back();
    elements.pop_back();
    return value;
}

Value& Array::at(size_t index) {
    if (index >= elements.size()) {
        std::string range = elements.empty() ? "[]" : "[0, " + std::to_string(elements.size()-1) + "]";
        throw std::runtime_error("Array index out of bounds: index " + std::to_string(index) + 
                                " is not within " + range);
    }
    return elements[index];
}

const Value& Array::at(size_t index) const {
    if (index >= elements.size()) {
        std::string range = elements.empty() ? "[]" : "[0, " + std::to_string(elements.size()-1) + "]";
        throw std::runtime_error("Array index out of bounds: index " + std::to_string(index) + 
                                " is not within " + range);
    }
    return elements[index];
}

void Array::set(size_t index, const Value& value) {
    if (index >= elements.size()) {
        std::string range = elements.empty() ? "[]" : "[0, " + std::to_string(elements.size()-1) + "]";
        throw std::runtime_error("Array index out of bounds: index " + std::to_string(index) + 
                                " is not within " + range);
    }
    elements[index] = value;
}

void Array::mark() {
    is_marked = true;
    // Mark all elements in the array
    // Note: Marking individual values would require implementing
    // a mark method on the Value class for garbage collection
}

} // namespace neutron
