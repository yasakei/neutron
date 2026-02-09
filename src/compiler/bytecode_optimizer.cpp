/*
 * Neutron Hybrid Compiler - Bytecode Optimizer Implementation
 * Copyright (c) 2026 yasakei
 *
 * This is the core of the hybrid compiler approach. After the AST→bytecode
 * compiler generates standard bytecode, this optimizer transforms it into
 * specialized instructions that bypass runtime type checks.
 *
 * The key insight: in most Neutron programs, types are stable within a function.
 * A variable initialized as a number stays a number. By detecting these patterns
 * at compile time (or after first execution via profiling), we can emit opcodes
 * that assume the type and skip the check entirely.
 */

#include "compiler/bytecode_optimizer.h"
#include "types/value.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace neutron {

// ============================================================================
// Instruction size helper (same as in jit_tier2.cpp)
// ============================================================================
int BytecodeOptimizer::getInstructionSize(const Chunk& chunk, size_t offset) {
    if (offset >= chunk.code.size()) return 1;
    uint8_t opcode = chunk.code[offset];
    switch ((OpCode)opcode) {
        case OpCode::OP_TRY:
            return 7;
        case OpCode::OP_JUMP:
        case OpCode::OP_JUMP_IF_FALSE:
        case OpCode::OP_LOOP:
        case OpCode::OP_CONSTANT_LONG:
        case OpCode::OP_SET_LOCAL_TYPED:
        case OpCode::OP_DEFINE_TYPED_GLOBAL:
        case OpCode::OP_INCREMENT_LOCAL:
        case OpCode::OP_ADD_LOCAL_CONST:
        case OpCode::OP_LESS_JUMP:
        case OpCode::OP_GREATER_JUMP:
        case OpCode::OP_EQUAL_JUMP:
            return 3;
        case OpCode::OP_CONSTANT:
        case OpCode::OP_GET_LOCAL:
        case OpCode::OP_SET_LOCAL:
        case OpCode::OP_GET_GLOBAL:
        case OpCode::OP_SET_GLOBAL:
        case OpCode::OP_DEFINE_GLOBAL:
        case OpCode::OP_SET_GLOBAL_TYPED:
        case OpCode::OP_CALL:
        case OpCode::OP_CALL_FAST:
        case OpCode::OP_TAIL_CALL:
        case OpCode::OP_GET_UPVALUE:
        case OpCode::OP_SET_UPVALUE:
        case OpCode::OP_GET_PROPERTY:
        case OpCode::OP_SET_PROPERTY:
        case OpCode::OP_CLOSURE:
        case OpCode::OP_ARRAY:
        case OpCode::OP_OBJECT:
        case OpCode::OP_VALIDATE_SAFE_VARIABLE:
        case OpCode::OP_VALIDATE_SAFE_FILE_VARIABLE:
        case OpCode::OP_GET_GLOBAL_FAST:
        case OpCode::OP_SET_GLOBAL_FAST:
        case OpCode::OP_INC_LOCAL_INT:
        case OpCode::OP_DEC_LOCAL_INT:
        case OpCode::OP_CONST_INT8:
        case OpCode::OP_TYPE_GUARD:
            return 2;
        default:
            return 1;
    }
}

bool BytecodeOptimizer::isNumericConstant(const Chunk& chunk, uint8_t constIdx) {
    if (constIdx >= chunk.constants.size()) return false;
    return chunk.constants[constIdx].type == ValueType::NUMBER;
}

bool BytecodeOptimizer::isSmallIntConstant(const Chunk& chunk, uint8_t constIdx, int8_t& outVal) {
    if (constIdx >= chunk.constants.size()) return false;
    const Value& v = chunk.constants[constIdx];
    if (v.type != ValueType::NUMBER) return false;
    double d = v.as.number;
    double intpart;
    if (std::modf(d, &intpart) != 0.0) return false; // Not an integer
    if (d < -128.0 || d > 127.0) return false; // Doesn't fit in int8
    outVal = static_cast<int8_t>(d);
    return true;
}

