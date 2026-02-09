#include "types/value.h"
#include "types/obj_string.h"
#include "jit/jit_codegen.h"
#include "jit/jit_tier2.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <chrono>
#include <cstring>
#include <algorithm>
#include <unordered_set>
#include <string>

namespace neutron::jit {

Tier2Compiler::Tier2Compiler()
    : code_cache_offset_(0), next_trace_id_(1),
      total_traces_(0), total_optimization_time_us_(0),
      code_cache_initialized_(false) {
    // Lazy init: don't allocate 50MB code cache until first trace compilation
}

Tier2Compiler::~Tier2Compiler() = default;

// Helper to determine instruction size
static inline uint16_t readU16BE(const std::vector<uint8_t>& code, uint64_t pc) {
    return static_cast<uint16_t>((code[pc] << 8) | code[pc + 1]);
}

static int getInstructionSize(uint8_t opcode) {
    switch ((OpCode)opcode) {
        // 7-byte instructions
        case OpCode::OP_TRY:
           return 7;

        // 5-byte instructions
        case OpCode::OP_LOOP_IF_LESS_LOCAL:
           return 5;

        // 3-byte instructions (opcode + 2-byte operand)
        case OpCode::OP_JUMP:
        case OpCode::OP_JUMP_IF_FALSE:
        case OpCode::OP_LOOP:
        case OpCode::OP_LESS_JUMP:
        case OpCode::OP_GREATER_JUMP:
        case OpCode::OP_EQUAL_JUMP:
        case OpCode::OP_CONSTANT_LONG:
        case OpCode::OP_SET_LOCAL_TYPED:
        case OpCode::OP_DEFINE_TYPED_GLOBAL:
        case OpCode::OP_ADD_LOCAL_CONST:
        case OpCode::OP_INVOKE:
           return 3;
           
        // 2-byte instructions (opcode + 1-byte operand)
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
        case OpCode::OP_INCREMENT_LOCAL:
        case OpCode::OP_DECREMENT_LOCAL:
        case OpCode::OP_INCREMENT_GLOBAL:
           return 2;
           
        // 1-byte instructions (opcode only)
        case OpCode::OP_CONST_ZERO:
        case OpCode::OP_CONST_ONE:
        case OpCode::OP_LOAD_LOCAL_0:
        case OpCode::OP_LOAD_LOCAL_1:
        case OpCode::OP_LOAD_LOCAL_2:
        case OpCode::OP_LOAD_LOCAL_3:
        case OpCode::OP_ADD_INT:
        case OpCode::OP_SUB_INT:
        case OpCode::OP_MUL_INT:
        case OpCode::OP_DIV_INT:
        case OpCode::OP_MOD_INT:
        case OpCode::OP_NEGATE_INT:
        case OpCode::OP_LESS_INT:
        case OpCode::OP_GREATER_INT:
        case OpCode::OP_EQUAL_INT:
        case OpCode::OP_LOOP_HINT:
        case OpCode::OP_COUNT:
        default:
           return 1;
    }
}

std::unique_ptr<Tier2Compiler::ExecutionTrace> Tier2Compiler::recordTrace(
    uint64_t method_id,
    const Chunk& bytecode,
    uint64_t start_pc,
    const HotSpotProfiler& profiler) {

    auto trace = std::make_unique<ExecutionTrace>();
    trace->trace_id = next_trace_id_++;
    trace->method_id = method_id;
    trace->loop_entry_pc = start_pc;
    trace->execution_count = 0;

    // Find loop boundaries
    // A loop starts at start_pc and ends when we hit OUR OP_LOOP.
    // Our OP_LOOP is the one whose backward jump target equals start_pc.
    // Other OP_LOOPs belong to inner loops and must be included in the trace
    // (they'll be rejected by convertToIR as unsupported opcodes).
    uint64_t end_pc = start_pc;
    
    for (uint64_t pc = start_pc; pc < bytecode.code.size() && 
         end_pc - start_pc < MAX_TRACE_LENGTH; ) {
        uint8_t opcode = bytecode.code[pc];
        int instr_size = getInstructionSize(opcode);
        
        // Check if this OP_LOOP jumps back to our start_pc
        if (opcode == static_cast<uint8_t>(OpCode::OP_LOOP)) {
            // Read the offset to determine the backward jump target
            uint16_t offset = readU16BE(bytecode.code, pc + 1);
            // The backward jump target is: (pc + instr_size) - offset
            uint64_t jump_target = (pc + instr_size) - offset;
            if (jump_target == start_pc) {
                // This is OUR loop's backward jump
                end_pc = pc + instr_size;
                break;
            }
            // Otherwise it's an inner loop — include it in the trace
            // (convertToIR will bail when it hits an unsupported inner loop)
        }
        
        // Stop at return (loop never completes)
        if (opcode == static_cast<uint8_t>(OpCode::OP_RETURN)) {
            end_pc = pc + instr_size;
            break;
        }
        
        pc += instr_size;
        end_pc = pc;
    }

    // Convert bytecode instructions in this range to IR
    trace->ir_instructions = convertToIR(bytecode, start_pc, end_pc);
    
    traces_[trace->trace_id] = std::make_unique<ExecutionTrace>(*trace);
    total_traces_++;

    return trace;
}

std::unique_ptr<Tier2Compiler::ExecutionTrace> 
Tier2Compiler::optimizeTrace(const ExecutionTrace& trace) {

    auto start_time = std::chrono::high_resolution_clock::now();
    auto optimized = std::make_unique<ExecutionTrace>(trace);

    // Apply optimizations in order of impact
    
    // 1. Type specialization (creates guards) - highest impact
    // Extract type information from profiler and add guards
    bool has_type_info = !trace.guard_conditions.empty();
    if (has_type_info) {
        // Add type guards at trace entry
        for (uint64_t value_id : trace.guard_conditions) {
            IRInstruction guard;
            guard.opcode = IRInstruction::Opcode::GUARD_TYPE;
            guard.operand1 = static_cast<uint32_t>(value_id & 0xFFFFFFFF);
            optimized->ir_instructions.insert(
                optimized->ir_instructions.begin(), guard);
        }
    }

    // 2. Loop unrolling for hot loops
    // Detect if this is a frequently executed loop
    if (trace.execution_count > TIER2_COMPILATION_THRESHOLD * 2) {
        auto unrolled = unrollLoop(*optimized, 2);
        if (unrolled && unrolled->ir_instructions.size() < MAX_TRACE_LENGTH) {
            optimized = std::move(unrolled);
        }
    }

    // 3. Constant folding and dead code elimination
    std::vector<IRInstruction> simplified;
    for (size_t i = 0; i < optimized->ir_instructions.size(); ++i) {
        const auto& instr = optimized->ir_instructions[i];
        
        // Skip dead instructions
        if (instr.opcode == IRInstruction::Opcode::UNROLL_MARKER) {
            continue;
        }
        
        // Constant folding: if two consecutive LOAD_CONST + operation
        if (i + 1 < optimized->ir_instructions.size() &&
            instr.opcode == IRInstruction::Opcode::LOAD_CONST &&
            optimized->ir_instructions[i + 1].opcode == IRInstruction::Opcode::LOAD_CONST) {
            
            // Could fold these constants, but for now just include them
        }
        
        simplified.push_back(instr);
    }
    optimized->ir_instructions = simplified;

    // 4. Common subexpression elimination
    // Track seen computations and reuse results
    std::unordered_set<uint64_t> seen_expressions;
    std::vector<IRInstruction> cse_optimized;
    
    for (const auto& instr : optimized->ir_instructions) {
        // Create a hash of the instruction
        uint64_t expr_hash = static_cast<uint64_t>(instr.opcode) * 73856093 ^
                             instr.operand1 * 19349663 ^
                             instr.operand2 * 83492791;
        
        // Check if we've seen this expression
        if (seen_expressions.count(expr_hash) > 0 &&
            (instr.opcode == IRInstruction::Opcode::ADD ||
             instr.opcode == IRInstruction::Opcode::MULTIPLY ||
             instr.opcode == IRInstruction::Opcode::EQUAL ||
             instr.opcode == IRInstruction::Opcode::LESS)) {
            // Could reuse result, but for safety include anyway
        }
        
        seen_expressions.insert(expr_hash);
        cse_optimized.push_back(instr);
    }
    optimized->ir_instructions = cse_optimized;

    // 5. Store-load forwarding: eliminate STORE_GLOBAL(X)/POP/LOAD_GLOBAL(X)
    //    Run until fixed point since each pass may expose new patterns.
    {
        bool changed = true;
        while (changed) {
            changed = false;
            std::vector<IRInstruction> forwarded;
            size_t n = optimized->ir_instructions.size();
            for (size_t i = 0; i < n; i++) {
                const auto& cur = optimized->ir_instructions[i];
                
                // Check for STORE_GLOBAL(X) / POP / LOAD_GLOBAL(X) pattern
                if (i + 2 < n &&
                    (cur.opcode == IRInstruction::Opcode::STORE_GLOBAL ||
                     cur.opcode == IRInstruction::Opcode::STORE_LOCAL) &&
                    optimized->ir_instructions[i + 1].opcode == IRInstruction::Opcode::POP) {
                    
                    const auto& load = optimized->ir_instructions[i + 2];
                    bool is_same = false;
                    
                    if (cur.opcode == IRInstruction::Opcode::STORE_GLOBAL &&
                        load.opcode == IRInstruction::Opcode::LOAD_GLOBAL &&
                        cur.data == load.data && cur.data != nullptr) {
                        is_same = true;
                    }
                    if (cur.opcode == IRInstruction::Opcode::STORE_LOCAL &&
                        load.opcode == IRInstruction::Opcode::LOAD_LOCAL &&
                        cur.operand1 == load.operand1) {
                        is_same = true;
                    }
                    
                    if (is_same) {
                        forwarded.push_back(cur);
                        i += 2; // Skip POP and LOAD
                        changed = true;
                        continue;
                    }
                }
                
                forwarded.push_back(cur);
            }
            optimized->ir_instructions = forwarded;
        }
    }

    // 6. Dead store elimination: remove redundant STORE_GLOBAL(X) when
    //    immediately followed by another STORE_GLOBAL(X) to the same location.
    //    Pattern: STORE_GLOBAL(X) / <ops that don't load X> / STORE_GLOBAL(X)
    //    Simplified: just eliminate STORE_GLOBAL(X) immediately followed by
    //    a computation that ends with STORE_GLOBAL(X) with no intervening LOAD_GLOBAL(X).
    {
        std::vector<IRInstruction> dse;
        size_t n = optimized->ir_instructions.size();
        for (size_t i = 0; i < n; i++) {
            const auto& cur = optimized->ir_instructions[i];
            
            // If this is STORE_GLOBAL, check if there's a later STORE_GLOBAL
            // to the same location before any LOAD_GLOBAL of the same location.
            if (cur.opcode == IRInstruction::Opcode::STORE_GLOBAL && cur.data) {
                bool is_dead = false;
                for (size_t j = i + 1; j < n; j++) {
                    const auto& next = optimized->ir_instructions[j];
                    if (next.opcode == IRInstruction::Opcode::STORE_GLOBAL &&
                        next.data == cur.data) {
                        is_dead = true; // Another store to same global before any read
                        break;
                    }
                    if (next.opcode == IRInstruction::Opcode::LOAD_GLOBAL &&
                        next.data == cur.data) {
                        break; // The value is read before next store, not dead
                    }
                    if (next.opcode == IRInstruction::Opcode::LOOP_BACK) {
                        break; // Don't look past loop boundary
                    }
                }
                if (is_dead) {
                    continue; // Skip this dead store
                }
            }
            
            dse.push_back(cur);
        }
        optimized->ir_instructions = dse;
    }

    // 7. Cross-statement value propagation: eliminate POP + LOAD_GLOBAL(X) when
    //    the value from STORE_GLOBAL(X) can stay on the XMM stack.
    //    Pattern: STORE_GLOBAL(X) / POP / <stuff> / LOAD_GLOBAL(X)
    //    where X is not stored again in <stuff> and the stack depth returns
    //    to the same level at the LOAD_GLOBAL(X) as it was after the POP.
    //    Transform: remove POP and LOAD_GLOBAL(X), keeping value on stack.
    {
        bool changed = true;
        while (changed) {
            changed = false;
            std::vector<IRInstruction> prop;
            size_t n = optimized->ir_instructions.size();
            
            for (size_t i = 0; i < n; i++) {
                const auto& cur = optimized->ir_instructions[i];
                
                // Look for STORE_GLOBAL(X) / POP pattern
                if (i + 1 < n &&
                    cur.opcode == IRInstruction::Opcode::STORE_GLOBAL && cur.data &&
                    optimized->ir_instructions[i + 1].opcode == IRInstruction::Opcode::POP) {
                    
                    // Scan forward to find matching LOAD_GLOBAL(X)
                    // Track stack depth changes to verify the value is accessible
                    int depth_delta = 0; // relative to after POP
                    bool found_load = false;
                    size_t load_idx = 0;
                    
                    for (size_t j = i + 2; j < n; j++) {
                        auto op = optimized->ir_instructions[j].opcode;
                        
                        // Check if this is the matching LOAD_GLOBAL(X)
                        if (op == IRInstruction::Opcode::LOAD_GLOBAL &&
                            optimized->ir_instructions[j].data == cur.data &&
                            depth_delta == 0) {
                            // Stack is at the same depth as after POP — the value
                            // from before the POP is now at the bottom, accessible
                            // as xmm[0] when depth_delta = 0.
                            found_load = true;
                            load_idx = j;
                            break;
                        }
                        
                        // Check for modification of X (would invalidate)
                        if (op == IRInstruction::Opcode::STORE_GLOBAL &&
                            optimized->ir_instructions[j].data == cur.data) {
                            break; // X was modified, can't forward
                        }
                        
                        // Don't cross loop boundaries
                        if (op == IRInstruction::Opcode::LOOP_BACK ||
                            op == IRInstruction::Opcode::JUMP ||
                            op == IRInstruction::Opcode::JUMP_IF_FALSE) {
                            break;
                        }
                        
                        // Track stack depth changes
                        switch (op) {
                            case IRInstruction::Opcode::LOAD_LOCAL:
                            case IRInstruction::Opcode::LOAD_GLOBAL:
                            case IRInstruction::Opcode::LOAD_CONST:
                                depth_delta++;
                                break;
                            case IRInstruction::Opcode::POP:
                                depth_delta--;
                                break;
                            case IRInstruction::Opcode::STORE_GLOBAL:
                            case IRInstruction::Opcode::STORE_LOCAL:
                                // Store doesn't change stack depth
                                break;
                            case IRInstruction::Opcode::ADD:
                            case IRInstruction::Opcode::SUBTRACT:
                            case IRInstruction::Opcode::MULTIPLY:
                            case IRInstruction::Opcode::DIVIDE:
                            case IRInstruction::Opcode::LESS:
                            case IRInstruction::Opcode::GREATER:
                            case IRInstruction::Opcode::EQUAL:
                            case IRInstruction::Opcode::BITWISE_AND:
                            case IRInstruction::Opcode::BITWISE_OR:
                            case IRInstruction::Opcode::BITWISE_XOR:
                            case IRInstruction::Opcode::LEFT_SHIFT:
                            case IRInstruction::Opcode::RIGHT_SHIFT:
                                depth_delta--; // Binary ops: pop 2, push 1
                                break;
                            case IRInstruction::Opcode::NEGATE:
                            case IRInstruction::Opcode::BITWISE_NOT:
                                // Unary: pop 1, push 1 = no change
                                break;
                            case IRInstruction::Opcode::GUARD_TYPE:
                            case IRInstruction::Opcode::UNROLL_MARKER:
                                // No stack effect
                                break;
                            default:
                                goto no_forward; // Unknown op, bail
                        }
                        
                        // Safety: don't let stack go negative (would underflow our saved value)
                        if (depth_delta < 0) break;
                    }
                    
                    if (found_load) {
                        // Success! Keep STORE_GLOBAL(X) but remove POP and LOAD_GLOBAL(X)
                        prop.push_back(cur); // STORE_GLOBAL(X) (value stays on stack)
                        i++; // Skip POP (i+1)
                        
                        // Copy instructions between POP and LOAD_GLOBAL(X), skipping LOAD_GLOBAL(X)
                        for (size_t j = i + 1; j < n; j++) {
                            if (j == load_idx) continue; // Skip LOAD_GLOBAL(X)
                            prop.push_back(optimized->ir_instructions[j]);
                        }
                        // We've processed all remaining instructions
                        i = n; // Skip outer loop
                        changed = true;
                        break;
                    }
                }
                no_forward:
                prop.push_back(cur);
            }
            optimized->ir_instructions = prop;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);

    total_optimization_time_us_ += duration.count();

    return optimized;
}

// This include is inside the namespace neutron::jit block!
// That is the problem. jit_codegen.h defines namespace neutron::jit.
// So we end up with neutron::jit::neutron::jit::X86_64CodeGen.
// We must move the include to the top.

uint64_t Tier2Compiler::compileTrace(const ExecutionTrace& trace) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // =====================================================================
    // VALIDATION: Only compile traces we can fully handle.
    // Reject traces with unsupported operations, nested loops,
    // or non-numeric constants.
    // =====================================================================
    auto markFailed = [this, &trace]() {
        failed_traces_.insert(trace.method_id ^ (trace.loop_entry_pc * 2654435761ULL));
    };
    
    int loop_back_count = 0;
    for (const auto& instr : trace.ir_instructions) {
        switch (instr.opcode) {
            case IRInstruction::Opcode::LOAD_LOCAL:
            case IRInstruction::Opcode::STORE_LOCAL:
            case IRInstruction::Opcode::ADD:
            case IRInstruction::Opcode::SUBTRACT:
            case IRInstruction::Opcode::MULTIPLY:
            case IRInstruction::Opcode::DIVIDE:
            case IRInstruction::Opcode::MODULO:
            case IRInstruction::Opcode::BITWISE_AND:
            case IRInstruction::Opcode::BITWISE_OR:
            case IRInstruction::Opcode::BITWISE_XOR:
            case IRInstruction::Opcode::BITWISE_NOT:
            case IRInstruction::Opcode::LEFT_SHIFT:
            case IRInstruction::Opcode::RIGHT_SHIFT:
            case IRInstruction::Opcode::NEGATE:
            case IRInstruction::Opcode::LESS:
            case IRInstruction::Opcode::GREATER:
            case IRInstruction::Opcode::EQUAL:
            case IRInstruction::Opcode::NOT_EQUAL:
            case IRInstruction::Opcode::JUMP_IF_FALSE:
            case IRInstruction::Opcode::JUMP:
            case IRInstruction::Opcode::POP:
            case IRInstruction::Opcode::GUARD_TYPE:
            case IRInstruction::Opcode::UNROLL_MARKER:
                break;
            case IRInstruction::Opcode::LOAD_GLOBAL:
            case IRInstruction::Opcode::STORE_GLOBAL:
                if (!instr.data) {
                    markFailed();
                    return 0;
                }
                break;
            case IRInstruction::Opcode::LOAD_CONST:
            {
                if (instr.data) {
                    const Value* val = static_cast<const Value*>(instr.data);
                    if (val->type != ValueType::NUMBER) {
                        markFailed();
                        return 0;
                    }
                }
                break;
            }
            case IRInstruction::Opcode::LOOP_BACK:
                loop_back_count++;
                if (loop_back_count > 1) {
                    markFailed();
                    return 0;
                }
                break;
            default:
                markFailed();
                return 0;
        }
    }
    if (loop_back_count == 0) {
        markFailed();
        return 0;
    }
    
    // Internal conditionals (multiple JUMP_IF_FALSE) are now supported.
    // The first JUMP_IF_FALSE is the loop exit condition.
    // Subsequent JUMP_IF_FALSE instructions are forward jumps (if-block skips)
    // which jump within the loop body.
    // We identify which is the loop exit by checking: is there a LOOP_BACK
    // after this JUMP_IF_FALSE? If this is the "outermost" one that guards
    // the whole loop body, it's the exit. Others are internal.

    X86_64CodeGen codegen;
    std::vector<uint8_t> code;

    // =====================================================================
    // Analyze trace to find which local slots are read/written.
    // We need to know which locals to sync back on loop exit.
    // =====================================================================
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
    // GLOBAL REGISTER CACHING: Assign top 4 most-accessed globals to R12-R15
    // This eliminates the 10-byte MOV RAX,imm64 for every global access.
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
        constexpr uint8_t cache_regs[] = {12, 13, 14, 15}; // R12, R13, R14, R15
        for (size_t i = 0; i < std::min(sorted_globals.size(), (size_t)4); i++) {
            global_reg_cache[sorted_globals[i].first] = cache_regs[i];
        }
    }

