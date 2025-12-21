#ifndef NEUTRON_BYTECODE_H
#define NEUTRON_BYTECODE_H

#include <vector>
#include <cstdint>

namespace neutron {

// Forward declarations
struct Value;

enum class OpCode : uint8_t {
    OP_RETURN,
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_DUP,  // Duplicate top of stack
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_SET_GLOBAL_TYPED,  // Type-safe global assignment
    OP_SET_LOCAL_TYPED,   // Type-safe local assignment
    OP_DEFINE_TYPED_GLOBAL,  // Define a global variable with type annotation
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULO,
    OP_NOT,
    OP_NEGATE,
    OP_SAY,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLOSE_UPVALUE,
    OP_ARRAY,
    OP_OBJECT,
    OP_INDEX_GET,
    OP_INDEX_SET,
    OP_THIS,
    OP_BREAK,
    OP_CONTINUE,
    OP_TRY,
    OP_END_TRY,
    OP_THROW,
    OP_NOT_EQUAL,
    OP_LOGICAL_AND,
    OP_VALIDATE_SAFE_FUNCTION,  // Validate function in safe block at runtime
    OP_VALIDATE_SAFE_VARIABLE,  // Validate variable in safe block at runtime
    OP_LOGICAL_OR,
    OP_BITWISE_AND,
    OP_BITWISE_OR,
    OP_BITWISE_XOR,
    OP_BITWISE_NOT,
    OP_LEFT_SHIFT,
    OP_RIGHT_SHIFT
};

class Chunk {
public:
    std::vector<uint8_t> code;
    std::vector<Value> constants;
    std::vector<int> lines;

    void write(uint8_t byte, int line);
    int addConstant(const Value& value);
};

} // namespace neutron

#endif // NEUTRON_BYTECODE_H
