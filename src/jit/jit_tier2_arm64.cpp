// ARM64 (AArch64) JIT trace compiler
// Only compiled on ARM64 targets — x86_64 code is untouched
#if defined(__aarch64__) || defined(__arm64__)

#include "types/value.h"
#include "types/obj_string.h"
#include "jit/jit_codegen_arm64.h"
#include "jit/jit_tier2.h"
#include <cstring>
#include <algorithm>
#include <unordered_set>

namespace neutron::jit {

/**
 * ARM64 register mapping:
 *   X0       = first argument (ExecutionFrame*), also temp
 *   X1       = temp (constant loading, bitwise operand B)
 *   X9       = temp (for integer operations — replaces RAX role on x86_64)
 *   X19      = locals pointer (callee-saved, = RBX on x86_64)
 *   X20      = ExecutionFrame* (callee-saved, = RBP on x86_64)
 *   X21-X24  = global register cache (callee-saved, = R12-R15 on x86_64)
 *   D0-D14   = operand stack (D8-D14 are callee-saved, saved in prologue)
 *   D15      = scratch FP register
 *
 * Value layout: { ValueType type (8 bytes), union as (8 bytes) }
 *   offset 0: type
 *   offset 8: as.number (double)
 *   sizeof(Value) = 16
 */

uint64_t Tier2Compiler::compileTraceARM64(const ExecutionTrace& trace) {
    using CG = AArch64CodeGen;
    
    // Analyze trace to find which local slots are read/written
    std::unordered_set<uint32_t> written_locals;
    std::unordered_set<uint32_t> read_locals;
    for (const auto& instr : trace.ir_instructions) {
        if (instr.opcode == IRInstruction::Opcode::STORE_LOCAL) {
            written_locals.insert(instr.operand1);
        }
        if (instr.opcode == IRInstruction::Opcode::LOAD_LOCAL) {
            read_locals.insert(instr.operand1);
        }
    }

    // =====================================================================
    // GLOBAL REGISTER CACHING: Assign top 4 most-accessed globals to X21-X24
    // =====================================================================
    std::unordered_map<void*, int> global_access_count;
    for (const auto& instr : trace.ir_instructions) {
        if ((instr.opcode == IRInstruction::Opcode::LOAD_GLOBAL ||
             instr.opcode == IRInstruction::Opcode::STORE_GLOBAL) && instr.data) {
            global_access_count[instr.data]++;
        }
    }
    
    std::unordered_map<void*, uint8_t> global_reg_cache;
    if (!global_access_count.empty()) {
        std::vector<std::pair<void*, int>> sorted_globals(
            global_access_count.begin(), global_access_count.end());
        std::sort(sorted_globals.begin(), sorted_globals.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        constexpr uint8_t cache_regs[] = {21, 22, 23, 24}; // X21-X24
        size_t cache_count = sorted_globals.size() < 4 ? sorted_globals.size() : 4;
        for (size_t i = 0; i < cache_count; i++) {
            global_reg_cache[sorted_globals[i].first] = cache_regs[i];
        }
    }

    std::vector<uint8_t> code;

    // =====================================================================
    // PROLOGUE: Save callee-saved registers
    // =====================================================================
    // ARM64 AAPCS64: X19-X28, D8-D15 are callee-saved. X30(LR) must be saved.
    
    // Save LR + X19-X24 (GP callee-saved we use)
    CG::emitStpPre(code, 30, 19, 31, -16);   // STP X30, X19, [SP, #-16]!
    CG::emitStpPre(code, 20, 21, 31, -16);   // STP X20, X21, [SP, #-16]!
    CG::emitStpPre(code, 22, 23, 31, -16);   // STP X22, X23, [SP, #-16]!
    CG::emitStpPre(code, 24, 29, 31, -16);   // STP X24, X29, [SP, #-16]!
    
    // Save D8-D14 (FP callee-saved we use as operand stack)
    CG::emitStpFpPre(code, 8, 9, 31, -16);   // STP D8, D9, [SP, #-16]!
    CG::emitStpFpPre(code, 10, 11, 31, -16); // STP D10, D11, [SP, #-16]!
    CG::emitStpFpPre(code, 12, 13, 31, -16); // STP D12, D13, [SP, #-16]!
    CG::emitStpFpPre(code, 14, 15, 31, -16); // STP D14, D15, [SP, #-16]!
    
    // ExecutionFrame layout:
    //   offset  0: uint64_t method_id
    //   offset  8: const Chunk* chunk  
    //   offset 16: uint64_t bytecode_pc
    //   offset 24: void* stack_pointer
    //   offset 32: void* local_variables
    
    // X0 = first argument = ExecutionFrame*
    CG::emitMovReg64(code, 20, 0);            // X20 = X0 (save ExecutionFrame*)
    CG::emitLdrReg64(code, 19, 20, 32);       // X19 = [X20+32] = local_variables ptr
    
    // Load cached global addresses into X21-X24
    for (const auto& [addr, reg] : global_reg_cache) {
        CG::emitMovImm64(code, reg, reinterpret_cast<uint64_t>(addr));
    }
    
    // X19 = Value* locals, X20 = ExecutionFrame*
    // X21-X24 = cached global Value* addresses
    // D0..D14 = operand stack (max 15 slots)
    
    int stack_depth = 0;
    
    // =====================================================================
    // LOOP START
    // =====================================================================
    size_t loop_start_offset = code.size();
    std::vector<size_t> exit_jumps;

    // Track last comparison for correct B.cond encoding
    // 0 = LESS, 1 = GREATER, 2 = EQUAL, 3 = NOT_EQUAL
    int last_comparison = 0;
    
    // Integer register cache (X9 holds int64 of a D-register)
    int int_cached_dreg = -1;
    bool int_dreg_stale = false;
    
    auto materialize_int_to_double = [&]() {
        if (int_dreg_stale && int_cached_dreg >= 0) {
            uint8_t dreg = static_cast<uint8_t>(int_cached_dreg);
            // SCVTF Dd, X9
            CG::emitScvtfDX(code, dreg, 9);
            int_dreg_stale = false;
        }
    };
    
    // Forward jump tracking
    struct ForwardJump {
        size_t patch_offset;
        uint32_t target_ir;
        uint8_t cond;  // condition code for B.cond, or 0xFF for unconditional B
    };
    std::vector<ForwardJump> forward_jumps;
    bool seen_first_jif = false;

    // =====================================================================
    // BODY: Emit ARM64 code for each IR instruction
    // =====================================================================
    for (size_t ir_idx = 0; ir_idx < trace.ir_instructions.size(); ++ir_idx) {
        const auto& instr = trace.ir_instructions[ir_idx];
        
        // Resolve forward jumps targeting this IR index
        for (auto it = forward_jumps.begin(); it != forward_jumps.end(); ) {
            if (it->target_ir == ir_idx) {
                int32_t diff = (int32_t)(code.size() - it->patch_offset);
                if (it->cond == 0xFF) {
                    CG::patchB(code, it->patch_offset, diff);
                } else {
                    CG::patchBCond(code, it->patch_offset, it->cond, diff);
                }
                it = forward_jumps.erase(it);
            } else {
                ++it;
            }
        }
        
        // Materialize deferred int→double conversion if needed
        if (int_dreg_stale) {
            switch (instr.opcode) {
                case IRInstruction::Opcode::BITWISE_AND:
                case IRInstruction::Opcode::BITWISE_OR:
                case IRInstruction::Opcode::BITWISE_XOR:
                case IRInstruction::Opcode::LEFT_SHIFT:
                case IRInstruction::Opcode::RIGHT_SHIFT:
                case IRInstruction::Opcode::BITWISE_NOT:
                case IRInstruction::Opcode::LOAD_CONST:
                case IRInstruction::Opcode::LOAD_LOCAL:
                case IRInstruction::Opcode::LOAD_GLOBAL:
                case IRInstruction::Opcode::POP:
                case IRInstruction::Opcode::GUARD_TYPE:
                case IRInstruction::Opcode::UNROLL_MARKER:
                    break;
                default:
                    materialize_int_to_double();
                    break;
            }
        }
        
        switch (instr.opcode) {
            
            case IRInstruction::Opcode::LOAD_LOCAL:
            {
                if (stack_depth < 15) {
                    if (int_cached_dreg == stack_depth) {
                        int_cached_dreg = -1;
                        int_dreg_stale = false;
                    }
                    // LDR Dd, [X19, #slot*16 + 8]
                    CG::emitLdrFp64(code, stack_depth, 19, instr.operand1 * 16 + 8);
                    stack_depth++;
                }
                break;
            }
            
            case IRInstruction::Opcode::STORE_LOCAL:
            {
                if (stack_depth > 0) {
                    // STR Dd, [X19, #slot*16 + 8]
                    CG::emitStrFp64(code, stack_depth - 1, 19, instr.operand1 * 16 + 8);
                }
                break;
            }
            
            case IRInstruction::Opcode::LOAD_GLOBAL:
            {
                if (stack_depth < 15 && instr.data) {
                    if (int_cached_dreg == stack_depth) {
                        int_cached_dreg = -1;
                        int_dreg_stale = false;
                    }
                    auto cache_it = global_reg_cache.find(instr.data);
                    if (cache_it != global_reg_cache.end()) {
                        // LDR Dd, [Xreg, #8]
                        CG::emitLdrFp64(code, stack_depth, cache_it->second, 8);
                    } else {
                        materialize_int_to_double();
                        // MOV X9, addr; LDR Dd, [X9, #8]
                        CG::emitMovImm64(code, 9, reinterpret_cast<uint64_t>(instr.data));
                        CG::emitLdrFp64(code, stack_depth, 9, 8);
                        int_cached_dreg = -1;
                        int_dreg_stale = false;
                    }
                    stack_depth++;
                }
                break;
            }
            
            case IRInstruction::Opcode::STORE_GLOBAL:
            {
                if (stack_depth > 0 && instr.data) {
                    auto cache_it = global_reg_cache.find(instr.data);
                    if (cache_it != global_reg_cache.end()) {
                        CG::emitStrFp64(code, stack_depth - 1, cache_it->second, 8);
                    } else {
                        // MOV X1, addr; STR Dd, [X1, #8]
                        CG::emitMovImm64(code, 1, reinterpret_cast<uint64_t>(instr.data));
                        CG::emitStrFp64(code, stack_depth - 1, 1, 8);
                    }
                }
                break;
            }
            
            case IRInstruction::Opcode::LOAD_CONST:
            {
                if (stack_depth < 15) {
                    double const_val = 0.0;
                    if (instr.data) {
                        const Value* val = static_cast<const Value*>(instr.data);
                        const_val = val->as.number;
                    }
                    
                    if (int_cached_dreg == stack_depth) int_cached_dreg = -1;
                    
                    // Load double bits into X1, then FMOV Dd, X1
                    uint64_t bits;
                    std::memcpy(&bits, &const_val, 8);
                    CG::emitMovImm64(code, 1, bits);
                    CG::emitFmovFromGP(code, stack_depth, 1);
                    
                    stack_depth++;
                }
                break;
            }
                
            case IRInstruction::Opcode::ADD:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    CG::emitFaddD(code, stack_depth - 1, stack_depth - 1, stack_depth);
                    if (int_cached_dreg == stack_depth - 1 || int_cached_dreg == stack_depth)
                        int_cached_dreg = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::SUBTRACT:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    CG::emitFsubD(code, stack_depth - 1, stack_depth - 1, stack_depth);
                    if (int_cached_dreg == stack_depth - 1 || int_cached_dreg == stack_depth)
                        int_cached_dreg = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::MULTIPLY:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    CG::emitFmulD(code, stack_depth - 1, stack_depth - 1, stack_depth);
                    if (int_cached_dreg == stack_depth - 1 || int_cached_dreg == stack_depth)
                        int_cached_dreg = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::DIVIDE:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    CG::emitFdivD(code, stack_depth - 1, stack_depth - 1, stack_depth);
                    if (int_cached_dreg == stack_depth - 1 || int_cached_dreg == stack_depth)
                        int_cached_dreg = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::MODULO:
            {
                // a % b = a - trunc(a/b) * b using D15 as scratch
                if (stack_depth >= 2) {
                    uint8_t a_reg = stack_depth - 2;
                    uint8_t b_reg = stack_depth - 1;
                    
                    // FDIV D15, Da, Db
                    CG::emitFdivD(code, 15, a_reg, b_reg);
                    // FCVTZS X9, D15  (truncate to int)
                    CG::emitFcvtzsXD(code, 9, 15);
                    // SCVTF D15, X9   (convert back to double)
                    CG::emitScvtfDX(code, 15, 9);
                    // FMUL D15, D15, Db  (trunc(a/b) * b)
                    CG::emitFmulD(code, 15, 15, b_reg);
                    // FSUB Da, Da, D15   (a - trunc(a/b)*b)
                    CG::emitFsubD(code, a_reg, a_reg, 15);
                    
                    stack_depth--;
                    int_cached_dreg = -1;
                }
                break;
            }
            
            // =========================================================
            // BITWISE OPERATIONS — with INTEGER REGISTER CACHE
            // Uses X9 for int64 value, X1 for operand B
            // =========================================================
            case IRInstruction::Opcode::BITWISE_AND:
            case IRInstruction::Opcode::BITWISE_OR:
            case IRInstruction::Opcode::BITWISE_XOR:
            case IRInstruction::Opcode::LEFT_SHIFT:
            case IRInstruction::Opcode::RIGHT_SHIFT:
            {
                if (stack_depth >= 2) {
                    uint8_t a_reg = stack_depth - 2;
                    uint8_t b_reg = stack_depth - 1;
                    
                    bool b_is_const = false;
                    int64_t b_const_int = 0;
                    if (ir_idx > 0) {
                        const auto& prev = trace.ir_instructions[ir_idx - 1];
                        if (prev.opcode == IRInstruction::Opcode::LOAD_CONST && prev.data) {
                            const Value* val = static_cast<const Value*>(prev.data);
                            double dval = val->as.number;
                            int64_t ival = static_cast<int64_t>(dval);
                            if (static_cast<double>(ival) == dval) {
                                b_is_const = true;
                                b_const_int = ival;
                                stack_depth--;
                                b_reg = stack_depth;
                                a_reg = stack_depth - 1;
                            }
                        }
                    }
                    
                    // Convert A to int in X9 (skip if cached)
                    if (int_cached_dreg == a_reg) {
                        // X9 already holds the value
                    } else {
                        CG::emitFcvtzsXD(code, 9, a_reg);
                    }
                    
                    // Convert/load B into X1
                    if (b_is_const) {
                        CG::emitMovImm64(code, 1, static_cast<uint64_t>(b_const_int));
                    } else if (int_cached_dreg == b_reg) {
                        CG::emitMovReg64(code, 1, 9);
                    } else {
                        CG::emitFcvtzsXD(code, 1, b_reg);
                    }
                    
                    // Perform operation: result in X9
                    switch (instr.opcode) {
                        case IRInstruction::Opcode::BITWISE_AND:
                            CG::emitAndReg64(code, 9, 9, 1);
                            break;
                        case IRInstruction::Opcode::BITWISE_OR:
                            CG::emitOrrReg64(code, 9, 9, 1);
                            break;
                        case IRInstruction::Opcode::BITWISE_XOR:
                            CG::emitEorReg64(code, 9, 9, 1);
                            break;
                        case IRInstruction::Opcode::LEFT_SHIFT:
                            CG::emitLslReg64(code, 9, 9, 1);
                            break;
                        case IRInstruction::Opcode::RIGHT_SHIFT:
                            CG::emitAsrReg64(code, 9, 9, 1);
                            break;
                        default: break;
                    }
                    
                    // Check if we can defer the int→double conversion
                    bool defer_conversion = false;
                    for (size_t look = ir_idx + 1; look < trace.ir_instructions.size(); look++) {
                        auto op = trace.ir_instructions[look].opcode;
                        if (op == IRInstruction::Opcode::BITWISE_AND ||
                            op == IRInstruction::Opcode::BITWISE_OR ||
                            op == IRInstruction::Opcode::BITWISE_XOR ||
                            op == IRInstruction::Opcode::LEFT_SHIFT ||
                            op == IRInstruction::Opcode::RIGHT_SHIFT ||
                            op == IRInstruction::Opcode::BITWISE_NOT) {
                            defer_conversion = true;
                            break;
                        }
                        if (op == IRInstruction::Opcode::STORE_GLOBAL ||
                            op == IRInstruction::Opcode::STORE_LOCAL ||
                            op == IRInstruction::Opcode::POP ||
                            op == IRInstruction::Opcode::LOOP_BACK ||
                            op == IRInstruction::Opcode::JUMP_IF_FALSE ||
                            op == IRInstruction::Opcode::JUMP) {
                            break;
                        }
                    }
                    
                    if (!defer_conversion) {
                        CG::emitScvtfDX(code, a_reg, 9);
                        int_dreg_stale = false;
                    } else {
                        int_dreg_stale = true;
                    }
                    
                    if (!b_is_const) {
                        stack_depth--;
                    }
                    int_cached_dreg = a_reg;
                }
                break;
            }
            
            case IRInstruction::Opcode::BITWISE_NOT:
            {
                if (stack_depth >= 1) {
                    uint8_t a_reg = stack_depth - 1;
                    
                    if (int_cached_dreg == a_reg) {
                        // X9 already has the value
                    } else {
                        CG::emitFcvtzsXD(code, 9, a_reg);
                    }
                    
                    // MVN X9, X9
                    CG::emitMvnReg64(code, 9, 9);
                    
                    bool defer_conversion = false;
                    for (size_t look = ir_idx + 1; look < trace.ir_instructions.size(); look++) {
                        auto op = trace.ir_instructions[look].opcode;
                        if (op == IRInstruction::Opcode::BITWISE_AND ||
                            op == IRInstruction::Opcode::BITWISE_OR ||
                            op == IRInstruction::Opcode::BITWISE_XOR ||
                            op == IRInstruction::Opcode::LEFT_SHIFT ||
                            op == IRInstruction::Opcode::RIGHT_SHIFT ||
                            op == IRInstruction::Opcode::BITWISE_NOT) {
                            defer_conversion = true;
                            break;
                        }
                        if (op == IRInstruction::Opcode::STORE_GLOBAL ||
                            op == IRInstruction::Opcode::STORE_LOCAL ||
                            op == IRInstruction::Opcode::POP ||
                            op == IRInstruction::Opcode::LOOP_BACK ||
                            op == IRInstruction::Opcode::JUMP_IF_FALSE ||
                            op == IRInstruction::Opcode::JUMP) {
                            break;
                        }
                    }
                    
                    if (!defer_conversion) {
                        CG::emitScvtfDX(code, a_reg, 9);
                        int_dreg_stale = false;
                    } else {
                        int_dreg_stale = true;
                    }
                    int_cached_dreg = a_reg;
                }
                break;
            }
            
            case IRInstruction::Opcode::NEGATE:
            {
                if (stack_depth >= 1) {
                    uint8_t a_reg = stack_depth - 1;
                    CG::emitFnegD(code, a_reg, a_reg);
                    if (int_cached_dreg == a_reg) int_cached_dreg = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::LESS:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    CG::emitFcmpD(code, stack_depth - 1, stack_depth);
                    stack_depth--;
                    last_comparison = 0;
                    if (int_cached_dreg >= stack_depth) int_cached_dreg = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::GREATER:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    CG::emitFcmpD(code, stack_depth - 1, stack_depth);
                    stack_depth--;
                    last_comparison = 1;
                    if (int_cached_dreg >= stack_depth) int_cached_dreg = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::EQUAL:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    CG::emitFcmpD(code, stack_depth - 1, stack_depth);
                    stack_depth--;
                    last_comparison = 2;
                    if (int_cached_dreg >= stack_depth) int_cached_dreg = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::NOT_EQUAL:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    CG::emitFcmpD(code, stack_depth - 1, stack_depth);
                    stack_depth--;
                    last_comparison = 3;
                    if (int_cached_dreg >= stack_depth) int_cached_dreg = -1;
                }
                break;
            }
                
            case IRInstruction::Opcode::JUMP_IF_FALSE:
            {
                int_cached_dreg = -1;
                
                // For FCMP, the "false" condition (i.e., jump when condition is NOT true):
                // LESS:      jump if NOT less      => B.GE (N=V, signed >=)
                // GREATER:   jump if NOT greater   => B.LE (Z=1 OR N≠V)
                // EQUAL:     jump if NOT equal     => B.NE
                // NOT_EQUAL: jump if NOT not-equal => B.EQ
                uint8_t cond;
                switch (last_comparison) {
                    case 0: cond = CG::COND_GE; break; // not less => >=
                    case 1: cond = CG::COND_LE; break; // not greater => <=
                    case 2: cond = CG::COND_NE; break; // not equal
                    case 3: cond = CG::COND_EQ; break; // not not-equal => equal
                    default: cond = CG::COND_GE; break;
                }
                
                if (!seen_first_jif) {
                    seen_first_jif = true;
                    exit_jumps.push_back(code.size());
                    CG::emitBCond(code, cond, 0); // placeholder
                } else {
                    uint32_t bytecodes_to_skip = instr.operand1;
                    uint32_t target_ir = ir_idx + 1;
                    int depth = 1;
                    for (size_t scan = ir_idx + 1; scan < trace.ir_instructions.size(); scan++) {
                        const auto& scan_instr = trace.ir_instructions[scan];
                        if (scan_instr.opcode == IRInstruction::Opcode::JUMP_IF_FALSE) {
                            depth++;
                        } else if (scan_instr.opcode == IRInstruction::Opcode::JUMP) {
                            depth--;
                            if (depth == 0) {
                                target_ir = scan + 1;
                                break;
                            }
                        }
                    }
                    if (depth != 0) {
                        uint32_t max_target = (uint32_t)(trace.ir_instructions.size());
                        uint32_t calc_target = (uint32_t)(ir_idx + bytecodes_to_skip);
                        target_ir = calc_target < max_target ? calc_target : max_target;
                    }
                    forward_jumps.push_back({code.size(), target_ir, cond});
                    CG::emitBCond(code, cond, 0); // placeholder
                }
                break;
            }
            
            case IRInstruction::Opcode::JUMP:
            {
                int_cached_dreg = -1;
                
                size_t jump_offset = code.size();
                CG::emitB(code, 0); // placeholder
                
                uint32_t bytecodes_to_skip = instr.operand1;
                uint32_t target_ir = ir_idx + 1;
                {
                    uint32_t max_target = (uint32_t)(trace.ir_instructions.size());
                    uint32_t calc_target = (uint32_t)(ir_idx + bytecodes_to_skip + 1);
                    target_ir = calc_target < max_target ? calc_target : max_target;
                }
                
                forward_jumps.push_back({jump_offset, target_ir, 0xFF}); // 0xFF = unconditional
                break;
            }
                
            case IRInstruction::Opcode::LOOP_BACK:
            {
                // Unconditional jump back to loop start
                int32_t diff = (int32_t)(loop_start_offset - code.size());
                CG::emitB(code, diff);
                break;
            }
            
            case IRInstruction::Opcode::POP:
            {
                if (stack_depth > 0) {
                    stack_depth--;
                }
                break;
            }
            
            case IRInstruction::Opcode::GUARD_TYPE:
            case IRInstruction::Opcode::UNROLL_MARKER:
                break;
                
            default:
                break;
        }
    }

    // =====================================================================
    // EXIT STUB: Patch exit jumps AND unresolved forward jumps to point here.
    // On ARM64, B with offset 0 = branch-to-self (infinite loop), so any
    // unresolved forward jump (e.g. from a "break" statement) MUST be
    // patched to jump to the epilogue.
    // =====================================================================
    for (size_t offset : exit_jumps) {
        // Read original instruction to get condition code
        uint32_t orig = code[offset] | (code[offset+1] << 8) | 
                        (code[offset+2] << 16) | (code[offset+3] << 24);
        uint8_t cond = orig & 0xF;
        int32_t diff = (int32_t)(code.size() - offset);
        CG::patchBCond(code, offset, cond, diff);
    }
    
    // Patch any remaining unresolved forward jumps (e.g. break → exit)
    for (const auto& fj : forward_jumps) {
        int32_t diff = (int32_t)(code.size() - fj.patch_offset);
        if (fj.cond == 0xFF) {
            CG::patchB(code, fj.patch_offset, diff);
        } else {
            CG::patchBCond(code, fj.patch_offset, fj.cond, diff);
        }
    }

    // =====================================================================
    // EPILOGUE: Restore callee-saved registers and return
    // =====================================================================
    // Restore in reverse order
    CG::emitLdpFpPost(code, 14, 15, 31, 16);  // LDP D14, D15, [SP], #16
    CG::emitLdpFpPost(code, 12, 13, 31, 16);  // LDP D12, D13, [SP], #16
    CG::emitLdpFpPost(code, 10, 11, 31, 16);  // LDP D10, D11, [SP], #16
    CG::emitLdpFpPost(code, 8, 9, 31, 16);    // LDP D8, D9, [SP], #16
    
    CG::emitLdpPost(code, 24, 29, 31, 16);    // LDP X24, X29, [SP], #16
    CG::emitLdpPost(code, 22, 23, 31, 16);    // LDP X22, X23, [SP], #16
    CG::emitLdpPost(code, 20, 21, 31, 16);    // LDP X20, X21, [SP], #16
    CG::emitLdpPost(code, 30, 19, 31, 16);    // LDP X30, X19, [SP], #16
    
    CG::emitRet(code);

    // =====================================================================
    // Copy generated code to executable memory
    // =====================================================================
    uint8_t* code_space = allocateCodeSpace(code.size());
    if (!code_space) {
        return 0;
    }
    
    code_cache_.makeWritable();
    std::memcpy(code_space, code.data(), code.size());
    code_cache_.makeExecutable();
    
    uint64_t code_addr = reinterpret_cast<uint64_t>(code_space);
    
    // Store compiled code address in trace
    for (auto& [tid, t] : traces_) {
        if (t->method_id == trace.method_id && 
            t->loop_entry_pc == trace.loop_entry_pc) {
            t->compiled_code_address = code_addr;
            t->compiled_code_size = code.size();
            uint64_t key = trace.method_id ^ (trace.loop_entry_pc * 2654435761ULL);
            compiled_trace_lookup_[key] = tid;
            return tid;
        }
    }
    
    return 0;
}

} // namespace neutron::jit

#endif // __aarch64__ || __arm64__
