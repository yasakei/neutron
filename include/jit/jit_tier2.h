#ifndef NEUTRON_JIT_TIER2_COMPILER_H
#define NEUTRON_JIT_TIER2_COMPILER_H

/*
 * Code Documentation: Tier-2 Tracing JIT Compiler (jit_tier2.h)
 * =============================================================
 * 
 * This header defines the Tier2Compiler - the second and final tier of
 * Neutron's JIT compilation strategy. It performs trace-based compilation
 * with aggressive optimizations for hot loops and frequently executed code.
 * 
 * What This File Includes:
 * ------------------------
 * - Tier2Compiler class: Trace-based JIT compiler
 * - IRInstruction: Intermediate representation for optimization
 * - ExecutionTrace: Recorded execution path for compilation
 * - Optimization passes: Inlining, unrolling, type specialization
 * 
 * How It Works:
 * -------------
 * Tier-2 uses trace-based compilation, which differs from method-based:
 * 
 * 1. Recording: Execute and record hot path through bytecode
 * 2. IR Conversion: Convert recorded trace to intermediate representation
 * 3. Optimization: Apply aggressive optimizations to the trace
 * 4. Codegen: Generate native machine code from optimized IR
 * 5. Execution: Run native code with guards for type assumptions
 * 
 * Trace-Based vs Method-Based:
 * - Method-based: Compile entire function (includes cold paths)
 * - Trace-based: Compile only hot execution paths (better optimization)
 * 
 * Optimizations Applied:
 * - Loop unrolling: Reduce loop overhead
 * - Method inlining: Eliminate call overhead
 * - Type specialization: Generate type-specific code
 * - Guard elision: Remove redundant type checks
 * - Constant folding: Pre-compute constant expressions
 * 
 * Adding Features:
 * ----------------
 * - New IR instructions: Add to IRInstruction::Opcode enum
 * - New optimizations: Add optimization pass methods
 * - Platform support: Implement compileTraceARM64() for ARM
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT record traces for cold code (wastes compilation time)
 * - Do NOT skip guard insertion (type assumptions can change)
 * - Do NOT exceed code cache limits
 * - Do NOT modify globals_map_ during trace execution
 * 
 * Performance Characteristics:
 * ----------------------------
 * - Compilation speed: Slow (seconds for complex traces)
 * - Execution speed: Fast (native code with minimal overhead)
 * - Memory usage: High (multiple traces, optimized IR)
 * - Best for: Hot loops, long-running code, compute-intensive workloads
 * 
 * Guard-Based Specialization:
 * ---------------------------
 * Guards are runtime checks that validate type assumptions:
 * @code
 * ; Optimized: assume x is integer
 * GUARD_TYPE x, INTEGER  ; Deopt if x is not integer
 * ADD_INT r1, x, 1       ; Fast integer addition
 * @endcode
 * If a guard fails, execution deoptimizes to the interpreter.
 */

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

// Forward declaration
class MultiTierJITManager;

/**
 * @brief Trace statistics container for Tier2Compiler.
 */
struct TraceStats {
    size_t total_traces;              ///< Total traces recorded
    size_t active_traces;             ///< Currently active traces
    size_t total_compiled_code_size;  ///< Bytes of compiled code
    size_t max_code_cache_size;       ///< Maximum cache capacity
    float avg_trace_length;           ///< Average IR instructions per trace
    float avg_optimization_time_us;   ///< Average optimization time
};

/**
 * @brief Tier2Compiler - Aggressive Trace-Based JIT Compiler.
 * 
 * Performs trace-based just-in-time compilation with aggressive optimizations
 * for hot loops and frequently executed methods. This is the second-tier
 * compilation in the multi-tier JIT strategy.
 * 
 * Key features:
 * - Trace-based compilation (compiles execution paths, not full methods)
 * - Aggressive optimizations: loop unrolling, inlining, type specialization
 * - Guard-based specialization for type feedback
 * - Uses profiling data from Tier-1 for optimization decisions
 * - OSR (On-Stack Replacement) support for mid-execution transitions
 * 
 * Think of Tier-2 as the "heavy artillery" - it takes time to load,
 * but when it fires, your code runs at near-native speed.
 * 
 * Reference: "A Lightweight Method for Generating Multi-Tier JIT Compilation..."
 */
