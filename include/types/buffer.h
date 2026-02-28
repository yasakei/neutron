#pragma once

#include "types/object.h"
#include <vector>
#include <cstdint>

namespace neutron {

class Buffer : public Object {
public:
    std::vector<uint8_t> data;

    Buffer(size_t size);
    std::string toString() const override;
    
    uint8_t get(size_t index) const;
    void set(size_t index, uint8_t value);
    size_t size() const;
};

}
