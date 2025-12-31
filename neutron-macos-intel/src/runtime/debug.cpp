#include <iostream>
#include "runtime/debug.h"
#include "compiler/bytecode.h"
#include "vm.h"

namespace neutron {

std::ostream& operator<<(std::ostream& os, ValueType type) {
    switch (type) {
        case ValueType::NIL:
            os << "NIL";
            break;
        case ValueType::BOOLEAN:
            os << "BOOLEAN";
            break;
        case ValueType::NUMBER:
            os << "NUMBER";
            break;
        case ValueType::OBJ_STRING:
            os << "STRING";
            break;
        case ValueType::OBJECT:
            os << "OBJECT";
            break;
        case ValueType::CALLABLE:
            os << "CALLABLE";
            break;
        case ValueType::ARRAY:
            os << "ARRAY";
            break;
        case ValueType::MODULE:
            os << "MODULE";
            break;
        case ValueType::CLASS:
            os << "CLASS";
            break;
        case ValueType::INSTANCE:
            os << "INSTANCE";
            break;
        case ValueType::BUFFER:
            os << "BUFFER";
            break;
    }
    return os;
}

void disassembleChunk(const Chunk* chunk, const char* name) {
    std::cout << "== " << name << " ==" << std::endl;

    for (size_t offset = 0; offset < chunk->code.size();) {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int simpleInstruction(const char* name, int offset) {
    std::cout << name << std::endl;
    return offset + 1;
}

static int constantInstruction(const char* name, const Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    std::cout << chunk->constants[constant].toString();
    std::cout << "' (" << chunk->constants[constant].type << ")" << std::endl;
    return offset + 2;
}

// static int byteInstruction(const char* name, const Chunk* chunk, int offset) {
//     uint8_t slot = chunk->code[offset + 1];
//     printf("%-16s %4d\n", name, slot);
//     return offset + 2;
/**
 * @brief Disassembles and prints the instruction at the given bytecode offset.
 *
 * Reads the opcode stored in chunk->code[offset], prints a formatted line
 * showing the offset, source line number, and a human-readable representation
 * of the instruction (including referenced constant information when applicable).
 *
 * @param chunk The bytecode chunk containing instructions, constants, and line mappings.
 * @param offset The byte offset within the chunk at which to disassemble an instruction.
 * @return size_t The offset immediately following the disassembled instruction.
 */

size_t disassembleInstruction(const Chunk* chunk, size_t offset) {
    std::cout << offset << " ";
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        std::cout << "   | ";
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch ((OpCode)instruction) {
        case OpCode::OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case OpCode::OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OpCode::OP_NIL:
            return simpleInstruction("OP_NIL", offset);
        case OpCode::OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OpCode::OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case OpCode::OP_POP:
            return simpleInstruction("OP_POP", offset);
        case OpCode::OP_DUP:
            return simpleInstruction("OP_DUP", offset);
        case OpCode::OP_GET_LOCAL:
            return constantInstruction("OP_GET_LOCAL", chunk, offset);
        case OpCode::OP_SET_LOCAL:
            return constantInstruction("OP_SET_LOCAL", chunk, offset);
        case OpCode::OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OpCode::OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OpCode::OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OpCode::OP_SET_GLOBAL_TYPED:
            return constantInstruction("OP_SET_GLOBAL_TYPED", chunk, offset);
        case OpCode::OP_SET_LOCAL_TYPED:
            return constantInstruction("OP_SET_LOCAL_TYPED", chunk, offset);
        case OpCode::OP_DEFINE_TYPED_GLOBAL:
            return constantInstruction("OP_DEFINE_TYPED_GLOBAL", chunk, offset);
        case OpCode::OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case OpCode::OP_NOT_EQUAL:
            return simpleInstruction("OP_NOT_EQUAL", offset);
        case OpCode::OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case OpCode::OP_LESS:
            return simpleInstruction("OP_LESS", offset);
        case OpCode::OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OpCode::OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OpCode::OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OpCode::OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OpCode::OP_MODULO:
            return simpleInstruction("OP_MODULO", offset);
        case OpCode::OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case OpCode::OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OpCode::OP_SAY:
            return simpleInstruction("OP_SAY", offset);
        case OpCode::OP_GET_PROPERTY:
            return constantInstruction("OP_GET_PROPERTY", chunk, offset);
        case OpCode::OP_SET_PROPERTY:
            return constantInstruction("OP_SET_PROPERTY", chunk, offset);
        case OpCode::OP_ARRAY:
            return constantInstruction("OP_ARRAY", chunk, offset);
        case OpCode::OP_INDEX_GET:
            return simpleInstruction("OP_INDEX_GET", offset);
        case OpCode::OP_INDEX_SET:
            return simpleInstruction("OP_INDEX_SET", offset);
        case OpCode::OP_THIS:
            return simpleInstruction("OP_THIS", offset);
        case OpCode::OP_JUMP:
            return constantInstruction("OP_JUMP", chunk, offset);
        case OpCode::OP_JUMP_IF_FALSE:
            return constantInstruction("OP_JUMP_IF_FALSE", chunk, offset);
        case OpCode::OP_LOOP:
            return constantInstruction("OP_LOOP", chunk, offset);
        case OpCode::OP_CALL:
            return constantInstruction("OP_CALL", chunk, offset);
        case OpCode::OP_TRY:
            return simpleInstruction("OP_TRY", offset);
        case OpCode::OP_END_TRY:
            return simpleInstruction("OP_END_TRY", offset);
        case OpCode::OP_THROW:
            return simpleInstruction("OP_THROW", offset);
        case OpCode::OP_LOGICAL_AND:
            return simpleInstruction("OP_LOGICAL_AND", offset);
        case OpCode::OP_VALIDATE_SAFE_FUNCTION:
            return simpleInstruction("OP_VALIDATE_SAFE_FUNCTION", offset);
        case OpCode::OP_VALIDATE_SAFE_VARIABLE:
            return constantInstruction("OP_VALIDATE_SAFE_VARIABLE", chunk, offset);
        case OpCode::OP_VALIDATE_SAFE_FILE_FUNCTION:
            return simpleInstruction("OP_VALIDATE_SAFE_FILE_FUNCTION", offset);
        case OpCode::OP_VALIDATE_SAFE_FILE_VARIABLE:
            return constantInstruction("OP_VALIDATE_SAFE_FILE_VARIABLE", chunk, offset);
        case OpCode::OP_LOGICAL_OR:
            return simpleInstruction("OP_LOGICAL_OR", offset);
        case OpCode::OP_CLOSURE:
            return constantInstruction("OP_CLOSURE", chunk, offset);
        case OpCode::OP_GET_UPVALUE:
            return constantInstruction("OP_GET_UPVALUE", chunk, offset);
        case OpCode::OP_SET_UPVALUE:
            return constantInstruction("OP_SET_UPVALUE", chunk, offset);
        case OpCode::OP_CLOSE_UPVALUE:
            return simpleInstruction("OP_CLOSE_UPVALUE", offset);
        case OpCode::OP_BREAK:
            return simpleInstruction("OP_BREAK", offset);
        case OpCode::OP_CONTINUE:
            return simpleInstruction("OP_CONTINUE", offset);
        default:
            std::cout << "Unknown opcode " << instruction << std::endl;
            return offset + 1;
    }
}

} // namespace neutron