    // =====================================================================
    // PROLOGUE: Save callee-saved registers, set up base pointers
    // =====================================================================
    // System V AMD64 ABI: RDI = first argument = ExecutionFrame*
    codegen.emitPushReg64(code, 3);  // RBX (callee-saved)
    codegen.emitPushReg64(code, 5);  // RBP (callee-saved)
    codegen.emitPushReg64(code, 12); // R12 (callee-saved)
    codegen.emitPushReg64(code, 13); // R13 (callee-saved)
    codegen.emitPushReg64(code, 14); // R14 (callee-saved)
    codegen.emitPushReg64(code, 15); // R15 (callee-saved)
    
    // ExecutionFrame layout:
    //   offset  0: uint64_t method_id
    //   offset  8: const Chunk* chunk  
    //   offset 16: uint64_t bytecode_pc
    //   offset 24: void* stack_pointer
    //   offset 32: void* local_variables (Value* pointing into VM stack)
    
    codegen.emitMovReg64Reg64(code, 5, 7);    // RBP = RDI (save ExecutionFrame*)
    codegen.emitMovReg64Mem(code, 3, 5, 32);  // RBX = [RBP+32] = local_variables ptr
    
    // Load cached global addresses into R12-R15
    for (const auto& [addr, reg] : global_reg_cache) {
        codegen.emitMovReg64Imm64(code, reg, reinterpret_cast<uint64_t>(addr));
    }
    
