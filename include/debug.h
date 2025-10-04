#ifndef NEUTRON_DEBUG_H
#define NEUTRON_DEBUG_H

#include "bytecode.h"

namespace neutron {

void disassembleChunk(const Chunk* chunk, const char* name);
size_t disassembleInstruction(const Chunk* chunk, size_t offset);

} // namespace neutron

#endif // NEUTRON_DEBUG_H
