#ifndef NEUTRON_JIT_TIER2_COMPILER_H
#define NEUTRON_JIT_TIER2_COMPILER_H

#include "jit_config.h"
#include "jit_profiler.h"
#include "jit_memory.h"
#include "compiler/bytecode.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <memory>
#include <deque>

namespace neutron::jit {

/**
 * Tier-2 JIT Compiler: Tracing JIT with Aggressive Optimizations
 * 
 * Performs trace-based just-in-time compilation with aggressive optimizations
 * for hot loops and frequently executed methods. This is the second-tier compilation
 * in the multi-tier JIT strategy, triggered when code from Tier-1 becomes hot.
 * 
 * Key features:
 * - Trace-based compilation (compiles execution paths, not full methods)
 * - Aggressive optimizations: loop unrolling, inlining, type specialization
 * - Guard-based specialization for type feedback
 * - Uses profiling data from Tier-1 for optimization decisions
 * 
 * Reference: "A Lightweight Method for Generating Multi-Tier JIT Compilation..."
 */
class Tier2Compiler {
public:
    /**
     * Intermediate representation (IR) instruction
     */
    struct IRInstruction {
        enum class Opcode : uint8_t {
            // Sentinel for uninitialized/invalid instructions
            INVALID = 0,
            
            // Memory operations
            LOAD_CONST,
            LOAD_LOCAL,
            STORE_LOCAL,
            POP,
            
            // Arithmetic
            ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULO,
            
            // Bitwise (operate on integers via double conversion)
            BITWISE_AND, BITWISE_OR, BITWISE_XOR, BITWISE_NOT,
            LEFT_SHIFT, RIGHT_SHIFT,
            NEGATE,
            
            // Logic
            NOT, EQUAL, NOT_EQUAL, LESS, GREATER,
            
            // Control flow
            JUMP, JUMP_IF_FALSE, JUMP_IF_TRUE,
            LOOP_BACK,
            
            // Specialization
            GUARD_TYPE,     ///< Type guard for specialization
            GUARD_CLASS,    ///< Class guard for method dispatch
            
            // Optimization markers
            INLINE_CALL,    ///< Mark for method inlining
            UNROLL_MARKER,  ///< Loop unroll marker
            
            // Global variable operations (use direct Value* pointers)
            LOAD_GLOBAL,    ///< Load from global variable (data = Value*)
            STORE_GLOBAL,   ///< Store to global variable (data = Value*)
            
            // Native operations
            CALL_NATIVE,    ///< Call to native code
        } opcode;

        uint32_t operand1;
        uint32_t operand2;
        void* data;  ///< Additional data (constants, types, etc.)
    };

    /**
     * Execution trace for a loop or method
     */
    struct ExecutionTrace {
        uint64_t trace_id;
        uint64_t method_id;
        uint64_t loop_entry_pc;  ///< PC of loop entry (backward jump target)
        
        std::vector<IRInstruction> ir_instructions;
        std::vector<uint64_t> guard_conditions;  ///< Type guards
        
        int64_t execution_count;
        int64_t compilation_time_us;
        uint64_t compiled_code_address;
        size_t compiled_code_size;
        
        std::vector<std::pair<uint64_t, uint64_t>> inline_cache_entries;
    };

    Tier2Compiler();
    ~Tier2Compiler();

    /**
     * Record execution trace by tracing through bytecode interpreter
     * @param method_id The method being traced
     * @param bytecode The bytecode of the method
     * @param start_pc Starting bytecode PC
     * @param profiler Profiler with type information
     * @return Recorded execution trace
     */
    std::unique_ptr<ExecutionTrace> recordTrace(
        uint64_t method_id,
        const Chunk& bytecode,
        uint64_t start_pc,
        const HotSpotProfiler& profiler);

    /**
     * Optimize a recorded trace with aggressive techniques
     * @param trace The trace to optimize
     * @return Optimized trace
     */
    std::unique_ptr<ExecutionTrace> optimizeTrace(const ExecutionTrace& trace);

    /**
     * Compile optimized trace to native code
     * @param trace The optimized trace
     * @return Compiled code address, or 0 on failure
     */
    uint64_t compileTrace(const ExecutionTrace& trace);

