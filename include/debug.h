#ifndef NEUTRON_DEBUG_H
#define NEUTRON_DEBUG_H

#include "bytecode.h"

namespace neutron {

void disassembleChunk(const Chunk* chunk, const char* name);
int disassembleInstruction(const Chunk* chunk, int offset);

} // namespace neutron

#endif // NEUTRON_DEBUG_H