    // RBP = ExecutionFrame*  (preserved across loop)
    // RBX = Value* locals    (preserved across loop)
    // R12-R15 = cached global Value* addresses (preserved across loop)
    // XMM0..XMM14 = operand stack (max 15 slots)
    
    int stack_depth = 0;
    
    // =====================================================================
    // LOOP START
    // =====================================================================
    size_t loop_start_offset = code.size();
    std::vector<size_t> exit_jumps;

    // Track last comparison for correct JUMP_IF_FALSE encoding
    // 0 = LESS (JAE), 1 = GREATER (JBE), 2 = EQUAL (JNE), 3 = NOT_EQUAL (JE)
    int last_comparison = 0;
    
    // =====================================================================
    // INTEGER REGISTER CACHE: Track when RAX already holds the int64 value
    // of an XMM register. Eliminates redundant CVTTSD2SI in bitwise chains.
    // int_cached_xmm = -1 means no cached value.
    // int_xmm_stale: when true, the XMM register indicated by int_cached_xmm
    //   has NOT been updated with CVTSI2SD — only RAX holds the correct value.
    //   Must emit CVTSI2SD before any operation that reads the XMM value.
    // =====================================================================
    int int_cached_xmm = -1;
    bool int_xmm_stale = false;
    
    // Helper lambda: materialize deferred int→double conversion if needed
    auto materialize_int_to_double = [&]() {
        if (int_xmm_stale && int_cached_xmm >= 0) {
            uint8_t reg = static_cast<uint8_t>(int_cached_xmm);
            // CVTSI2SD xmm[reg], RAX
            code.push_back(0xF2);
            uint8_t rex2 = 0x48;
            if (reg >= 8) rex2 |= 0x04;
            code.push_back(rex2);
            code.push_back(0x0F);
            code.push_back(0x2A);
            code.push_back(X86_64CodeGen::encodeModRM(3, reg & 7, 0));
            int_xmm_stale = false;
        }
    };
    
    // =====================================================================
    // FORWARD JUMP TRACKING: For internal conditionals (if-blocks within
    // the loop body). Maps from IR index to code patch offset.
    // The first JUMP_IF_FALSE is the loop exit; subsequent ones are
    // forward jumps that skip over if-block bodies.
    // =====================================================================
    struct ForwardJump {
        size_t patch_offset;   // Where in code[] to patch the rel32
        uint32_t target_ir;    // Target IR index (current + operand1 + 1)
    };
    std::vector<ForwardJump> forward_jumps;
    bool seen_first_jif = false; // Track whether we've seen the loop exit JIF

    // =====================================================================
    // BODY: Emit native code for each IR instruction
    // =====================================================================
    for (size_t ir_idx = 0; ir_idx < trace.ir_instructions.size(); ++ir_idx) {
        const auto& instr = trace.ir_instructions[ir_idx];
        
        // Resolve forward jumps that target this IR index
        for (auto it = forward_jumps.begin(); it != forward_jumps.end(); ) {
            if (it->target_ir == ir_idx) {
                // Patch the rel32 at patch_offset to jump here
                int64_t diff = (int64_t)code.size() - (int64_t)(it->patch_offset + 4);
                int32_t diff32 = (int32_t)diff;
                code[it->patch_offset]     = diff32 & 0xFF;
                code[it->patch_offset + 1] = (diff32 >> 8) & 0xFF;
                code[it->patch_offset + 2] = (diff32 >> 16) & 0xFF;
                code[it->patch_offset + 3] = (diff32 >> 24) & 0xFF;
                it = forward_jumps.erase(it);
            } else {
                ++it;
            }
        }
        
        // Before processing any instruction, materialize deferred int→double
        // conversion if the current instruction is NOT a bitwise op (which
        // handles the stale XMM via int_cached_xmm) and NOT a pure stack op
        // (LOAD_CONST, LOAD_LOCAL, LOAD_GLOBAL, POP that don't read the stale XMM).
        if (int_xmm_stale) {
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
                    // These either use RAX directly or don't read the stale XMM
                    // LOAD_GLOBAL handles RAX clobbering internally
                    break;
                default:
                    // Any other operation might read XMM values — materialize first
                    materialize_int_to_double();
                    break;
            }
        }
        