// ============================================================================
// Main optimization entry point
// ============================================================================
void BytecodeOptimizer::optimize(Chunk& chunk) {
    stats_ = {};
    
    if (chunk.code.empty()) return;
    
    // Re-enabled: INT opcode handlers now use in-place references
    // (no 16-byte Value copies), making them faster than generic handlers.
    // Fused opcodes save dispatch overhead (1 goto per fused pair).
    
    // Pass 1: Constant integer shortcuts (same-size replacement, no jump fixup needed)
    optimizeConstantIntegers(chunk);
    
    // Pass 2: Integer specialization inside loops (same-size replacement)
    optimizeIntegerArithmetic(chunk);
    
    // Pass 3: Fused compare-and-jump (erases 1 byte per fusion)
    optimizeFusedCompareJump(chunk);
    
    // Pass 4: Increment/decrement detection (erases multiple bytes)
    optimizeIncrementDecrement(chunk);
    
    // Pass 5: Local variable access shortcuts (erases 1 byte per shortcut)
    optimizeLocalAccess(chunk);
    
    // Pass 6: Tail call optimization (same-size replacement)
    optimizeTailCalls(chunk);
}

// ============================================================================
// Pass 1: Local variable access shortcuts
// ============================================================================
void BytecodeOptimizer::optimizeLocalAccess(Chunk& chunk) {
    for (size_t i = 0; i + 1 < chunk.code.size(); ) {
        uint8_t op = chunk.code[i];
        if (op == (uint8_t)OpCode::OP_GET_LOCAL) {
            uint8_t slot = chunk.code[i + 1];
            if (slot <= 3) {
                // Replace with LOAD_LOCAL_0..3 (1-byte instruction)
                switch (slot) {
                    case 0: chunk.code[i] = (uint8_t)OpCode::OP_LOAD_LOCAL_0; break;
                    case 1: chunk.code[i] = (uint8_t)OpCode::OP_LOAD_LOCAL_1; break;
                    case 2: chunk.code[i] = (uint8_t)OpCode::OP_LOAD_LOCAL_2; break;
                    case 3: chunk.code[i] = (uint8_t)OpCode::OP_LOAD_LOCAL_3; break;
                }
                // Remove the operand byte by shifting everything after it left
                chunk.code.erase(chunk.code.begin() + i + 1);
                chunk.lines.erase(chunk.lines.begin() + i + 1);
                
                // Fix up all jump targets that cross this point
                fixJumpsAfterErase(chunk, i + 1, 1);
                
                stats_.local_shortcuts++;
                i += 1; // Now a 1-byte instruction
                continue;
            }
        }
        i += getInstructionSize(chunk, i);
    }
}

// ============================================================================
// Pass 2: Constant integer shortcuts
// ============================================================================
void BytecodeOptimizer::optimizeConstantIntegers(Chunk& chunk) {
    for (size_t i = 0; i + 1 < chunk.code.size(); ) {
        uint8_t op = chunk.code[i];
        if (op == (uint8_t)OpCode::OP_CONSTANT) {
            uint8_t constIdx = chunk.code[i + 1];
            int8_t smallVal;
            if (isSmallIntConstant(chunk, constIdx, smallVal)) {
                // Only do same-size replacement: OP_CONSTANT(2 bytes) → OP_CONST_INT8(2 bytes)
                // This avoids erasing bytes which would corrupt jump offsets.
                chunk.code[i] = (uint8_t)OpCode::OP_CONST_INT8;
                chunk.code[i + 1] = static_cast<uint8_t>(smallVal);
                stats_.const_folds++;
            }
        }
        i += getInstructionSize(chunk, i);
    }
}

