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
    OP_CONSTANT_LONG,  // For constants with 16-bit indices (>255)
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
    OP_VALIDATE_SAFE_FILE_FUNCTION,  // Validate function in safe file at runtime
    OP_VALIDATE_SAFE_FILE_VARIABLE,  // Validate variable in safe file at runtime
    OP_LOGICAL_OR,
    OP_BITWISE_AND,
    OP_BITWISE_OR,
    OP_BITWISE_XOR,
    OP_BITWISE_NOT,
    OP_LEFT_SHIFT,
    OP_RIGHT_SHIFT,
    OP_INVOKE,  // Optimized method call: GET_PROPERTY + CALL in one opcode
    OP_INCREMENT_LOCAL,  // Increment local variable by 1 (replaces GET_LOCAL+CONST+ADD+SET_LOCAL+POP)
    OP_DECREMENT_LOCAL,  // Decrement local variable by 1
    OP_LOOP_IF_LESS_LOCAL,  // Fused: if (local[slot] < constant) continue, else jump
    OP_INCREMENT_GLOBAL, // Increment global variable by 1 (replaces GET_GLOBAL+CONST+ADD+SET_GLOBAL+POP)

    // === Bytecode optimizer extended opcodes ===
    // These are emitted by BytecodeOptimizer post-compilation passes
    OP_CALL_FAST,        // Fast function call (no closure check)
    OP_TAIL_CALL,        // Tail-call optimized call
    OP_GET_GLOBAL_FAST,  // Fast global read (cached slot)
    OP_SET_GLOBAL_FAST,  // Fast global write (cached slot)
    OP_LOAD_LOCAL_0,     // Shortcut: GET_LOCAL 0
    OP_LOAD_LOCAL_1,     // Shortcut: GET_LOCAL 1
    OP_LOAD_LOCAL_2,     // Shortcut: GET_LOCAL 2
    OP_LOAD_LOCAL_3,     // Shortcut: GET_LOCAL 3
    OP_CONST_ZERO,       // Push 0.0
    OP_CONST_ONE,        // Push 1.0
    OP_CONST_INT8,       // Push int8 constant (1-byte operand)
    OP_ADD_INT,          // Integer-specialized add
    OP_SUB_INT,          // Integer-specialized subtract
    OP_MUL_INT,          // Integer-specialized multiply
    OP_DIV_INT,          // Integer-specialized divide
    OP_MOD_INT,          // Integer-specialized modulo
    OP_NEGATE_INT,       // Integer-specialized negate
    OP_LESS_INT,         // Integer-specialized less-than
    OP_GREATER_INT,      // Integer-specialized greater-than
    OP_EQUAL_INT,        // Integer-specialized equality
    OP_INC_LOCAL_INT,    // Increment local by 1 (integer fast path)
    OP_DEC_LOCAL_INT,    // Decrement local by 1 (integer fast path)
    OP_LESS_JUMP,        // Fused: OP_LESS + OP_JUMP_IF_FALSE
    OP_GREATER_JUMP,     // Fused: OP_GREATER + OP_JUMP_IF_FALSE
    OP_EQUAL_JUMP,       // Fused: OP_EQUAL + OP_JUMP_IF_FALSE
    OP_ADD_LOCAL_CONST,  // Fused: GET_LOCAL + CONSTANT + ADD
    OP_TYPE_GUARD,       // Runtime type guard (for JIT)
    OP_LOOP_HINT,        // JIT loop compilation hint

    OP_COUNT             // Sentinel: total number of opcodes
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