        switch (instr.opcode) {
            
            case IRInstruction::Opcode::LOAD_LOCAL:
            {
                if (stack_depth < 15) {
                    // Invalidate if we're about to overwrite the cached XMM slot
                    if (int_cached_xmm == stack_depth) {
                        int_cached_xmm = -1;
                        int_xmm_stale = false;
                    }
                    codegen.emitMovSdXmmMem(code, stack_depth, 3, 
                                            instr.operand1 * 16 + 8);
                    stack_depth++;
                }
                break;
            }
            
            case IRInstruction::Opcode::STORE_LOCAL:
            {
                // Store xmm[stack_depth-1] to locals[slot].as.number
                // Assignment result stays on stack (Neutron semantics)
                if (stack_depth > 0) {
                    codegen.emitMovSdMemXmm(code, 3, 
                                            instr.operand1 * 16 + 8, 
                                            stack_depth - 1);
                }
                break;
            }
            
            case IRInstruction::Opcode::LOAD_GLOBAL:
            {
                if (stack_depth < 15 && instr.data) {
                    // Invalidate if we're about to overwrite the cached XMM slot
                    if (int_cached_xmm == stack_depth) {
                        int_cached_xmm = -1;
                        int_xmm_stale = false;
                    }
                    auto cache_it = global_reg_cache.find(instr.data);
                    if (cache_it != global_reg_cache.end()) {
                        codegen.emitMovSdXmmMem(code, stack_depth, cache_it->second, 8);
                    } else {
                        // RAX will be clobbered — materialize deferred conversion first
                        materialize_int_to_double();
                        codegen.emitMovReg64Imm64(code, 0, reinterpret_cast<uint64_t>(instr.data));
                        codegen.emitMovSdXmmMem(code, stack_depth, 0, 8);
                        int_cached_xmm = -1; // RAX clobbered
                        int_xmm_stale = false;
                    }
                    stack_depth++;
                }
                break;
            }
            
            case IRInstruction::Opcode::STORE_GLOBAL:
            {
                // Store xmm[stack_depth-1] to global variable via direct Value* pointer.
                // Uses R12-R15 register cache for frequently accessed globals.
                if (stack_depth > 0 && instr.data) {
                    auto cache_it = global_reg_cache.find(instr.data);
                    if (cache_it != global_reg_cache.end()) {
                        codegen.emitMovSdMemXmm(code, cache_it->second, 8, stack_depth - 1);
                    } else {
                        // Use RDX instead of RAX to preserve integer cache
                        codegen.emitMovReg64Imm64(code, 2, reinterpret_cast<uint64_t>(instr.data));
                        codegen.emitMovSdMemXmm(code, 2, 8, stack_depth - 1);
                    }
                }
                break;
            }
            
            case IRInstruction::Opcode::LOAD_CONST:
            {
                // Load a constant from the chunk's constant pool.
                // We store the constant's double value as raw bits directly
                // in the instruction stream (RIP-relative or MOV to GPR then MOVQ).
                //
                // Strategy: MOV RAX, imm64; MOVQ xmm, RAX
                // This is 10+4 = 14 bytes but always works.
                if (stack_depth < 15) {
                    // We need the Chunk to resolve the constant. Store the constant
                    // index as operand1. At compile time we have the trace which has
                    // chunk info. We need to resolve it NOW.
                    // 
                    // The trace was recorded from a specific chunk. We stored it in
                    // recordTrace. Let's access it via traces_ map.
                    // BUT compileTrace receives a const reference. The chunk is available
                    // from the trace's method. However, we don't store it directly.
                    //
                    // WORKAROUND: Emit code that loads from [RBX + slot*16 + 8], 
                    // but we don't have a slot for constants.
                    //
                    // BETTER: During convertToIR, resolve the constant value and store
                    // it in the IR instruction's data field.
                    //
                    // For now, we use a hardcoded approach: look up the constant in
                    // the trace's chunk reference. The trace stores method_id and
                    // loop_entry_pc. We can find the original trace in traces_ map.
                    //
                    // SIMPLEST FIX: Store the double value directly in the IR data ptr.
                    // We do this: instr.data points to the Value, read its double.
                    
                    double const_val = 0.0;
                    if (instr.data) {
                        // data points to a Value in the chunk's constants pool
                        const Value* val = static_cast<const Value*>(instr.data);
                        const_val = val->as.number;
                    }
                    
                    // Invalidate if we're about to overwrite the cached XMM slot
                    if (int_cached_xmm == stack_depth) int_cached_xmm = -1;
                    
                    // Emit: MOV RDX, imm64 (the double's bit pattern)
                    // Use RDX instead of RAX to preserve integer register cache
                    uint64_t bits;
                    std::memcpy(&bits, &const_val, 8);
                    codegen.emitMovReg64Imm64(code, 2, bits); // RDX = double bits
                    
                    // Emit: MOVQ xmm[stack_depth], RDX
                    // MOVQ xmm, r64: 66 REX.W 0F 6E /r
                    uint8_t xmm = stack_depth;
                    code.push_back(0x66);
                    // REX.W required, R = xmm>=8, B = 0 (RDX=2)
                    uint8_t rex = 0x48; // REX.W
                    if (xmm >= 8) rex |= 0x04; // REX.R
                    code.push_back(rex);
                    code.push_back(0x0F);
                    code.push_back(0x6E);
                    code.push_back(X86_64CodeGen::encodeModRM(3, xmm & 7, 2)); // xmm, RDX
                    
                    stack_depth++;
                    // Don't invalidate int cache - RDX used, RAX preserved
                }
                break;
            }
                
            case IRInstruction::Opcode::ADD:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    codegen.emitAddSdXmmXmm(code, stack_depth - 1, stack_depth);
                    if (int_cached_xmm == stack_depth - 1 || int_cached_xmm == stack_depth)
                        int_cached_xmm = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::SUBTRACT:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    codegen.emitSubSdXmmXmm(code, stack_depth - 1, stack_depth);
                    if (int_cached_xmm == stack_depth - 1 || int_cached_xmm == stack_depth)
                        int_cached_xmm = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::MULTIPLY:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    codegen.emitMulSdXmmXmm(code, stack_depth - 1, stack_depth);
                    if (int_cached_xmm == stack_depth - 1 || int_cached_xmm == stack_depth)
                        int_cached_xmm = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::DIVIDE:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    codegen.emitDivSdXmmXmm(code, stack_depth - 1, stack_depth);
                    if (int_cached_xmm == stack_depth - 1 || int_cached_xmm == stack_depth)
                        int_cached_xmm = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::MODULO:
            {
                // Compute a % b using: a - trunc(a/b) * b
                // Uses XMM15 as scratch register.
                // Before: xmm[sd-2] = a, xmm[sd-1] = b
                // After:  xmm[sd-2] = a % b, stack_depth--
                if (stack_depth >= 2) {
                    uint8_t a_reg = stack_depth - 2;
                    uint8_t b_reg = stack_depth - 1;
                    
                    // MOVSD XMM15, xmm[a]  — copy a to scratch
                    // F2 [REX] 0F 10 ModRM(3, 15, a_reg)
                    code.push_back(0xF2);
                    uint8_t rex = 0x44; // REX.R (XMM15 needs R bit)
                    if (a_reg >= 8) rex |= 0x01; // REX.B
                    code.push_back(rex);
                    code.push_back(0x0F);
                    code.push_back(0x10);
                    code.push_back(X86_64CodeGen::encodeModRM(3, 7, a_reg & 7)); // XMM15=7 in low bits
                    
                    // DIVSD XMM15, xmm[b]  — XMM15 = a/b
                    code.push_back(0xF2);
                    rex = 0x44; // REX.R
                    if (b_reg >= 8) rex |= 0x01;
                    code.push_back(rex);
                    code.push_back(0x0F);
                    code.push_back(0x5E);
                    code.push_back(X86_64CodeGen::encodeModRM(3, 7, b_reg & 7));
                    
                    // CVTTSD2SI RAX, XMM15 — truncate to int64
                    // F2 REX(W=1,B=1) 0F 2C ModRM(3, RAX=0, XMM15&7=7)
                    code.push_back(0xF2);
                    code.push_back(0x49); // REX.W + REX.B (XMM15 needs B extension)
                    code.push_back(0x0F);
                    code.push_back(0x2C);
                    code.push_back(X86_64CodeGen::encodeModRM(3, 0, 7)); // RAX=0, XMM15&7=7
                    
                    // CVTSI2SD XMM15, RAX — convert back to double
                    // F2 REX.W+REX.R 0F 2A ModRM(3, XMM15&7, RAX)
                    code.push_back(0xF2);
                    code.push_back(0x4C); // REX.W(0x08) + REX.R(0x04) = 0x4C
                    code.push_back(0x0F);
                    code.push_back(0x2A);
                    code.push_back(X86_64CodeGen::encodeModRM(3, 7, 0)); // XMM15&7=7, RAX=0
                    
                    // MULSD XMM15, xmm[b] — XMM15 = trunc(a/b) * b
                    code.push_back(0xF2);
                    rex = 0x44; // REX.R
                    if (b_reg >= 8) rex |= 0x01;
                    code.push_back(rex);
                    code.push_back(0x0F);
                    code.push_back(0x59);
                    code.push_back(X86_64CodeGen::encodeModRM(3, 7, b_reg & 7));
                    
                    // SUBSD xmm[a], XMM15 — result = a - trunc(a/b)*b
                    code.push_back(0xF2);
                    rex = 0x41; // REX.B (XMM15 in src/rm)
                    if (a_reg >= 8) rex |= 0x04; // REX.R
                    code.push_back(rex);
                    code.push_back(0x0F);
                    code.push_back(0x5C);
                    code.push_back(X86_64CodeGen::encodeModRM(3, a_reg & 7, 7));
                    
                    stack_depth--;
                    int_cached_xmm = -1;
                }
                break;
            }
            
            // =========================================================
            // BITWISE OPERATIONS — with INTEGER REGISTER CACHE
            // Binary ops pop 2, push 1. Keeps result in RAX as int64.
            // Uses int_cached_xmm to skip redundant CVTTSD2SI when the
            // result of one bitwise op feeds into the next.
            // Deferred conversion: if the next consuming instruction is
            // also a bitwise op, skip the CVTSI2SD (int→double) since
            // the next op will use RAX directly via int_cached_xmm.
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
                    
                    // Check if operand B was a LOAD_CONST (previous IR).
                    // If so, we can load the constant directly as integer
                    // in RCX, avoiding the XMM detour.
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
                                // Undo the LOAD_CONST: the XMM was already loaded,
                                // but we can just decrement stack_depth since we'll
                                // use the integer directly. The XMM write is wasted
                                // but harmless.
                                stack_depth--;
                                b_reg = stack_depth; // b_reg no longer valid as XMM source
                                a_reg = stack_depth - 1;
                            }
                        }
                    }
                    
                    // Convert operand A to int64 in RAX (skip if cached)
                    if (int_cached_xmm == a_reg) {
                        // RAX already has the int64 value of xmm[a] — skip CVTTSD2SI
                    } else {
                        // CVTTSD2SI RAX, xmm[a]
                        code.push_back(0xF2);
                        {
                            uint8_t rex2 = 0x48;
                            if (a_reg >= 8) rex2 |= 0x01;
                            code.push_back(rex2);
                        }
                        code.push_back(0x0F);
                        code.push_back(0x2C);
                        code.push_back(X86_64CodeGen::encodeModRM(3, 0, a_reg & 7));
                    }
                    
                    if (b_is_const) {
                        // Load constant directly as integer into RCX
                        // MOV RCX, imm64 (or imm32 if fits)
                        codegen.emitMovReg64Imm64(code, 1, static_cast<uint64_t>(b_const_int));
                    } else {
                        // Convert operand B to int64 in RCX (always needed)
                        code.push_back(0xF2);
                        {
                            uint8_t rex2 = 0x48;
                            if (b_reg >= 8) rex2 |= 0x01;
                            code.push_back(rex2);
                        }
                        code.push_back(0x0F);
                        code.push_back(0x2C);
                        code.push_back(X86_64CodeGen::encodeModRM(3, 1, b_reg & 7));
                    }
                    
                    // Integer operation: RAX = RAX op RCX
                    switch (instr.opcode) {
                        case IRInstruction::Opcode::BITWISE_AND:
                            code.push_back(0x48); code.push_back(0x21);
                            code.push_back(X86_64CodeGen::encodeModRM(3, 1, 0));
                            break;
                        case IRInstruction::Opcode::BITWISE_OR:
                            code.push_back(0x48); code.push_back(0x09);
                            code.push_back(X86_64CodeGen::encodeModRM(3, 1, 0));
                            break;
                        case IRInstruction::Opcode::BITWISE_XOR:
                            code.push_back(0x48); code.push_back(0x31);
                            code.push_back(X86_64CodeGen::encodeModRM(3, 1, 0));
                            break;
                        case IRInstruction::Opcode::LEFT_SHIFT:
                            code.push_back(0x48); code.push_back(0xD3);
                            code.push_back(X86_64CodeGen::encodeModRM(3, 4, 0));
                            break;
                        case IRInstruction::Opcode::RIGHT_SHIFT:
                            code.push_back(0x48); code.push_back(0xD3);
                            code.push_back(X86_64CodeGen::encodeModRM(3, 7, 0));
                            break;
                        default: break;
                    }
                    
                    // Check if we can defer the int→double conversion.
                    // Look ahead: if the next instruction that consumes this
                    // XMM slot is a bitwise op (or BITWISE_NOT), and there's
                    // no intervening STORE_GLOBAL that reads this XMM, we can
                    // skip CVTSI2SD and let the next op use RAX directly.
                    bool defer_conversion = false;
                    {
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
                                break; // Can't defer past these
                            }
                            // LOAD_CONST, LOAD_GLOBAL, LOAD_LOCAL are OK to skip over
                        }
                    }
                    
                    if (!defer_conversion) {
                        // Convert result back to double: CVTSI2SD xmm[a], RAX
                        code.push_back(0xF2);
                        {
                            uint8_t rex2 = 0x48;
                            if (a_reg >= 8) rex2 |= 0x04;
                            code.push_back(rex2);
                        }
                        code.push_back(0x0F);
                        code.push_back(0x2A);
                        code.push_back(X86_64CodeGen::encodeModRM(3, a_reg & 7, 0));
                        int_xmm_stale = false;
                    } else {
                        int_xmm_stale = true;
                    }
                    
                    // Binary op consumes 2 values, produces 1.
                    // If b_is_const, we already decremented stack_depth once
                    // (to undo the LOAD_CONST). Don't decrement again.
                    if (!b_is_const) {
                        stack_depth--;
                    }
                    // Mark: RAX still holds the int64 of xmm[a_reg] = xmm[stack_depth-1]
                    int_cached_xmm = a_reg;
                }
                break;
            }
            
            case IRInstruction::Opcode::BITWISE_NOT:
            {
                // Unary: ~a. Pop 1, push 1.
                if (stack_depth >= 1) {
                    uint8_t a_reg = stack_depth - 1;
                    
                    // Convert to int64 in RAX (skip if cached)
                    if (int_cached_xmm == a_reg) {
                        // RAX already has the int64 value — skip CVTTSD2SI
                    } else {
                        code.push_back(0xF2);
                        {
                            uint8_t rex2 = 0x48;
                            if (a_reg >= 8) rex2 |= 0x01;
                            code.push_back(rex2);
                        }
                        code.push_back(0x0F);
                        code.push_back(0x2C);
                        code.push_back(X86_64CodeGen::encodeModRM(3, 0, a_reg & 7));
                    }
                    
                    // NOT RAX
                    code.push_back(0x48);
                    code.push_back(0xF7);
                    code.push_back(X86_64CodeGen::encodeModRM(3, 2, 0));
                    
                    // Check if we can defer the int→double conversion
                    bool defer_conversion = false;
                    {
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
                    }
                    
                    if (!defer_conversion) {
                        // CVTSI2SD xmm[a], RAX
                        code.push_back(0xF2);
                        {
                            uint8_t rex2 = 0x48;
                            if (a_reg >= 8) rex2 |= 0x04;
                            code.push_back(rex2);
                        }
                        code.push_back(0x0F);
                        code.push_back(0x2A);
                        code.push_back(X86_64CodeGen::encodeModRM(3, a_reg & 7, 0));
                        int_xmm_stale = false;
                    } else {
                        int_xmm_stale = true;
                    }
                    // stack_depth unchanged (pop 1, push 1)
                    int_cached_xmm = a_reg; // Result is cached in RAX
                }
                break;
            }
            
            case IRInstruction::Opcode::NEGATE:
            {
                // Unary negation: -a
                // XOR the sign bit of the double using XORPD with a constant
                // Simpler: SUBSD xmm_zero, xmm[a] where xmm_zero = 0.0
                // Even simpler: use MULSD with -1.0
                // Simplest: CVTTSD2SI, NEG, CVTSI2SD (for integer values)
                // Most correct for doubles: load -0.0 and XORPD
                // Let's use: emit SUBSD of 0.0 - value into XMM15, then MOVSD
                if (stack_depth >= 1) {
                    uint8_t a_reg = stack_depth - 1;
                    // Load 0.0 into XMM15
                    // XORPD XMM15, XMM15 (66 45 0F 57 FF)
                    code.push_back(0x66);
                    code.push_back(0x45); // REX.R + REX.B
                    code.push_back(0x0F);
                    code.push_back(0x57);
                    code.push_back(X86_64CodeGen::encodeModRM(3, 7, 7)); // XMM15, XMM15
                    
                    // SUBSD XMM15, xmm[a] → XMM15 = 0.0 - a = -a
                    code.push_back(0xF2);
                    {
                        uint8_t rex2 = 0x44; // REX.R
                        if (a_reg >= 8) rex2 |= 0x01;
                        code.push_back(rex2);
                    }
                    code.push_back(0x0F);
                    code.push_back(0x5C);
                    code.push_back(X86_64CodeGen::encodeModRM(3, 7, a_reg & 7));
                    
                    // MOVSD xmm[a], XMM15 → copy result back
                    code.push_back(0xF2);
                    {
                        uint8_t rex2 = 0x41; // REX.B (XMM15 in rm)
                        if (a_reg >= 8) rex2 |= 0x04;
                        code.push_back(rex2);
                    }
                    code.push_back(0x0F);
                    code.push_back(0x10);
                    code.push_back(X86_64CodeGen::encodeModRM(3, a_reg & 7, 7));
                    if (int_cached_xmm == a_reg) int_cached_xmm = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::LESS:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    codegen.emitUcomisdXmmXmm(code, stack_depth - 1, stack_depth);
                    stack_depth--;
                    last_comparison = 0;
                    // Comparisons don't modify values, but consume them.
                    // The cached XMM may now be at a consumed index.
                    if (int_cached_xmm >= stack_depth) int_cached_xmm = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::GREATER:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    codegen.emitUcomisdXmmXmm(code, stack_depth - 1, stack_depth);
                    stack_depth--;
                    last_comparison = 1;
                    if (int_cached_xmm >= stack_depth) int_cached_xmm = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::EQUAL:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    codegen.emitUcomisdXmmXmm(code, stack_depth - 1, stack_depth);
                    stack_depth--;
                    last_comparison = 2;
                    if (int_cached_xmm >= stack_depth) int_cached_xmm = -1;
                }
                break;
            }
            
            case IRInstruction::Opcode::NOT_EQUAL:
            {
                if (stack_depth >= 2) {
                    stack_depth--;
                    codegen.emitUcomisdXmmXmm(code, stack_depth - 1, stack_depth);
                    stack_depth--;
                    last_comparison = 3;
                    if (int_cached_xmm >= stack_depth) int_cached_xmm = -1;
                }
                break;
            }
                
            case IRInstruction::Opcode::JUMP_IF_FALSE:
            {
                int_cached_xmm = -1; // Invalidate integer cache at branches
                
                // Emit conditional jump based on which comparison set the flags.
                code.push_back(0x0F);
                switch (last_comparison) {
                    case 0: code.push_back(0x83); break; // JAE (not less)
                    case 1: code.push_back(0x86); break; // JBE (not greater)  
                    case 2: code.push_back(0x85); break; // JNE (not equal)
                    case 3: code.push_back(0x84); break; // JE  (not not-equal)
                    default: code.push_back(0x83); break; // fallback: JAE
                }
                
                if (!seen_first_jif) {
                    // First JUMP_IF_FALSE = loop exit condition → exit stub
                    seen_first_jif = true;
                    exit_jumps.push_back(code.size());
                    code.push_back(0); code.push_back(0); 
                    code.push_back(0); code.push_back(0);
                } else {
                    // Internal conditional → forward jump within loop body.
                    // operand1 is the bytecode jump offset, but we need the IR
                    // index to jump to. We calculate the target IR index by
                    // scanning forward for the matching POP (condition result).
                    // The jump target in IR is approximately: skip ahead by
                    // the number of IR instructions in the if-block body.
                    // We use operand1 as a hint for the number of bytecodes
                    // to skip, and count corresponding IR instructions.
                    uint32_t bytecodes_to_skip = instr.operand1;
                    // Rough estimate: each IR instruction ≈ 1-2 bytecodes
                    // More precise: count IR instructions that correspond to
                    // bytecodes_to_skip bytecodes from current position.
                    // For safety, scan forward for JUMP instruction which
                    // marks the end of the if-block.
                    uint32_t target_ir = ir_idx + 1;
                    int depth = 1;
                    for (size_t scan = ir_idx + 1; scan < trace.ir_instructions.size(); scan++) {
                        const auto& scan_instr = trace.ir_instructions[scan];
                        if (scan_instr.opcode == IRInstruction::Opcode::JUMP_IF_FALSE) {
                            depth++;
                        } else if (scan_instr.opcode == IRInstruction::Opcode::JUMP) {
                            depth--;
                            if (depth == 0) {
                                // The JUMP's target is after the else-block.
                                // But for simple if (no else), the JIF target
                                // is right after the if-body, which is at JUMP+1.
                                target_ir = scan + 1;
                                break;
                            }
                        }
                    }
                    // If no JUMP found, use bytecode offset as rough estimate
                    if (depth != 0) {
                        target_ir = std::min((uint32_t)(ir_idx + bytecodes_to_skip),
                                            (uint32_t)(trace.ir_instructions.size()));
                    }
                    forward_jumps.push_back({code.size(), target_ir});
                    code.push_back(0); code.push_back(0);
                    code.push_back(0); code.push_back(0);
                }
                break;
            }
            
            case IRInstruction::Opcode::JUMP:
            {
                int_cached_xmm = -1; // Invalidate integer cache at branches
                
                // Forward jump — used for skipping past else bodies in if/else.
                // Calculate the target IR index from the bytecode offset.
                // operand1 is the bytecode offset of the jump.
                // We scan forward to find the target IR instruction.
                codegen.emitJmpRel32(code, 0); // placeholder
                
                // Find the target IR index — the JUMP skips bytecodes_to_skip
                // bytecodes ahead. Use operand1 as rough offset from current IR.
                uint32_t bytecodes_to_skip = instr.operand1;
                uint32_t target_ir = ir_idx + 1;
                // For if-without-else: JUMP target is usually right after the else body
                // For simple if: operand1 skips to after the if-body POP
                // Approximate: target_ir ≈ ir_idx + operand1
                // More precise: just use operand1 as an IR offset estimate
                target_ir = std::min((uint32_t)(ir_idx + bytecodes_to_skip + 1),
                                    (uint32_t)(trace.ir_instructions.size()));
                
                forward_jumps.push_back({code.size() - 4, target_ir});
                break;
            }
                
            case IRInstruction::Opcode::LOOP_BACK:
            {
                // Unconditional jump back to loop start
                int64_t diff = (int64_t)loop_start_offset - (int64_t)(code.size() + 5);
                codegen.emitJmpRel32(code, (int32_t)diff);
                break;
            }
            
            case IRInstruction::Opcode::POP:
            {
                // Pop one value from the XMM stack
                if (stack_depth > 0) {
                    stack_depth--;
                }
                break;
            }
            
            case IRInstruction::Opcode::GUARD_TYPE:
            case IRInstruction::Opcode::UNROLL_MARKER:
                // Skip optimization markers in code generation
                break;
                
            default:
                break;
        }
    }

    // =====================================================================
    // EXIT STUB: Sync modified locals back to memory, then return
    // =====================================================================
    // Patch all exit_jumps to point HERE (the sync-back stub)
    for (size_t offset : exit_jumps) {
        int64_t diff = (int64_t)code.size() - (int64_t)(offset + 4);
        int32_t diff32 = (int32_t)diff;
        code[offset]     = diff32 & 0xFF;
        code[offset + 1] = (diff32 >> 8) & 0xFF;
        code[offset + 2] = (diff32 >> 16) & 0xFF;
        code[offset + 3] = (diff32 >> 24) & 0xFF;
    }
    
    // Note: Written locals were already synced via STORE_LOCAL during the last
    // iteration. The STORE_LOCAL writes directly to memory on every iteration,
    // so locals are always up-to-date. No additional sync needed here.

    // =====================================================================
    // EPILOGUE: Restore callee-saved registers, return
    // =====================================================================
    codegen.emitPopReg64(code, 15); // R15
    codegen.emitPopReg64(code, 14); // R14
    codegen.emitPopReg64(code, 13); // R13
    codegen.emitPopReg64(code, 12); // R12
    codegen.emitPopReg64(code, 5);  // RBP
    codegen.emitPopReg64(code, 3);  // RBX
    codegen.emitRet(code);

    // =====================================================================
    // Copy generated code to executable memory
    // =====================================================================
    uint8_t* code_space = allocateCodeSpace(code.size());
    if (!code_space) {
        return 0;
    }
    
    std::memcpy(code_space, code.data(), code.size());
    
    // Make the code cache executable using the codegen helper
    codegen.makeExecutable(code_cache_.data(), code_cache_.size());
    
    uint64_t code_addr = reinterpret_cast<uint64_t>(code_space);
    
    // =====================================================================
    // CRITICAL: Store the compiled code address in the trace!
    // The trace_id returned must be findable later via findTrace().
    // We look up the trace in traces_ map and set compiled_code_address.
    // =====================================================================
    for (auto& [tid, t] : traces_) {
        if (t->method_id == trace.method_id && 
            t->loop_entry_pc == trace.loop_entry_pc) {
            t->compiled_code_address = code_addr;
            t->compiled_code_size = code.size();
            // Register in O(1) lookup cache
            uint64_t key = trace.method_id ^ (trace.loop_entry_pc * 2654435761ULL);
            compiled_trace_lookup_[key] = tid;
            return tid;
        }
    }
    
    // If trace not found in map (shouldn't happen), return 0
    return 0;
}