// ============================================================================
// Pass 3: Increment/decrement detection
// Pattern: GET_LOCAL(s) + CONST(1) + ADD + SET_LOCAL(s) + POP
// Result:  INC_LOCAL_INT(s)  (or DEC for SUBTRACT)
// ============================================================================
void BytecodeOptimizer::optimizeIncrementDecrement(Chunk& chunk) {
    // Look for: GET_LOCAL(s), CONST(1|ONE), ADD/SUB, SET_LOCAL(s), POP
    // These come from i = i + 1 or i += 1 patterns
    for (size_t i = 0; i + 6 < chunk.code.size(); ) {
        uint8_t op0 = chunk.code[i];
        int instrSize = getInstructionSize(chunk, i);
        
        // Check for GET_LOCAL or LOAD_LOCAL_X
        int slot = -1;
        int loadSize = 0;
        if (op0 == (uint8_t)OpCode::OP_GET_LOCAL) {
            slot = chunk.code[i + 1];
            loadSize = 2;
        } else if (op0 == (uint8_t)OpCode::OP_LOAD_LOCAL_0) { slot = 0; loadSize = 1; }
        else if (op0 == (uint8_t)OpCode::OP_LOAD_LOCAL_1) { slot = 1; loadSize = 1; }
        else if (op0 == (uint8_t)OpCode::OP_LOAD_LOCAL_2) { slot = 2; loadSize = 1; }
        else if (op0 == (uint8_t)OpCode::OP_LOAD_LOCAL_3) { slot = 3; loadSize = 1; }
        
        if (slot >= 0) {
            size_t j = i + loadSize;
            if (j >= chunk.code.size()) { i += instrSize; continue; }
            
            // Check for constant 1 (various encodings)
            bool isOne = false;
            int constSize = 0;
            if (chunk.code[j] == (uint8_t)OpCode::OP_CONST_ONE) {
                isOne = true; constSize = 1;
            } else if (chunk.code[j] == (uint8_t)OpCode::OP_CONST_INT8 && j + 1 < chunk.code.size() && chunk.code[j + 1] == 1) {
                isOne = true; constSize = 2;
            } else if (chunk.code[j] == (uint8_t)OpCode::OP_CONSTANT && j + 1 < chunk.code.size()) {
                int8_t sv;
                if (isSmallIntConstant(chunk, chunk.code[j + 1], sv) && sv == 1) {
                    isOne = true; constSize = 2;
                }
            }
            
            if (isOne) {
                size_t k = j + constSize;
                if (k + 2 < chunk.code.size()) {
                    uint8_t arithOp = chunk.code[k];
                    bool isAdd = (arithOp == (uint8_t)OpCode::OP_ADD || 
                                  arithOp == (uint8_t)OpCode::OP_ADD_INT);
                    bool isSub = (arithOp == (uint8_t)OpCode::OP_SUBTRACT || 
                                  arithOp == (uint8_t)OpCode::OP_SUB_INT);
                    
                    if ((isAdd || isSub) && 
                        chunk.code[k + 1] == (uint8_t)OpCode::OP_SET_LOCAL &&
                        k + 2 < chunk.code.size() &&
                        chunk.code[k + 2] == (uint8_t)slot) {
                        
                        // Check for POP after SET_LOCAL
                        size_t popPos = k + 3;
                        if (popPos < chunk.code.size() && 
                            chunk.code[popPos] == (uint8_t)OpCode::OP_POP) {
                            
                            // Replace entire sequence with INC_LOCAL_INT or DEC_LOCAL_INT
                            size_t totalSize = popPos + 1 - i;
                            
                            // Replace in-place
                            chunk.code[i] = isAdd ? (uint8_t)OpCode::OP_INC_LOCAL_INT 
                                                   : (uint8_t)OpCode::OP_DEC_LOCAL_INT;
                            chunk.code[i + 1] = (uint8_t)slot;
                            
                            // Erase remaining bytes
                            size_t eraseCount = totalSize - 2;
                            chunk.code.erase(chunk.code.begin() + i + 2, 
                                            chunk.code.begin() + i + 2 + eraseCount);
                            chunk.lines.erase(chunk.lines.begin() + i + 2,
                                             chunk.lines.begin() + i + 2 + eraseCount);
                            fixJumpsAfterErase(chunk, i + 2, eraseCount);
                            
                            stats_.inc_dec_locals++;
                            i += 2;
                            continue;
                        }
                    }
                }
            }
        }
        
        i += instrSize;
    }
}

