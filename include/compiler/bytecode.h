#ifndef NEUTRON_BYTECODE_H
#define NEUTRON_BYTECODE_H

/*
 * Code Documentation: Bytecode Instruction Set (bytecode.h)
 * =========================================================
 * 
 * This header defines the bytecode instruction set for the Neutron VM.
 * It contains all opcodes, the Chunk structure for storing bytecode,
 * and constant pool management.
 * 
 * What This File Includes:
 * ------------------------
 * - OpCode enum: All VM instructions
 * - Chunk class: Bytecode container with constants and debug info
 * - Constant pool: Storage for literals and identifiers
 * 
 * How It Works:
 * -------------
 * Bytecode is a linear sequence of instructions executed by the VM:
 * 1. Each opcode is a single byte (0-255 instructions max)
 * 2. Operands follow the opcode (constant indices, jump offsets)
 * 3. Constants are stored in a separate pool, referenced by index
 * 4. Line number table maps bytecode offsets to source lines
 * 
 * Instruction Format:
 *   [OPCODE] [OPERAND1] [OPERAND2] ...
 * 
 * Example:
 *   OP_CONSTANT 0x0002  ; Load constant at index 2
 *   OP_ADD              ; Add top two stack values
 *   OP_RETURN           ; Return result
 * 
 * Opcode Categories:
 * - Stack operations: POP, DUP
 * - Constants: CONSTANT, NIL, TRUE, FALSE
 * - Locals: GET_LOCAL, SET_LOCAL
 * - Globals: GET_GLOBAL, SET_GLOBAL, DEFINE_GLOBAL
 * - Arithmetic: ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULO
 * - Comparison: EQUAL, GREATER, LESS, NOT_EQUAL
 * - Logical: NOT, LOGICAL_AND, LOGICAL_OR
 * - Bitwise: BITWISE_AND, BITWISE_OR, BITWISE_XOR, BITWISE_NOT
 * - Control flow: JUMP, JUMP_IF_FALSE, LOOP
 * - Functions: CALL, CLOSURE, RETURN
 * - Objects: GET_PROPERTY, SET_PROPERTY, ARRAY, OBJECT
 * - Exception handling: TRY, END_TRY, THROW
 * 
 * Adding Features:
 * ----------------
 * - New opcodes: Add to OpCode enum, implement in vm.cpp run loop
 * - Optimized opcodes: Add to "extended opcodes" section, implement in optimizer
 * - JIT hints: Add hint opcodes for JIT compilation (LOOP_HINT, TYPE_GUARD)
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT exceed 256 opcodes (uint8_t limit)
 * - Do NOT change opcode values without updating the optimizer
 * - Do NOT add opcodes that break stack balance
 * - Do NOT forget to update OpCode string tables for debugging
 * 
 * Optimization Notes:
 * -------------------
 * Extended opcodes (OP_CALL_FAST, OP_CONST_ZERO, etc.) are emitted by
 * the BytecodeOptimizer in a post-compilation pass. They provide
 * specialized fast paths for common patterns.
 */

#include <vector>
#include <cstdint>

namespace neutron {

// Forward declarations
struct Value;

/**
 * @brief OpCode - The Neutron Instruction Set.
 * 
 * Each opcode represents a single VM instruction. The VM's run loop
 * decodes and executes these instructions in sequence.
 * 
 * Conventions:
 * - Opcodes are named in uppercase with underscores (OP_ADD)
 * - Stack operations consume operands from stack, push results
 * - Jump offsets are relative (forward = positive, backward = negative)
 * - Constant indices are 16-bit (allowing 65536 constants per function)
 */
enum class OpCode : uint8_t {
    // === Core instructions ===
    OP_RETURN,           ///< Return from function (pops return value)
    OP_CONSTANT,         ///< Push constant from pool (16-bit index)
    OP_CONSTANT_LONG,    ///< Extended constant for large indices
    OP_NIL,              ///< Push nil value
    OP_TRUE,             ///< Push true
    OP_FALSE,            ///< Push false
    OP_POP,              ///< Pop and discard top of stack
    OP_DUP,              ///< Duplicate top of stack
    
    // === Local variables (fast stack slots) ===
    OP_GET_LOCAL,        ///< Push local variable by slot index
    OP_SET_LOCAL,        ///< Set local variable, keep value on stack
    