bool Tier2Compiler::executeTrace(uint64_t trace_id, void* context) {
    auto it = traces_.find(trace_id);
    if (it == traces_.end()) {
        return false;
    }

    const auto& trace = it->second;
    
    if (trace->compiled_code_address != 0) {
        using JITFunc = void(*)(void*);
        JITFunc func = reinterpret_cast<JITFunc>(trace->compiled_code_address);
        func(context);
        
        trace->execution_count++;
        return true;
    }
    
    return false;
}
uint64_t Tier2Compiler::findTrace(uint64_t method_id, uint64_t loop_entry_pc) {
    // O(1) lookup via pre-built hash map
    uint64_t key = method_id ^ (loop_entry_pc * 2654435761ULL);
    auto it = compiled_trace_lookup_.find(key);
    if (it != compiled_trace_lookup_.end()) {
        return it->second;
    }
    return 0;
}
std::unique_ptr<Tier2Compiler::ExecutionTrace> 
Tier2Compiler::unrollLoop(const ExecutionTrace& trace, int unroll_factor) {

    auto unrolled = std::make_unique<ExecutionTrace>(trace);
    
    if (unroll_factor <= 1) {
        return unrolled;  // No unrolling needed
    }

    // Find the loop back instruction
    size_t loop_back_idx = 0;
    bool found_loop_back = false;
    
    for (size_t i = 0; i < trace.ir_instructions.size(); ++i) {
        if (trace.ir_instructions[i].opcode == IRInstruction::Opcode::LOOP_BACK) {
            loop_back_idx = i;
            found_loop_back = true;
            break;
        }
    }

    if (!found_loop_back || loop_back_idx == 0) {
        return unrolled;  // No loop found
    }

    // Get the loop body (instructions before loop back)
    std::vector<IRInstruction> loop_body(
        trace.ir_instructions.begin(),
        trace.ir_instructions.begin() + loop_back_idx);

    // Replicate the loop body unroll_factor times
    unrolled->ir_instructions.clear();
    
    for (int i = 0; i < unroll_factor; ++i) {
        // Copy loop body, but modify branch targets for unrolled iterations
        for (const auto& instr : loop_body) {
            IRInstruction copy = instr;
            
            // Mark unrolled instructions
            if (instr.opcode == IRInstruction::Opcode::JUMP_IF_FALSE ||
                instr.opcode == IRInstruction::Opcode::JUMP) {
                // In a real system, would adjust jump targets
                // For now, keep as-is
            }
            
            unrolled->ir_instructions.push_back(copy);
        }
        
        // Add unroll marker
        IRInstruction unroll_marker;
        unroll_marker.opcode = IRInstruction::Opcode::UNROLL_MARKER;
        unroll_marker.operand1 = i;
        unrolled->ir_instructions.push_back(unroll_marker);
    }

    // Add final loop back
    IRInstruction loop_back;
    loop_back.opcode = IRInstruction::Opcode::LOOP_BACK;
    loop_back.operand1 = unroll_factor;
    unrolled->ir_instructions.push_back(loop_back);

    return unrolled;
}