// ============================================================================
// Pass 4: Integer specialization
// Replace ADD/SUB/MUL/LESS/GREATER/EQUAL with INT variants when we can
// prove operands are numeric (from constant analysis or local type tracking).
// ============================================================================
void BytecodeOptimizer::optimizeIntegerArithmetic(Chunk& chunk) {
    // Simple analysis: scan for loops (OP_LOOP instructions) and check
    // if all variables used in the loop are numeric.
    // For safety, only specialize arithmetic that's inside loops (hot paths).
    
    // First pass: find all loop boundaries
    struct LoopInfo {
        size_t start;   // Target of the backward jump (loop header)
        size_t end;     // Position of OP_LOOP instruction
    };
    std::vector<LoopInfo> loops;
    
    for (size_t i = 0; i < chunk.code.size(); ) {
        uint8_t op = chunk.code[i];
        int size = getInstructionSize(chunk, i);
        if (op == (uint8_t)OpCode::OP_LOOP && i + 2 < chunk.code.size()) {
            uint16_t offset = (chunk.code[i + 1] << 8) | chunk.code[i + 2];
            size_t loopStart = i + size - offset;
            loops.push_back({loopStart, i});
        }
        i += size;
    }
    
    // For each loop, determine if all arithmetic is on numeric values
    for (const auto& loop : loops) {
        // Scan the loop body to check if any non-numeric operations exist
        bool allNumeric = true;
        bool hasArithmetic = false;
        
        for (size_t i = loop.start; i < loop.end && i < chunk.code.size(); ) {
            uint8_t op = chunk.code[i];
            int size = getInstructionSize(chunk, i);
            
            switch ((OpCode)op) {
                case OpCode::OP_ADD:
                case OpCode::OP_SUBTRACT:
                case OpCode::OP_MULTIPLY:
                case OpCode::OP_DIVIDE:
                case OpCode::OP_MODULO:
                case OpCode::OP_LESS:
                case OpCode::OP_GREATER:
                case OpCode::OP_EQUAL:
                case OpCode::OP_NOT_EQUAL:
                case OpCode::OP_NEGATE:
                    hasArithmetic = true;
                    break;
                    
                // Operations that introduce non-numeric values
                case OpCode::OP_GET_PROPERTY:
                case OpCode::OP_SET_PROPERTY:
                case OpCode::OP_ARRAY:
                case OpCode::OP_OBJECT:
                case OpCode::OP_INDEX_GET:
                case OpCode::OP_INDEX_SET:
                case OpCode::OP_CALL:
                case OpCode::OP_CALL_FAST:
                    allNumeric = false;
                    break;
                    
                case OpCode::OP_CONSTANT:
                    if (i + 1 < chunk.code.size()) {
                        if (!isNumericConstant(chunk, chunk.code[i + 1])) {
                            allNumeric = false;
                        }
                    }
                    break;
                    
                default:
                    break;
            }
            
            if (!allNumeric) break;
            i += size;
        }
        
        // If the loop is all-numeric, specialize the arithmetic
        if (allNumeric && hasArithmetic) {
            for (size_t i = loop.start; i < loop.end && i < chunk.code.size(); ) {
                uint8_t op = chunk.code[i];
                int size = getInstructionSize(chunk, i);
                
                switch ((OpCode)op) {
                    case OpCode::OP_ADD:
                        chunk.code[i] = (uint8_t)OpCode::OP_ADD_INT;
                        stats_.int_specializations++;
                        break;
                    case OpCode::OP_SUBTRACT:
                        chunk.code[i] = (uint8_t)OpCode::OP_SUB_INT;
                        stats_.int_specializations++;
                        break;
                    case OpCode::OP_MULTIPLY:
                        chunk.code[i] = (uint8_t)OpCode::OP_MUL_INT;
                        stats_.int_specializations++;
                        break;
                    case OpCode::OP_DIVIDE:
                        chunk.code[i] = (uint8_t)OpCode::OP_DIV_INT;
                        stats_.int_specializations++;
                        break;
                    case OpCode::OP_MODULO:
                        chunk.code[i] = (uint8_t)OpCode::OP_MOD_INT;
                        stats_.int_specializations++;
                        break;
                    case OpCode::OP_LESS:
                        chunk.code[i] = (uint8_t)OpCode::OP_LESS_INT;
                        stats_.int_specializations++;
                        break;
                    case OpCode::OP_GREATER:
                        chunk.code[i] = (uint8_t)OpCode::OP_GREATER_INT;
                        stats_.int_specializations++;
                        break;
                    case OpCode::OP_EQUAL:
                        chunk.code[i] = (uint8_t)OpCode::OP_EQUAL_INT;
                        stats_.int_specializations++;
                        break;
                    case OpCode::OP_NEGATE:
                        chunk.code[i] = (uint8_t)OpCode::OP_NEGATE_INT;
                        stats_.int_specializations++;
                        break;
                    default:
                        break;
                }
                
                i += size;
            }
        }
    }
}