    /**
     * Execute compiled trace
     * @param trace_id ID of the trace to execute
     * @param context Execution context
     * @return true if execution succeeded
     */
    bool executeTrace(uint64_t trace_id, void* context);
    
    /**
     * Find a compiled trace
     */
    uint64_t findTrace(uint64_t method_id, uint64_t loop_entry_pc);

    /**
     * Check if a trace has already failed compilation (avoid retrying)
     */
    bool isTraceFailed(uint64_t method_id, uint64_t loop_entry_pc) const {
        return failed_traces_.count(method_id ^ (loop_entry_pc * 2654435761ULL)) > 0;
    }

    /**
     * Set the pointer to the VM's globals map for JIT global variable access.
     * Must be called before any trace compilation that uses global variables.
     * @param globals Pointer to std::unordered_map<std::string, Value>
     */
    void setGlobalsMap(void* globals) { globals_map_ = globals; }

    /**
     * Optimization: Loop unrolling for frequently executed loops
     * @param trace Original trace
     * @param unroll_factor How many times to unroll
     * @return Unrolled trace
     */
    std::unique_ptr<ExecutionTrace> unrollLoop(
        const ExecutionTrace& trace, int unroll_factor);

    /**
     * Optimization: Method inlining based on profiling data
     * @param trace Trace to inline into
     * @param profiler Profiling data for decisions
     * @return Trace with inlined methods
     */
    std::unique_ptr<ExecutionTrace> inlineMethods(
        const ExecutionTrace& trace,
        const HotSpotProfiler& profiler);

    /**
     * Optimization: Type specialization using guards
     * @param trace Trace to specialize
     * @param profiler Type feedback from profiler
     * @return Specialized trace with guards
     */
    std::unique_ptr<ExecutionTrace> specializeTypes(
        const ExecutionTrace& trace,
        const HotSpotProfiler& profiler);

    /**
     * Get compiled trace
     */
    const ExecutionTrace* getCompiledTrace(uint64_t trace_id) const;

    /**
     * Get trace statistics
     */
    struct TraceStats {
        size_t total_traces;
        size_t active_traces;
        size_t total_compiled_code_size;
        size_t max_code_cache_size;
        float avg_trace_length;
        float avg_optimization_time_us;
    };

    TraceStats getTraceStats() const;

    /**
     * Clear trace cache
     */
    void clearTraceCache();

private:
    /// Map from trace_id to execution trace
    std::unordered_map<uint64_t, std::unique_ptr<ExecutionTrace>> traces_;
    
    /// Set of (method_id ^ loop_pc) that failed compilation — don't retry
    std::unordered_set<uint64_t> failed_traces_;
    
    /// Pointer to VM's globals map (std::unordered_map<std::string, Value>*)
    void* globals_map_ = nullptr;
    
    /// O(1) lookup: hash(method_id, loop_pc) → trace_id
    std::unordered_map<uint64_t, uint64_t> compiled_trace_lookup_;
    
    /// Code cache for compiled traces (platform-specific executable memory)
    JITMemory code_cache_;
    bool code_cache_initialized_;

    /// Next trace ID
    uint64_t next_trace_id_;

    /// Trace queue for breadth-first trace recording
    std::deque<std::pair<uint64_t, uint64_t>> trace_queue_;  // (method_id, pc)

    /// Statistics
    size_t total_traces_;
    int64_t total_optimization_time_us_;

    /**
     * Convert bytecode instructions to IR
     */
    std::vector<IRInstruction> convertToIR(
        const Chunk& bytecode, uint64_t start_pc, uint64_t end_pc);

    /**
     * Add type guard to trace for specialization
     */
    void addTypeGuard(ExecutionTrace& trace, uint64_t value_id, 
                     const std::string& expected_type);

    /**
     * Allocate space in code cache
     */
    uint8_t* allocateCodeSpace(size_t size);

    /**
     * Generate native code from IR instructions
     */
    uint64_t generateNativeCode(const std::vector<IRInstruction>& ir);

#if defined(__aarch64__) || defined(__arm64__)
    /**
     * Compile optimized trace to native ARM64/AArch64 code
     * @param trace The optimized trace
     * @return trace_id on success, 0 on failure
     */
    uint64_t compileTraceARM64(const ExecutionTrace& trace);
#endif
};

} // namespace neutron::jit

#endif // NEUTRON_JIT_TIER2_COMPILER_H