std::unique_ptr<Tier2Compiler::ExecutionTrace> 
Tier2Compiler::inlineMethods(const ExecutionTrace& trace,
                              const HotSpotProfiler& profiler) {

    auto inlined = std::make_unique<ExecutionTrace>(trace);
    
    // For each CALL instruction, check if the method should be inlined
    for (size_t i = 0; i < inlined->ir_instructions.size(); ++i) {
        auto& instr = inlined->ir_instructions[i];
        
        if (instr.opcode == IRInstruction::Opcode::CALL_NATIVE) {
            uint64_t called_method_id = instr.operand1;
            auto method_profile = profiler.getMethodProfile(called_method_id);
            
            // Inline if method is hot enough
            if (method_profile && method_profile->call_count > TIER1_COMPILATION_THRESHOLD) {
                // Mark for inlining
                IRInstruction inline_marker;
                inline_marker.opcode = IRInstruction::Opcode::INLINE_CALL;
                inline_marker.operand1 = called_method_id;
                inlined->ir_instructions[i] = inline_marker;
            }
        }
    }

    return inlined;
}

std::unique_ptr<Tier2Compiler::ExecutionTrace> 
Tier2Compiler::specializeTypes(const ExecutionTrace& trace,
                                const HotSpotProfiler& profiler) {

    auto specialized = std::make_unique<ExecutionTrace>(trace);
    
    // Add type guards based on profiling data
    // Type specialization enables type-specific code generation
    
    // Insert guards at trace entry
    if (specialized->ir_instructions.size() > 0) {
        // For each load operation, add potential type guard
        std::vector<IRInstruction> with_guards;
        
        for (size_t i = 0; i < specialized->ir_instructions.size(); ++i) {
            const auto& instr = specialized->ir_instructions[i];
            
            // Add guard before loads
            if (instr.opcode == IRInstruction::Opcode::LOAD_LOCAL ||
                instr.opcode == IRInstruction::Opcode::LOAD_CONST) {
                
                // Type guard: check if value is expected type
                IRInstruction guard;
                guard.opcode = IRInstruction::Opcode::GUARD_TYPE;
                guard.operand1 = instr.operand1;
                with_guards.push_back(guard);
            }
            
            with_guards.push_back(instr);
        }
        
        specialized->ir_instructions = with_guards;
    }

    return specialized;
}

const Tier2Compiler::ExecutionTrace* 
Tier2Compiler::getCompiledTrace(uint64_t trace_id) const {
    auto it = traces_.find(trace_id);
    if (it == traces_.end()) {
        return nullptr;
    }

    return it->second.get();
}

Tier2Compiler::TraceStats Tier2Compiler::getTraceStats() const {
    TraceStats stats;
    stats.total_traces = traces_.size();
    stats.active_traces = traces_.size();
    stats.total_compiled_code_size = code_cache_offset_;
    stats.max_code_cache_size = TIER2_CODE_CACHE_SIZE;

    // Calculate average trace length
    if (!traces_.empty()) {
        size_t total_length = 0;
        for (const auto& [trace_id, trace] : traces_) {
            total_length += trace->ir_instructions.size();
        }
        stats.avg_trace_length = static_cast<float>(total_length) / traces_.size();
    } else {
        stats.avg_trace_length = 0.0f;
    }

    // Calculate average optimization time
    if (total_traces_ > 0) {
        stats.avg_optimization_time_us = 
            static_cast<float>(total_optimization_time_us_) / total_traces_;
    } else {
        stats.avg_optimization_time_us = 0.0f;
    }

    return stats;
}

void Tier2Compiler::clearTraceCache() {
    traces_.clear();
    code_cache_.clear();
    code_cache_initialized_ = false;
    code_cache_offset_ = 0;
    trace_queue_.clear();
    compiled_trace_lookup_.clear();
    failed_traces_.clear();
    next_trace_id_ = 1;
    total_traces_ = 0;
    total_optimization_time_us_ = 0;
}