class Tier2Compiler {
public:
    /**
     * @brief IRInstruction - Intermediate Representation instruction.
     * 
     * IR is a higher-level representation used for optimization before
     * native code generation. It's architecture-independent and supports
     * aggressive optimizations.
     */
    struct IRInstruction {
        enum class Opcode : uint8_t {
            // Sentinel for uninitialized/invalid instructions
            INVALID = 0,

            // Memory operations - load/store values
            LOAD_CONST,     ///< Load constant from pool
            LOAD_LOCAL,     ///< Load from local variable slot
            STORE_LOCAL,    ///< Store to local variable slot
            POP,            ///< Pop value from stack

            // Arithmetic operations
            ADD, SUBTRACT, MULTIPLY, DIVIDE, MODULO,

            // Bitwise operations (operate on integers via double conversion)
            BITWISE_AND, BITWISE_OR, BITWISE_XOR, BITWISE_NOT,
            LEFT_SHIFT, RIGHT_SHIFT,
            NEGATE,

            // Logic operations
            NOT, EQUAL, NOT_EQUAL, LESS, GREATER,

            // Control flow
            JUMP, JUMP_IF_FALSE, JUMP_IF_TRUE,
            LOOP_BACK,      ///< Back-edge to loop header

            // Specialization - guards for type assumptions
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

        uint32_t operand1;   ///< First operand (register index, constant index, etc.)
        uint32_t operand2;   ///< Second operand (for binary operations)
        void* data;          ///< Additional data (constants, types, pointers)
    };

    /**
     * @brief ExecutionTrace - Recorded execution path for compilation.
     * 
     * A trace represents a single execution path through hot code.
     * Multiple traces may exist for the same method (different type specializations).
     * 
     * Trace lifecycle:
     * 1. Recording: Execute and record instructions
     * 2. Optimization: Apply optimization passes
     * 3. Compilation: Generate native code
     * 4. Execution: Run native code with guards
     * 5. Deoptimization: Fall back if guards fail
     */
    struct ExecutionTrace {
        uint64_t trace_id;                              ///< Unique trace identifier
        uint64_t method_id;                             ///< Parent method identifier
        uint64_t loop_entry_pc;                         ///< PC of loop entry (backward jump target)

        std::vector<IRInstruction> ir_instructions;     ///< Optimized IR
        std::vector<uint64_t> guard_conditions;         ///< Type guards for deoptimization

        int64_t execution_count;                        ///< Number of executions
        int64_t compilation_time_us;                    ///< Time spent compiling
        uint64_t compiled_code_address;                 ///< Native code entry point
        size_t compiled_code_size;                      ///< Size of compiled code

        std::vector<std::pair<uint64_t, uint64_t>> inline_cache_entries;  ///< Method dispatch caches
    };

    Tier2Compiler();
    ~Tier2Compiler();

    /**
     * @brief Record execution trace by tracing through bytecode interpreter.
     * @param method_id The method being traced.
     * @param bytecode The bytecode of the method.
     * @param start_pc Starting bytecode PC (usually loop header).
     * @param profiler Profiler with type information.
     * @return Recorded execution trace.
     * 
     * Recording process:
     * 1. Execute bytecode in interpreter mode
     * 2. Record each instruction to IR
     * 3. Track type information from profiler
     * 4. Detect loop back-edges (trace end)
     * 5. Return trace for optimization
     */
    std::unique_ptr<ExecutionTrace> recordTrace(
        uint64_t method_id,
        const Chunk& bytecode,
        uint64_t start_pc,
        const HotSpotProfiler& profiler);

