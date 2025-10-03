#pragma once

#include <string>
#include <vector>
#include "object.h"
#include "value.h"

namespace neutron {

class Array : public Object {
public:
    std::vector<Value> elements;
    
    Array() = default;
    Array(std::vector<Value> elements) : elements(std::move(elements)) {}
    
    std::string toString() const override;
    
    size_t size() const { return elements.size(); }
    void push(const Value& value) { elements.push_back(value); }
    Value pop();
    Value& at(size_t index);
    const Value& at(size_t index) const;
    void set(size_t index, const Value& value);
    
    void mark() override;
};

}