std::vector<Tier2Compiler::IRInstruction> 
Tier2Compiler::convertToIR(const Chunk& bytecode, uint64_t start_pc, uint64_t end_pc) {
    
    std::vector<IRInstruction> ir;
    // Stable storage for immediate constants introduced by specialized opcodes
    static std::deque<Value> temp_constants;

    for (uint64_t pc = start_pc; pc < end_pc && pc < bytecode.code.size(); ) {
        uint8_t opcode = bytecode.code[pc];
        int instr_size = getInstructionSize(opcode);
        pc++; // Consume opcode

        IRInstruction instr;
        instr.opcode = IRInstruction::Opcode::INVALID;
        instr.operand1 = 0;
        instr.operand2 = 0;
        instr.data = nullptr;

        // Convert bytecode opcode to IR opcode
        switch (static_cast<OpCode>(opcode)) {
            // Arithmetic operations (generic)
            case OpCode::OP_ADD:
                instr.opcode = IRInstruction::Opcode::ADD;
                break;
            case OpCode::OP_SUBTRACT:
                instr.opcode = IRInstruction::Opcode::SUBTRACT;
                break;
            case OpCode::OP_MULTIPLY:
                instr.opcode = IRInstruction::Opcode::MULTIPLY;
                break;
            case OpCode::OP_DIVIDE:
                instr.opcode = IRInstruction::Opcode::DIVIDE;
                break;
            case OpCode::OP_MODULO:
                instr.opcode = IRInstruction::Opcode::MODULO;
                break;

            // Arithmetic operations (specialized int) → map to generic IR
            case OpCode::OP_ADD_INT:
                instr.opcode = IRInstruction::Opcode::ADD;
                break;
            case OpCode::OP_SUB_INT:
                instr.opcode = IRInstruction::Opcode::SUBTRACT;
                break;
            case OpCode::OP_MUL_INT:
                instr.opcode = IRInstruction::Opcode::MULTIPLY;
                break;
            case OpCode::OP_DIV_INT:
                instr.opcode = IRInstruction::Opcode::DIVIDE;
                break;
            case OpCode::OP_MOD_INT:
                instr.opcode = IRInstruction::Opcode::MODULO;
                break;
            case OpCode::OP_NEGATE_INT:
                instr.opcode = IRInstruction::Opcode::NEGATE;
                break;
            
            // Bitwise operations
            case OpCode::OP_BITWISE_AND:
                instr.opcode = IRInstruction::Opcode::BITWISE_AND;
                break;
            case OpCode::OP_BITWISE_OR:
                instr.opcode = IRInstruction::Opcode::BITWISE_OR;
                break;
            case OpCode::OP_BITWISE_XOR:
                instr.opcode = IRInstruction::Opcode::BITWISE_XOR;
                break;
            case OpCode::OP_BITWISE_NOT:
                instr.opcode = IRInstruction::Opcode::BITWISE_NOT;
                break;
            case OpCode::OP_LEFT_SHIFT:
                instr.opcode = IRInstruction::Opcode::LEFT_SHIFT;
                break;
            case OpCode::OP_RIGHT_SHIFT:
                instr.opcode = IRInstruction::Opcode::RIGHT_SHIFT;
                break;
            case OpCode::OP_NEGATE:
                instr.opcode = IRInstruction::Opcode::NEGATE;
                break;
            
            // Comparison
            case OpCode::OP_LESS:
                instr.opcode = IRInstruction::Opcode::LESS;
                break;
            case OpCode::OP_GREATER:
                instr.opcode = IRInstruction::Opcode::GREATER;
                break;
            case OpCode::OP_EQUAL:
                instr.opcode = IRInstruction::Opcode::EQUAL;
                break;
            case OpCode::OP_NOT_EQUAL:
                instr.opcode = IRInstruction::Opcode::NOT_EQUAL;
                break;

            // Comparison (specialized int) → map to generic IR
            case OpCode::OP_LESS_INT:
                instr.opcode = IRInstruction::Opcode::LESS;
                break;
            case OpCode::OP_GREATER_INT:
                instr.opcode = IRInstruction::Opcode::GREATER;
                break;
            case OpCode::OP_EQUAL_INT:
                instr.opcode = IRInstruction::Opcode::EQUAL;
                break;
            
            // Memory operations
            case OpCode::OP_CONSTANT:
                instr.opcode = IRInstruction::Opcode::LOAD_CONST;
                instr.operand1 = bytecode.code[pc++]; // Read index
                // Resolve the constant value NOW and store pointer in data
                if (instr.operand1 < bytecode.constants.size()) {
                    instr.data = const_cast<void*>(
                        static_cast<const void*>(&bytecode.constants[instr.operand1]));
                }
                break;
            case OpCode::OP_CONST_INT8:
            {
                instr.opcode = IRInstruction::Opcode::LOAD_CONST;
                int8_t val = static_cast<int8_t>(bytecode.code[pc++]);
                temp_constants.emplace_back(static_cast<double>(val));
                instr.data = const_cast<void*>(static_cast<const void*>(&temp_constants.back()));
                break;
            }
            case OpCode::OP_CONST_ZERO:
            {
                instr.opcode = IRInstruction::Opcode::LOAD_CONST;
                temp_constants.emplace_back(0.0);
                instr.data = const_cast<void*>(static_cast<const void*>(&temp_constants.back()));
                break;
            }
            case OpCode::OP_CONST_ONE:
            {
                instr.opcode = IRInstruction::Opcode::LOAD_CONST;
                temp_constants.emplace_back(1.0);
                instr.data = const_cast<void*>(static_cast<const void*>(&temp_constants.back()));
                break;
            }
            case OpCode::OP_CONSTANT_LONG:
            {
                instr.opcode = IRInstruction::Opcode::LOAD_CONST;
                uint16_t idx = readU16BE(bytecode.code, pc);
                pc += 2;
                instr.operand1 = idx;
                if (idx < bytecode.constants.size()) {
                    instr.data = const_cast<void*>(
                        static_cast<const void*>(&bytecode.constants[idx]));
                }
                break;
            }
            case OpCode::OP_GET_LOCAL:
                instr.opcode = IRInstruction::Opcode::LOAD_LOCAL;
                instr.operand1 = bytecode.code[pc++]; // Read slot
                break;
            case OpCode::OP_LOAD_LOCAL_0:
                instr.opcode = IRInstruction::Opcode::LOAD_LOCAL;
                instr.operand1 = 0;
                break;
            case OpCode::OP_LOAD_LOCAL_1:
                instr.opcode = IRInstruction::Opcode::LOAD_LOCAL;
                instr.operand1 = 1;
                break;
            case OpCode::OP_LOAD_LOCAL_2:
                instr.opcode = IRInstruction::Opcode::LOAD_LOCAL;
                instr.operand1 = 2;
                break;
            case OpCode::OP_LOAD_LOCAL_3:
                instr.opcode = IRInstruction::Opcode::LOAD_LOCAL;
                instr.operand1 = 3;
                break;
            case OpCode::OP_SET_LOCAL:
                instr.opcode = IRInstruction::Opcode::STORE_LOCAL;
                instr.operand1 = bytecode.code[pc++]; // Read slot
                break;
            case OpCode::OP_SET_LOCAL_TYPED:
            {
                instr.opcode = IRInstruction::Opcode::STORE_LOCAL;
                instr.operand1 = bytecode.code[pc++]; // Read slot
                pc++; // Skip type byte
                break;
            }
            case OpCode::OP_INC_LOCAL_INT:
            {
                // Expand into LOAD_LOCAL, CONST 1, ADD, STORE_LOCAL
                uint8_t slot = bytecode.code[pc++];
                instr.opcode = IRInstruction::Opcode::LOAD_LOCAL;
                instr.operand1 = slot;
                ir.push_back(instr);

                IRInstruction c1;
                c1.opcode = IRInstruction::Opcode::LOAD_CONST;
                temp_constants.emplace_back(1.0);
                c1.data = const_cast<void*>(static_cast<const void*>(&temp_constants.back()));
                ir.push_back(c1);

                IRInstruction add;
                add.opcode = IRInstruction::Opcode::ADD;
                ir.push_back(add);

                instr = IRInstruction();
                instr.opcode = IRInstruction::Opcode::STORE_LOCAL;
                instr.operand1 = slot;
                break;
            }
            case OpCode::OP_DEC_LOCAL_INT:
            {
                uint8_t slot = bytecode.code[pc++];
                instr.opcode = IRInstruction::Opcode::LOAD_LOCAL;
                instr.operand1 = slot;
                ir.push_back(instr);

                IRInstruction c1;
                c1.opcode = IRInstruction::Opcode::LOAD_CONST;
                temp_constants.emplace_back(1.0);
                c1.data = const_cast<void*>(static_cast<const void*>(&temp_constants.back()));
                ir.push_back(c1);

                IRInstruction sub;
                sub.opcode = IRInstruction::Opcode::SUBTRACT;
                ir.push_back(sub);

                instr = IRInstruction();
                instr.opcode = IRInstruction::Opcode::STORE_LOCAL;
                instr.operand1 = slot;
                break;
            }
            
            // Global variable access — resolve to direct Value* pointer
            case OpCode::OP_GET_GLOBAL:
            {
                uint8_t name_idx = bytecode.code[pc++];
                instr.opcode = IRInstruction::Opcode::LOAD_GLOBAL;
                instr.operand1 = name_idx;
                // Resolve global address at IR-build time
                if (globals_map_ && name_idx < bytecode.constants.size()) {
                    const Value& nameVal = bytecode.constants[name_idx];
                    if (nameVal.type == ValueType::OBJ_STRING) {
                        auto* globals = static_cast<std::unordered_map<std::string, Value>*>(globals_map_);
                        auto it = globals->find(nameVal.as.obj_string->chars);
                        if (it != globals->end()) {
                            instr.data = &(it->second); // Direct pointer to Value in map
                        }
                    }
                }
                break;
            }
            case OpCode::OP_SET_GLOBAL:
            {
                uint8_t name_idx = bytecode.code[pc++];
                instr.opcode = IRInstruction::Opcode::STORE_GLOBAL;
                instr.operand1 = name_idx;
                if (globals_map_ && name_idx < bytecode.constants.size()) {
                    const Value& nameVal = bytecode.constants[name_idx];
                    if (nameVal.type == ValueType::OBJ_STRING) {
                        auto* globals = static_cast<std::unordered_map<std::string, Value>*>(globals_map_);
                        auto it = globals->find(nameVal.as.obj_string->chars);
                        if (it != globals->end()) {
                            instr.data = &(it->second);
                        }
                    }
                }
                break;
            }
            case OpCode::OP_SET_GLOBAL_TYPED:
            {
                uint8_t name_idx = bytecode.code[pc++];
                instr.opcode = IRInstruction::Opcode::STORE_GLOBAL;
                instr.operand1 = name_idx;
                if (globals_map_ && name_idx < bytecode.constants.size()) {
                    const Value& nameVal = bytecode.constants[name_idx];
                    if (nameVal.type == ValueType::OBJ_STRING) {
                        auto* globals = static_cast<std::unordered_map<std::string, Value>*>(globals_map_);
                        auto it = globals->find(nameVal.as.obj_string->chars);
                        if (it != globals->end()) {
                            instr.data = &(it->second);
                        }
                    }
                }
                break;
            }
            
            // Fast global access (same encoding as OP_GET_GLOBAL/OP_SET_GLOBAL)
            case OpCode::OP_GET_GLOBAL_FAST:
            {
                uint8_t name_idx = bytecode.code[pc++];
                instr.opcode = IRInstruction::Opcode::LOAD_GLOBAL;
                instr.operand1 = name_idx;
                if (globals_map_ && name_idx < bytecode.constants.size()) {
                    const Value& nameVal = bytecode.constants[name_idx];
                    if (nameVal.type == ValueType::OBJ_STRING) {
                        auto* globals = static_cast<std::unordered_map<std::string, Value>*>(globals_map_);
                        auto it = globals->find(nameVal.as.obj_string->chars);
                        if (it != globals->end()) {
                            instr.data = &(it->second);
                        }
                    }
                }
                break;
            }
            case OpCode::OP_SET_GLOBAL_FAST:
            {
                uint8_t name_idx = bytecode.code[pc++];
                instr.opcode = IRInstruction::Opcode::STORE_GLOBAL;
                instr.operand1 = name_idx;
                if (globals_map_ && name_idx < bytecode.constants.size()) {
                    const Value& nameVal = bytecode.constants[name_idx];
                    if (nameVal.type == ValueType::OBJ_STRING) {
                        auto* globals = static_cast<std::unordered_map<std::string, Value>*>(globals_map_);
                        auto it = globals->find(nameVal.as.obj_string->chars);
                        if (it != globals->end()) {
                            instr.data = &(it->second);
                        }
                    }
                }
                break;
            }
            case OpCode::OP_INCREMENT_GLOBAL:
            {
                uint8_t name_idx = bytecode.code[pc++];
                // Expand to LOAD_GLOBAL + CONST_ONE + ADD + STORE_GLOBAL
                instr.opcode = IRInstruction::Opcode::LOAD_GLOBAL;
                instr.operand1 = name_idx;
                if (globals_map_ && name_idx < bytecode.constants.size()) {
                    const Value& nameVal = bytecode.constants[name_idx];
                    if (nameVal.type == ValueType::OBJ_STRING) {
                        auto* globals = static_cast<std::unordered_map<std::string, Value>*>(globals_map_);
                        auto it = globals->find(nameVal.as.obj_string->chars);
                        if (it != globals->end()) {
                            instr.data = &(it->second);
                        }
                    }
                }
                ir.push_back(instr);

                IRInstruction c1;
                c1.opcode = IRInstruction::Opcode::LOAD_CONST;
                temp_constants.emplace_back(1.0);
                c1.data = const_cast<void*>(static_cast<const void*>(&temp_constants.back()));
                ir.push_back(c1);

                IRInstruction add;
                add.opcode = IRInstruction::Opcode::ADD;
                ir.push_back(add);

                instr = IRInstruction();
                instr.opcode = IRInstruction::Opcode::STORE_GLOBAL;
                instr.operand1 = name_idx;
                // Re-resolve for store
                if (globals_map_ && name_idx < bytecode.constants.size()) {
                    const Value& nameVal = bytecode.constants[name_idx];
                    if (nameVal.type == ValueType::OBJ_STRING) {
                        auto* globals = static_cast<std::unordered_map<std::string, Value>*>(globals_map_);
                        auto it = globals->find(nameVal.as.obj_string->chars);
                        if (it != globals->end()) {
                            instr.data = &(it->second);
                        }
                    }
                }
                break;
            }
            
            // Fused: GET_LOCAL + CONSTANT + ADD
            case OpCode::OP_ADD_LOCAL_CONST:
            {
                uint8_t slot = bytecode.code[pc++];
                uint8_t constIdx = bytecode.code[pc++];
                // Expand to LOAD_LOCAL + LOAD_CONST + ADD
                instr.opcode = IRInstruction::Opcode::LOAD_LOCAL;
                instr.operand1 = slot;
                ir.push_back(instr);

                IRInstruction constInstr;
                constInstr.opcode = IRInstruction::Opcode::LOAD_CONST;
                constInstr.operand1 = constIdx;
                if (constIdx < bytecode.constants.size()) {
                    constInstr.data = const_cast<void*>(
                        static_cast<const void*>(&bytecode.constants[constIdx]));
                }
                ir.push_back(constInstr);

                instr = IRInstruction();
                instr.opcode = IRInstruction::Opcode::ADD;
                break;
            }

            // Control flow
            case OpCode::OP_LOOP:
            {
                // Check if this is OUR loop's backward jump (at the end of the trace)
                // or an inner loop's OP_LOOP (which means we can't compile this trace)
                uint64_t this_loop_pc = pc - 1; // pc already past opcode
                pc += 2; // Skip offset (2 bytes)
                if (this_loop_pc + 3 >= end_pc) {
                    // This is our loop's OP_LOOP — it's at the end of the trace
                    instr.opcode = IRInstruction::Opcode::LOOP_BACK;
                } else {
                    // This is an inner loop — we can't handle nested loops
                    return {};
                }
                break;
            }
            
            case OpCode::OP_JUMP_IF_FALSE:
                instr.opcode = IRInstruction::Opcode::JUMP_IF_FALSE;
                instr.operand1 = readU16BE(bytecode.code, pc);
                pc += 2; // Skip offset
                break;

            case OpCode::OP_LESS_JUMP:
            {
                // Expand to LESS + JUMP_IF_FALSE
                instr.opcode = IRInstruction::Opcode::LESS;
                ir.push_back(instr);
                IRInstruction jif;
                jif.opcode = IRInstruction::Opcode::JUMP_IF_FALSE;
                jif.operand1 = readU16BE(bytecode.code, pc);
                pc += 2;
                instr = jif;
                break;
            }
            case OpCode::OP_GREATER_JUMP:
            {
                instr.opcode = IRInstruction::Opcode::GREATER;
                ir.push_back(instr);
                IRInstruction jif;
                jif.opcode = IRInstruction::Opcode::JUMP_IF_FALSE;
                jif.operand1 = readU16BE(bytecode.code, pc);
                pc += 2;
                instr = jif;
                break;
            }
            case OpCode::OP_EQUAL_JUMP:
            {
                instr.opcode = IRInstruction::Opcode::EQUAL;
                ir.push_back(instr);
                IRInstruction jif;
                jif.opcode = IRInstruction::Opcode::JUMP_IF_FALSE;
                jif.operand1 = readU16BE(bytecode.code, pc);
                pc += 2;
                instr = jif;
                break;
            }
            
            case OpCode::OP_JUMP:
                instr.opcode = IRInstruction::Opcode::JUMP;
                instr.operand1 = readU16BE(bytecode.code, pc);
                pc += 2; // Skip offset
                break;
            
            case OpCode::OP_POP:
                instr.opcode = IRInstruction::Opcode::POP;
                break;

            case OpCode::OP_INCREMENT_LOCAL:
            {
                // Expand superinstruction: local[slot] += 1.0
                // Only 1 operand byte: slot
                uint8_t slot = bytecode.code[pc++];
                
                // LOAD_LOCAL
                instr.opcode = IRInstruction::Opcode::LOAD_LOCAL;
                instr.operand1 = slot;
                ir.push_back(instr);

                IRInstruction c1;
                c1.opcode = IRInstruction::Opcode::LOAD_CONST;
                temp_constants.emplace_back(1.0);
                c1.data = const_cast<void*>(static_cast<const void*>(&temp_constants.back()));
                ir.push_back(c1);

                IRInstruction add;
                add.opcode = IRInstruction::Opcode::ADD;
                ir.push_back(add);

                instr = IRInstruction();
                instr.opcode = IRInstruction::Opcode::STORE_LOCAL;
                instr.operand1 = slot;
                break;
            }

            case OpCode::OP_LOOP_IF_LESS_LOCAL:
            {
                // Fused: if (local[slot] < constant) continue, else jump
                // Encoding: slot(1) + constant_idx(1) + jump_offset(2)
                uint8_t slot = bytecode.code[pc++];
                uint8_t constIdx = bytecode.code[pc++];
                uint16_t offset = readU16BE(bytecode.code, pc);
                pc += 2;

                // Expand to LOAD_LOCAL, LOAD_CONST, LESS, JUMP_IF_FALSE
                instr.opcode = IRInstruction::Opcode::LOAD_LOCAL;
                instr.operand1 = slot;
                ir.push_back(instr);

                IRInstruction constInstr;
                constInstr.opcode = IRInstruction::Opcode::LOAD_CONST;
                constInstr.operand1 = constIdx;
                if (constIdx < bytecode.constants.size()) {
                    constInstr.data = const_cast<void*>(
                        static_cast<const void*>(&bytecode.constants[constIdx]));
                }
                ir.push_back(constInstr);

                IRInstruction lessInstr;
                lessInstr.opcode = IRInstruction::Opcode::LESS;
                ir.push_back(lessInstr);

                instr = IRInstruction();
                instr.opcode = IRInstruction::Opcode::JUMP_IF_FALSE;
                instr.operand1 = offset;
                break;
            }

            case OpCode::OP_DECREMENT_LOCAL:
            {
                // Expand superinstruction: local[slot] -= 1.0
                uint8_t slot = bytecode.code[pc++];
                
                instr.opcode = IRInstruction::Opcode::LOAD_LOCAL;
                instr.operand1 = slot;
                ir.push_back(instr);

                IRInstruction c1;
                c1.opcode = IRInstruction::Opcode::LOAD_CONST;
                temp_constants.emplace_back(1.0);
                c1.data = const_cast<void*>(static_cast<const void*>(&temp_constants.back()));
                ir.push_back(c1);

                IRInstruction sub;
                sub.opcode = IRInstruction::Opcode::SUBTRACT;
                ir.push_back(sub);

                instr = IRInstruction();
                instr.opcode = IRInstruction::Opcode::STORE_LOCAL;
                instr.operand1 = slot;
                break;
            }

            default:
                // Unsupported opcode encountered — bail out entirely.
                // Returning an empty IR vector causes compileTrace to reject
                // this trace (no LOOP_BACK found), preventing partial/broken
                // native code from being generated.
                return {};
        }

        if (instr.opcode != IRInstruction::Opcode::INVALID) {
             ir.push_back(instr);
        }
        
        // Stop converting after LOOP_BACK (everything after is dead code)
        if (instr.opcode == IRInstruction::Opcode::LOOP_BACK) {
            break;
        }
    }

    return ir;
}