    // === Global variables (hash table lookup) ===
    OP_GET_GLOBAL,       ///< Push global variable by name
    OP_DEFINE_GLOBAL,    ///< Define new global variable
    OP_SET_GLOBAL,       ///< Set global variable by name
    OP_SET_GLOBAL_TYPED, ///< Type-safe global assignment (with runtime check)
    OP_SET_LOCAL_TYPED,  ///< Type-safe local assignment (with runtime check)
    OP_DEFINE_TYPED_GLOBAL,  ///< Define global with type annotation
    
    // === Object properties ===
    OP_GET_PROPERTY,     ///< Get object property by name
    OP_SET_PROPERTY,     ///< Set object property
    
    // === Arithmetic and comparison ===
    OP_EQUAL,            ///< Equality comparison (==)
    OP_GREATER,          ///< Greater than (>)
    OP_LESS,             ///< Less than (<)
    OP_ADD,              ///< Addition (+) or string concatenation
    OP_SUBTRACT,         ///< Subtraction (-)
    OP_MULTIPLY,         ///< Multiplication (*)
    OP_DIVIDE,           ///< Division (/)
    OP_MODULO,           ///< Modulo (%)
    OP_NOT,              ///< Logical negation (not)
    OP_NEGATE,           ///< Arithmetic negation (-x)
    
    // === Output ===
    OP_SAY,              ///< Print top of stack (with newline)
    
    // === Control flow ===
    OP_JUMP,             ///< Unconditional jump (relative offset)
    OP_JUMP_IF_FALSE,    ///< Jump if top of stack is false/nil
    OP_LOOP,             ///< Loop back to start (relative backward offset)
    
    // === Functions and closures ===
    OP_CALL,             ///< Call function (operand = arg count)
    OP_CLOSURE,          ///< Create closure (captures upvalues)
    OP_GET_UPVALUE,      ///< Get upvalue (captured local)
    OP_SET_UPVALUE,      ///< Set upvalue
    OP_CLOSE_UPVALUE,    ///< Close upvalue when leaving scope
    
    // === Composite types ===
    OP_ARRAY,            ///< Create array from stack values
    OP_OBJECT,           ///< Create object (dictionary) from stack
    OP_INDEX_GET,        ///< Array/object index access
    OP_INDEX_SET,        ///< Array/object index assignment
    
    // === OOP ===
    OP_THIS,             ///< Push 'this' reference (for methods)
    
    // === Loop control ===
    OP_BREAK,            ///< Break out of loop
    OP_CONTINUE,         ///< Continue to next iteration
    
    // === Exception handling ===
    OP_TRY,              ///< Begin try block (push exception frame)
    OP_END_TRY,          ///< End try block (pop exception frame)
    OP_THROW,            ///< Throw exception (value on stack)
    
    // === Additional operators ===
    OP_NOT_EQUAL,        ///< Inequality (!=)
    OP_LOGICAL_AND,      ///< Logical and (short-circuit)
    OP_LOGICAL_OR,       ///< Logical or (short-circuit)
    
    // === Bitwise operations ===
    OP_BITWISE_AND,      ///< Bitwise and (&)
    OP_BITWISE_OR,       ///< Bitwise or (|)
    OP_BITWISE_XOR,      ///< Bitwise xor (^)
    OP_BITWISE_NOT,      ///< Bitwise not (~)
    OP_LEFT_SHIFT,       ///< Left shift (<<)
    OP_RIGHT_SHIFT,      ///< Right shift (>>)
    
    // === Optimized method calls ===
    OP_INVOKE,           ///< Optimized: GET_PROPERTY + CALL combined
    
    // === Increment/decrement shortcuts ===
    OP_INCREMENT_LOCAL,  ///< Increment local by 1 (i++)
    OP_DECREMENT_LOCAL,  ///< Decrement local by 1 (i--)
    OP_INCREMENT_GLOBAL, ///< Increment global by 1
    
    // === Fused loop instruction ===
    OP_LOOP_IF_LESS_LOCAL,  ///< Fused: if (local < const) loop, else jump
    
