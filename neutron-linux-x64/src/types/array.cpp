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