// ============================================================================
// Pass 5: Fused compare-and-jump
// Pattern: OP_LESS + OP_JUMP_IF_FALSE → OP_LESS_JUMP (saves 1 dispatch)
// ============================================================================
void BytecodeOptimizer::optimizeFusedCompareJump(Chunk& chunk) {
    for (size_t i = 0; i + 3 < chunk.code.size(); ) {
        uint8_t op = chunk.code[i];
        int size = getInstructionSize(chunk, i);
        
        // Check for COMPARE + JUMP_IF_FALSE pattern
        if (i + size < chunk.code.size() && 
            chunk.code[i + size] == (uint8_t)OpCode::OP_JUMP_IF_FALSE) {
            
            OpCode fusedOp;
            bool canFuse = false;
            
            switch ((OpCode)op) {
                case OpCode::OP_LESS:
                case OpCode::OP_LESS_INT:
                    fusedOp = OpCode::OP_LESS_JUMP;
                    canFuse = true;
                    break;
                case OpCode::OP_GREATER:
                case OpCode::OP_GREATER_INT:
                    fusedOp = OpCode::OP_GREATER_JUMP;
                    canFuse = true;
                    break;
                case OpCode::OP_EQUAL:
                case OpCode::OP_EQUAL_INT:
                    fusedOp = OpCode::OP_EQUAL_JUMP;
                    canFuse = true;
                    break;
                default:
                    break;
            }
            
            if (canFuse) {
                // The JUMP_IF_FALSE has a 2-byte offset
                uint16_t jumpOffset = (chunk.code[i + size + 1] << 8) | chunk.code[i + size + 2];
                
                // Replace COMPARE with fused instruction
                chunk.code[i] = (uint8_t)fusedOp;
                
                // The fused instruction is 3 bytes: opcode + 2-byte offset
                // The COMPARE was 1 byte, JUMP_IF_FALSE was 3 bytes = 4 total
                // We need to become 3 bytes (save 1 byte)
                
                // Write the offset right after the fused opcode
                chunk.code[i + 1] = (jumpOffset >> 8) & 0xFF;
                chunk.code[i + 2] = jumpOffset & 0xFF;
                
                // Erase the JUMP_IF_FALSE instruction (was at i+size, now redundant)
                // The JUMP_IF_FALSE was at position i+1 (since compare was 1 byte)
                // After writing fused op at i with 2-byte offset, we need to remove
                // the original JUMP_IF_FALSE at position i+1+2 = i+3
                // But we already wrote the offset there, so erase bytes at i+3
                
                // Actually: compare was 1 byte at [i]. JUMP_IF_FALSE was at [i+1].
                // We turned [i] into fused_op, [i+1],[i+2] into offset.
                // The original JIF was [i+1]=JIF, [i+2],[i+3] = offset bytes.
                // So we need to erase [i+3] (the extra byte).
                // Wait — let me recalculate:
                // Original: [i]=LESS(1 byte), [i+1]=JIF, [i+2]=hi, [i+3]=lo
                // New:      [i]=LESS_JUMP,    [i+1]=hi,  [i+2]=lo
                // So erase byte at position i+3
                
                if (i + 3 < chunk.code.size()) {
                    chunk.code.erase(chunk.code.begin() + i + 3);
                    chunk.lines.erase(chunk.lines.begin() + i + 3);
                    fixJumpsAfterErase(chunk, i + 3, 1);
                }
                
                stats_.fused_compares++;
                i += 3;
                continue;
            }
        }
        
        i += size;
    }
}