    // === Safe block validation ===
    OP_VALIDATE_SAFE_FUNCTION,   ///< Validate function in safe block
    OP_VALIDATE_SAFE_VARIABLE,   ///< Validate variable in safe block
    OP_VALIDATE_SAFE_FILE_FUNCTION,  ///< Validate function in safe file
    OP_VALIDATE_SAFE_FILE_VARIABLE,  ///< Validate variable in safe file

    // === Bytecode optimizer extended opcodes ===
    // These are emitted by BytecodeOptimizer in post-compilation passes
    OP_CALL_FAST,        ///< Fast function call (no closure check overhead)
    OP_TAIL_CALL,        ///< Tail-call optimization (reuses current frame)
    OP_GET_GLOBAL_FAST,  ///< Fast global read (cached slot, no hash lookup)
    OP_SET_GLOBAL_FAST,  ///< Fast global write (cached slot)
    OP_LOAD_LOCAL_0,     ///< Shortcut: GET_LOCAL 0 (single byte instruction)
    OP_LOAD_LOCAL_1,     ///< Shortcut: GET_LOCAL 1
    OP_LOAD_LOCAL_2,     ///< Shortcut: GET_LOCAL 2
    OP_LOAD_LOCAL_3,     ///< Shortcut: GET_LOCAL 3
    OP_CONST_ZERO,       ///< Push 0.0 (common constant optimization)
    OP_CONST_ONE,        ///< Push 1.0
    OP_CONST_INT8,       ///< Push int8 constant (1-byte signed operand)
    
    // === Integer-specialized arithmetic (skip float checks) ===
    OP_ADD_INT,          ///< Integer addition (assumes integer operands)
    OP_SUB_INT,          ///< Integer subtraction
    OP_MUL_INT,          ///< Integer multiplication
    OP_DIV_INT,          ///< Integer division
    OP_MOD_INT,          ///< Integer modulo
    OP_NEGATE_INT,       ///< Integer negation
    OP_LESS_INT,         ///< Integer less-than
    OP_GREATER_INT,      ///< Integer greater-than
    OP_EQUAL_INT,        ///< Integer equality
    OP_INC_LOCAL_INT,    ///< Increment local by 1 (integer fast path)
    OP_DEC_LOCAL_INT,    ///< Decrement local by 1 (integer fast path)
    
    // === Fused comparison+jump (reduces branch instructions) ===
    OP_LESS_JUMP,        ///< Fused: OP_LESS + OP_JUMP_IF_FALSE
    OP_GREATER_JUMP,     ///< Fused: OP_GREATER + OP_JUMP_IF_FALSE
    OP_EQUAL_JUMP,       ///< Fused: OP_EQUAL + OP_JUMP_IF_FALSE
    
    // === Fused arithmetic ===
    OP_ADD_LOCAL_CONST,  ///< Fused: GET_LOCAL + CONSTANT + ADD
    
    // === JIT support ===
    OP_TYPE_GUARD,       ///< Runtime type guard (for JIT deoptimization)
    OP_LOOP_HINT,        ///< Hint for JIT loop compilation

    OP_COUNT             ///< Sentinel: total number of opcodes (not a real opcode)
};

/**
 * @brief Chunk - A bytecode container.
 * 
 * A Chunk holds:
 * - code: The bytecode instruction stream
 * - constants: Literal values referenced by OP_CONSTANT
 * - lines: Source line numbers for each bytecode offset (debug info)
 * 
 * Each Function has an associated Chunk containing its compiled bytecode.
 * The VM executes chunks by interpreting the code array.
 * 
 * Memory Layout:
 * - code and lines are parallel arrays (code[i] is at line lines[i])
 * - constants are referenced by 16-bit indices
 * - Multiple chunks can exist simultaneously (one per function)
 */
class Chunk {
public:
    std::vector<uint8_t> code;    ///< Bytecode instruction stream
    std::vector<Value> constants; ///< Constant pool (literals, identifiers)
    std::vector<int> lines;       ///< Source line numbers for debugging

    /**
     * @brief Write a byte to the bytecode stream.
     * @param byte The byte to write.
     * @param line Source line number for debug info.
     */
    void write(uint8_t byte, int line);
    
    /**
     * @brief Add a constant to the pool.
     * @param value The constant value.
     * @return Index of the constant in the pool.
     */
    int addConstant(const Value& value);
};

} // namespace neutron

#endif // NEUTRON_BYTECODE_H