    /**
     * @brief Optimize a recorded trace with aggressive techniques.
     * @param trace The trace to optimize.
     * @return Optimized trace.
     * 
     * Optimization passes (applied in order):
     * 1. Type specialization - use type info from profiler
     * 2. Constant folding - pre-compute constant expressions
     * 3. Dead code elimination - remove unused values
     * 4. Method inlining - inline hot method calls
     * 5. Loop unrolling - reduce loop overhead
     * 6. Guard insertion - add type checks for assumptions
     */
    std::unique_ptr<ExecutionTrace> optimizeTrace(const ExecutionTrace& trace);

    /**
     * @brief Compile optimized trace to native code.
     * @param trace The optimized trace.
     * @return Compiled code address, or 0 on failure.
     * 
     * Code generation process:
     * 1. Lower IR to architecture-specific instructions
     * 2. Allocate registers (linear scan or graph coloring)
     * 3. Emit machine code to executable memory
     * 4. Insert guard checks for deoptimization
     * 5. Return entry point address
     */
    uint64_t compileTrace(const ExecutionTrace& trace);

    /**
     * @brief Execute compiled trace.
     * @param trace_id ID of the trace to execute.
     * @param context Execution context (stack, locals, etc.).
     * @return true if execution succeeded.
     * 
     * Execution jumps to compiled_code_address and runs native code.
     * If a guard fails, execution deoptimizes to the interpreter.
     */
    bool executeTrace(uint64_t trace_id, void* context);

    /**
     * @brief Find a compiled trace by method and loop entry.
     * @param method_id The method containing the trace.
     * @param loop_entry_pc Loop header bytecode offset.
     * @return Trace ID, or 0 if not found.
     */
    uint64_t findTrace(uint64_t method_id, uint64_t loop_entry_pc);

    /**
     * @brief Register OSR entry for deoptimization support.
     * 
     * OSR (On-Stack Replacement) allows transitioning from interpreted
     * to compiled code mid-execution. This is crucial for long-running
     * loops that are already executing when compilation completes.
     */
    void registerOSREntry(uint64_t trace_id, uint64_t method_id, uint64_t bytecode_pc);

    /**
     * @brief Check if a trace has already failed compilation.
     * @param method_id The method ID.
     * @param loop_entry_pc The loop entry PC.
     * @return true if compilation previously failed (don't retry).
     * 
     * Failed traces are cached to avoid repeated compilation attempts
     * on code that can't be compiled (e.g., complex control flow).
     */
    bool isTraceFailed(uint64_t method_id, uint64_t loop_entry_pc) const {
        return failed_traces_.count(method_id ^ (loop_entry_pc * 2654435761ULL)) > 0;
    }

    /**
     * @brief Set the pointer to the VM's globals map.
     * @param globals Pointer to std::unordered_map<std::string, Value>.
     * 
     * Must be called before any trace compilation that uses global variables.
     * The JIT uses this for direct global variable access in compiled code.
     */
    void setGlobalsMap(void* globals) { globals_map_ = globals; }

    // === Optimization Passes ===

    /**
     * @brief Loop unrolling for frequently executed loops.
     * @param trace Original trace.
     * @param unroll_factor How many times to unroll.
     * @return Unrolled trace.
     * 
     * Unrolling reduces loop overhead by executing multiple iterations
     * per loop check. Trade-off: larger code size for faster execution.
     * 
     * Example (unroll factor 4):
     * @code
     * Before: for (i = 0; i < n; i++) { body }
     * After:  for (i = 0; i < n; i += 4) { body; body; body; body }
     * @endcode
     */
    std::unique_ptr<ExecutionTrace> unrollLoop(
        const ExecutionTrace& trace, int unroll_factor);

    /**
     * @brief Method inlining based on profiling data.
     * @param trace Trace to inline into.
     * @param profiler Profiling data for decisions.
     * @return Trace with inlined methods.
     * 
     * Inlining eliminates call overhead and enables cross-method optimization.
     * Decisions based on:
     * - Call frequency (hot calls are inlined)
     * - Method size (small methods are better candidates)
     * - Type stability (stable types enable better inlining)
     */
    std::unique_ptr<ExecutionTrace> inlineMethods(
        const ExecutionTrace& trace,
        const HotSpotProfiler& profiler);

