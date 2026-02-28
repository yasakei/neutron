#include "compiler/bytecode.h"
#include "vm.h"

namespace neutron {

void Chunk::write(uint8_t byte, int line) {
    code.push_back(byte);
    lines.push_back(line);
}

int Chunk::addConstant(const Value& value) {
    constants.push_back(value);
    return constants.size() - 1;
}

} // namespace neutron
