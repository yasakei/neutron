#include "types/buffer.h"
#include <sstream>
#include <iomanip>

namespace neutron {

Buffer::Buffer(size_t size) : data(size, 0) {}

std::string Buffer::toString() const {
    std::stringstream ss;
    ss << "<buffer size=" << data.size() << ">";
    return ss.str();
}

uint8_t Buffer::get(size_t index) const {
    if (index >= data.size()) {
        throw std::out_of_range("Buffer index out of bounds");
    }
    return data[index];
}

void Buffer::set(size_t index, uint8_t value) {
    if (index >= data.size()) {
        throw std::out_of_range("Buffer index out of bounds");
    }
    data[index] = value;
}

size_t Buffer::size() const {
    return data.size();
}

}