    /**
     * @brief Type specialization using guards.
     * @param trace Trace to specialize.
     * @param profiler Type feedback from profiler.
     * @return Specialized trace with guards.
     * 
     * Type specialization generates type-specific code:
     * - Integer operations use integer instructions
     * - Known classes use direct method dispatch
     * - Guards verify assumptions at runtime
     * 
     * If a guard fails, execution deoptimizes and recompiles.
     */
    std::unique_ptr<ExecutionTrace> specializeTypes(
        const ExecutionTrace& trace,
        const HotSpotProfiler& profiler);

    /**
     * @brief Get compiled trace by ID.
     * @param trace_id The trace ID to retrieve.
     * @return Pointer to ExecutionTrace, or nullptr if not found.
     */
    const ExecutionTrace* getCompiledTrace(uint64_t trace_id) const;

    /**
     * @brief Get trace statistics.
     * @return TraceStats with current metrics.
     */
    TraceStats getTraceStats() const;

    /**
     * @brief Clear trace cache.
     * 
     * Frees all compiled traces and resets state.
     * Use sparingly - causes all hot code to recompile.
     */
    void clearTraceCache();

private:
    /// Map from trace_id to execution trace
    std::unordered_map<uint64_t, std::unique_ptr<ExecutionTrace>> traces_;

    /// Set of (method_id ^ loop_pc) that failed compilation - don't retry
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
    size_t total_traces_;                 ///< Total traces recorded
    int64_t total_optimization_time_us_;  ///< Total optimization time

    /// Optimization counters (atomic for thread safety)
    std::atomic<size_t> loop_unrollings_{0};
    std::atomic<size_t> method_inlinings_{0};
    std::atomic<size_t> type_specializations_{0};

public:
    /// Record optimization counts (called internally)
    void recordLoopUnrolling() { loop_unrollings_++; }
    void recordMethodInlining() { method_inlinings_++; }
    void recordTypeSpecialization() { type_specializations_++; }
    
    /// Get optimization counts
    size_t getLoopUnrollings() const { return loop_unrollings_.load(); }
    size_t getMethodInlinings() const { return method_inlinings_.load(); }
    size_t getTypeSpecializations() const { return type_specializations_.load(); }

    /**
     * @brief Convert bytecode instructions to IR.
     * @param bytecode The bytecode chunk.
     * @param start_pc Starting program counter.
     * @param end_pc Ending program counter (trace end).
     * @return Vector of IR instructions.
     */
    std::vector<IRInstruction> convertToIR(
        const Chunk& bytecode, uint64_t start_pc, uint64_t end_pc);

    /**
     * @brief Add type guard to trace for specialization.
     * @param trace Trace to add guard to.
     * @param value_id Value being guarded.
     * @param expected_type Expected type string.
     * 
     * Guards are checked at runtime. If the guard fails:
     * 1. Execution deoptimizes to interpreter
     * 2. Profiler records the new type
     * 3. Trace is recompiled with updated assumptions
     */
    void addTypeGuard(ExecutionTrace& trace, uint64_t value_id,
                     const std::string& expected_type);

    /**
     * @brief Allocate space in code cache.
     * @param size Bytes to allocate.
     * @return Pointer to allocated executable memory.
     */
    uint8_t* allocateCodeSpace(size_t size);

    /**
     * @brief Generate native code from IR instructions.
     * @param ir Vector of IR instructions.
     * @return Address of generated native code.
     */
    uint64_t generateNativeCode(const std::vector<IRInstruction>& ir);

#if defined(__aarch64__) || defined(__arm64__)
    /**
     * @brief Compile optimized trace to native ARM64/AArch64 code.
     * @param trace The optimized trace.
     * @return trace_id on success, 0 on failure.
     * 
     * ARM64 code generation:
     * - Uses AArch64 instruction set
     * - Follows ARM calling convention
     * - Supports SIMD for vector operations
     */
    uint64_t compileTraceARM64(const ExecutionTrace& trace);
#endif
};

} // namespace neutron::jit

#endif // NEUTRON_JIT_TIER2_COMPILER_H
