/*
 * Neutron Hybrid Compiler - Bytecode Optimizer
 * Copyright (c) 2026 yasakei
 *
 * Post-compilation optimization pass that transforms generic bytecode into
 * specialized fast-path instructions. This is the "hybrid" part — the compiler
 * performs type inference and pattern matching on the bytecode to emit
 * specialized opcodes that skip runtime type checks.
 *
 * Optimizations:
 * 1. Integer specialization: numeric loops with integer arithmetic
 * 2. Fused compare-and-jump: eliminate separate compare + branch
 * 3. Superinstructions: common instruction sequences merged
 * 4. Local variable shortcuts: LOAD_LOCAL_0..3 for hot locals
 * 5. Constant folding: compile-time evaluation of constant expressions
 * 6. Tail call detection: convert qualifying calls to OP_TAIL_CALL
 * 7. Dead code elimination: remove unreachable code after returns/jumps
 */

#ifndef NEUTRON_BYTECODE_OPTIMIZER_H
#define NEUTRON_BYTECODE_OPTIMIZER_H

#include "compiler/bytecode.h"
#include <vector>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>

namespace neutron {

class BytecodeOptimizer {
public:
    BytecodeOptimizer() = default;
    
    /**
     * Run all optimization passes on a chunk.
     * This modifies the chunk in-place.
     */
    void optimize(Chunk& chunk);
    
    /**
     * Optimization statistics for monitoring
     */
    struct Stats {
        int local_shortcuts = 0;       // OP_LOAD_LOCAL_0..3 replacements
        int int_specializations = 0;   // Integer-specialized arithmetic
        int fused_compares = 0;        // Fused compare-and-jump
        int inc_dec_locals = 0;        // OP_INC_LOCAL_INT / OP_DEC_LOCAL_INT
        int const_folds = 0;           // Constant folding
        int tail_calls = 0;            // Tail call optimizations
        int dead_code = 0;             // Dead code bytes removed
        int superinstructions = 0;     // Multi-instruction fusions
    };
    
    Stats getStats() const { return stats_; }

private:
    Stats stats_;
    
    /**
     * Pass 1: Local variable shortcut instructions
     * Replace OP_GET_LOCAL 0 → OP_LOAD_LOCAL_0, etc.
     */
    void optimizeLocalAccess(Chunk& chunk);
    
    /**
     * Pass 2: Integer specialization
     * Detect numeric-only code paths and replace with int-specialized ops.
     * Analyzes local variable types through data flow.
     */
    void optimizeIntegerArithmetic(Chunk& chunk);
    
    /**
     * Pass 3: Fused compare-and-jump
     * Replace OP_LESS + OP_JUMP_IF_FALSE → OP_LESS_JUMP
     */
    void optimizeFusedCompareJump(Chunk& chunk);
    
    /**
     * Pass 4: Increment/decrement detection
     * Replace patterns like: GET_LOCAL(s) + CONST(1) + ADD + SET_LOCAL(s) + POP → INC_LOCAL_INT(s)
     */
    void optimizeIncrementDecrement(Chunk& chunk);
    
    /**
     * Pass 5: Constant integer shortcuts
     * Replace CONSTANT pool lookups for small integers with OP_CONST_INT8/ZERO/ONE
     */
    void optimizeConstantIntegers(Chunk& chunk);
    
    /**
     * Pass 6: Tail call optimization
     * Detect function calls immediately followed by OP_RETURN and convert to OP_TAIL_CALL
     */
    void optimizeTailCalls(Chunk& chunk);
    
    /**
     * Helper: Get the size of an instruction at the given offset
     */
    static int getInstructionSize(const Chunk& chunk, size_t offset);
    
    /**
     * Helper: Check if a constant at index is a numeric type
     */
    static bool isNumericConstant(const Chunk& chunk, uint8_t constIdx);
    
    /**
     * Helper: Check if a constant is a small integer (fits in int8)
     */
    static bool isSmallIntConstant(const Chunk& chunk, uint8_t constIdx, int8_t& outVal);
    
    /**
     * Helper: Rebuild the chunk from a modified instruction stream
     * Adjusts all jump offsets after instructions have been resized.
     */
    void rebuildChunk(Chunk& chunk, const std::vector<uint8_t>& newCode,
                      const std::vector<int>& newLines);
    
    /**
     * Helper: Fix jump targets after erasing bytes from the code
     */
    static void fixJumpsAfterErase(Chunk& chunk, size_t erasePos, size_t eraseCount);
};

} // namespace neutron

#endif // NEUTRON_BYTECODE_OPTIMIZER_H