// ============================================================================
// Pass 6: Tail call optimization
// Pattern: OP_CALL(n) + OP_RETURN → OP_TAIL_CALL(n)
// ============================================================================
void BytecodeOptimizer::optimizeTailCalls(Chunk& chunk) {
    for (size_t i = 0; i + 3 < chunk.code.size(); ) {
        uint8_t op = chunk.code[i];
        int size = getInstructionSize(chunk, i);
        
        if ((op == (uint8_t)OpCode::OP_CALL || op == (uint8_t)OpCode::OP_CALL_FAST) &&
            i + size + 1 < chunk.code.size()) {
            
            // Check if the next meaningful instruction is OP_RETURN
            // (there might be a POP of nil before it from the implicit return)
            size_t nextPos = i + size;
            
            // Skip OP_POP if present (assignment result of call expression)
            // Actually, for tail call, the call must be the last thing before return
            if (chunk.code[nextPos] == (uint8_t)OpCode::OP_RETURN) {
                // Direct tail call
                chunk.code[i] = (uint8_t)OpCode::OP_TAIL_CALL;
                stats_.tail_calls++;
            }
            // Also check: CALL + POP + NIL + RETURN (statement call at end of function)
            else if (nextPos + 3 < chunk.code.size() &&
                     chunk.code[nextPos] == (uint8_t)OpCode::OP_POP &&
                     chunk.code[nextPos + 1] == (uint8_t)OpCode::OP_NIL &&
                     chunk.code[nextPos + 2] == (uint8_t)OpCode::OP_RETURN) {
                chunk.code[i] = (uint8_t)OpCode::OP_TAIL_CALL;
                stats_.tail_calls++;
            }
        }
        
        i += size;
    }
}

// ============================================================================
// Helper: Fix jump targets after erasing bytes from the code
// ============================================================================
void BytecodeOptimizer::fixJumpsAfterErase(Chunk& chunk, size_t erasePos, size_t eraseCount) {
    // Scan all instructions and adjust any jump offsets that cross the erased region
    for (size_t i = 0; i < chunk.code.size(); ) {
        uint8_t op = chunk.code[i];
        int size = BytecodeOptimizer::getInstructionSize(chunk, i);
        
        switch ((OpCode)op) {
            case OpCode::OP_JUMP:
            case OpCode::OP_JUMP_IF_FALSE:
            case OpCode::OP_LESS_JUMP:
            case OpCode::OP_GREATER_JUMP:
            case OpCode::OP_EQUAL_JUMP:
            {
                if (i + 2 < chunk.code.size()) {
                    uint16_t offset = (chunk.code[i + 1] << 8) | chunk.code[i + 2];
                    size_t jumpTarget = i + size + offset;
                    
                    // If the jump crosses the erase point, adjust
                    if (i < erasePos && jumpTarget >= erasePos) {
                        // Forward jump crossing erase point
                        if (offset >= eraseCount) {
                            offset -= eraseCount;
                            chunk.code[i + 1] = (offset >> 8) & 0xFF;
                            chunk.code[i + 2] = offset & 0xFF;
                        }
                    }
                }
                break;
            }
            case OpCode::OP_LOOP:
            {
                if (i + 2 < chunk.code.size()) {
                    uint16_t offset = (chunk.code[i + 1] << 8) | chunk.code[i + 2];
                    size_t loopTarget = i + size - offset;
                    
                    // If the loop backward jump crosses the erase point
                    if (i >= erasePos && loopTarget < erasePos) {
                        // The target is before the erase, so loop offset needs to shrink
                        if (offset >= eraseCount) {
                            offset -= eraseCount;
                            chunk.code[i + 1] = (offset >> 8) & 0xFF;
                            chunk.code[i + 2] = offset & 0xFF;
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
        
        i += size;
    }
}

} // namespace neutron