void Tier2Compiler::addTypeGuard(ExecutionTrace& trace, uint64_t value_id,
                                  const std::string& expected_type) {
    
    IRInstruction guard;
    guard.opcode = IRInstruction::Opcode::GUARD_TYPE;
    guard.operand1 = static_cast<uint32_t>(value_id & 0xFFFFFFFF);
    guard.data = const_cast<char*>(expected_type.c_str());

    trace.guard_conditions.push_back(value_id);
    trace.ir_instructions.insert(trace.ir_instructions.begin(), guard);
}

uint8_t* Tier2Compiler::allocateCodeSpace(size_t size) {
    // Lazy-init code cache on first allocation
    if (!code_cache_initialized_) {
        code_cache_.resize(TIER2_CODE_CACHE_SIZE);
        code_cache_initialized_ = true;
    }
    if (code_cache_offset_ + size > TIER2_CODE_CACHE_SIZE) {
        return nullptr;
    }

    uint8_t* ptr = code_cache_.data() + code_cache_offset_;
    code_cache_offset_ += size;

    return ptr;
}

uint64_t Tier2Compiler::generateNativeCode(const std::vector<IRInstruction>& ir) {
    // Generate native code from IR instructions
    // This is highly platform-specific
    
    // In a real implementation, this would use:
    // - LLVM: Convert IR to LLVM IR, compile with LLVM backend
    // - libgccjit: Convert IR to GCC IR, compile with libgccjit
    // - Direct assembly: Write machine code directly (platform-specific)
    
    // For simulation, allocate space and store IR size
    size_t estimated_size = ir.size() * 8;  // ~8 bytes per IR instruction average
    uint8_t* code_space = allocateCodeSpace(estimated_size);
    
    if (!code_space) {
        return 0;
    }

    // Store header with IR information for debugging/profiling
    struct IRCodeHeader {
        uint32_t ir_count;
        uint32_t reserved;
    };
    
    if (estimated_size >= sizeof(IRCodeHeader)) {
        IRCodeHeader* header = reinterpret_cast<IRCodeHeader*>(code_space);
        header->ir_count = static_cast<uint32_t>(ir.size());
        header->reserved = 0;
    }

    return reinterpret_cast<uint64_t>(code_space);
}

} // namespace neutron::jit
