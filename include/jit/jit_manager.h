#ifndef NEUTRON_JIT_MANAGER_H
#define NEUTRON_JIT_MANAGER_H

#include "jit_config.h"
#include "jit_profiler.h"
#include "jit_tier1.h"
#include "jit_tier2.h"
#include "compiler/bytecode.h"
#include <memory>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <vector>

namespace neutron::jit {

/**
 * Multi-Tier JIT Manager
 * 
 * Coordinates the execution and compilation across Tier-1 and Tier-2 JIT compilers.
 * Manages the complete multi-tier JIT compilation workflow as described in the
 * research paper "A Lightweight Method for Generating Multi-Tier JIT Compilation
 * Virtual Machine in a Meta-Tracing Compiler Framework"
 * 
 * Execution Flow:
 * 1. Code starts in the interpreter
 * 2. When a backward jump (loop) exceeds TIER1_COMPILATION_THRESHOLD:
 *    - Compile to Tier-1 (lightweight threaded code)
 * 3. When Tier-1 code exceeds TIER2_COMPILATION_THRESHOLD:
 *    - Record execution trace
 *    - Compile to Tier-2 (tracing JIT with optimizations)
 * 4. Execution transitions to appropriate tier based on profiling
 */
class MultiTierJITManager {
public:
    /**
     * Execution context for a method/function call
     */
    struct ExecutionFrame {
        uint64_t method_id;
        const Chunk* chunk;
        uint64_t bytecode_pc;
        void* stack_pointer;
        void* local_variables;
        CompilationTier current_tier;
        std::chrono::high_resolution_clock::time_point tier_enter_time;
    };

    /**
     * JIT compilation event for monitoring
     */
    struct CompilationEvent {
        enum class Type : uint8_t {
            TIER1_COMPILATION_TRIGGERED,
            TIER1_COMPILATION_COMPLETED,
            TIER2_COMPILATION_TRIGGERED,
            TIER2_COMPILATION_COMPLETED,
            TIER_TRANSITION,
            COMPILATION_FAILED,
        };

        Type type;
        uint64_t method_id;
        uint64_t timestamp_us;
        CompilationTier target_tier;
        int64_t duration_us;
    };

    MultiTierJITManager();
    ~MultiTierJITManager();

    /**
     * Initialize the JIT manager
     * @param enable_monitoring Enable compilation event monitoring
     * @return true if initialization succeeded
     */
    bool initialize(bool enable_monitoring = false);

    /**
     * Request compilation for a method/function
     * @param method_id Unique method identifier
     * @param bytecode The bytecode of the method
     * @param current_tier Current execution tier
     * @return true if compilation was triggered
     */
    bool requestCompilation(uint64_t method_id, const Chunk& bytecode,
                           CompilationTier current_tier);

    /**
     * Record bytecode execution for profiling
     * @param method_id The method being executed
     * @param bytecode_pc The program counter (instruction offset)
     * @return The next tier to execute in (may be different from current)
     */
    CompilationTier recordExecution(uint64_t method_id, uint64_t bytecode_pc);

    /**
     * Handle tier transition
     * @param from_tier Source compilation tier
     * @param to_tier Target compilation tier
     * @param method_id The method transitioning
     * @param frame Current execution frame
     * @return true if transition succeeded
     */
    bool transitionTier(CompilationTier from_tier, CompilationTier to_tier,
                       uint64_t method_id, ExecutionFrame& frame);

    /**
     * Execute compiled code at specified tier
     * @param method_id The method to execute
     * @param tier The compilation tier to use
     * @param frame Execution context
     * @return true if execution succeeded
     */
    bool executeCompiledCode(uint64_t method_id, CompilationTier tier,
                            ExecutionFrame& frame);

    /**
     * Get profiling information for a method
     */
    const HotSpotProfiler::MethodProfile* getMethodProfile(uint64_t method_id) const;

    /**
     * Get current JIT statistics
     */
    struct JITStatistics {
        // Compilation counts
        size_t total_tier1_compilations;
        size_t total_tier2_compilations;
        size_t failed_compilations;

        // Performance metrics
        int64_t total_compilation_time_us;
        int64_t total_execution_time_us;
        int64_t warmup_duration_us;

        // Cache metrics
        size_t tier1_code_cache_size;
        size_t tier2_code_cache_size;
        float cache_hit_ratio;

        // Optimizations applied
        size_t loop_unrollings;
        size_t method_inlinings;
        size_t type_specializations;

        // Tier distribution
        int64_t time_in_interpreter;
        int64_t time_in_tier1;
        int64_t time_in_tier2;
    };

    JITStatistics getStatistics() const;

    /**
     * Check if JIT is in warmup phase
     */
    bool isInWarmupPhase() const;

    /**
     * Force recompilation of a method
     * @param method_id The method to recompile
     * @param force_tier Which tier to compile to (use TIER2 for full optimization)
     */
    bool recompile(uint64_t method_id, CompilationTier force_tier);

    /**
     * Clear all compiled code and reset JIT state
     */
    void reset();

    /**
     * Register compilation event listener
     * @param callback Function to call on compilation events
     */
    using EventCallback = std::function<void(const CompilationEvent&)>;
    void registerEventListener(EventCallback callback);

    /**
     * Access the Tier-2 compiler directly for fast-path execution
     */
    Tier2Compiler* getTier2Compiler() { return tier2_compiler_.get(); }

private:
    /// Profiler for hot-spot detection
    std::unique_ptr<HotSpotProfiler> profiler_;

    /// Tier-1 (lightweight) compiler
    std::unique_ptr<Tier1Compiler> tier1_compiler_;

    /// Tier-2 (tracing JIT) compiler
    std::unique_ptr<Tier2Compiler> tier2_compiler_;

    /// Execution frames stack for context management
    std::vector<ExecutionFrame> frame_stack_;

    /// Compilation event listeners
    std::vector<EventCallback> event_listeners_;

    /// Whether monitoring is enabled
    bool monitoring_enabled_;

    /// Statistics
    std::atomic<size_t> tier1_compilations_{0};
    std::atomic<size_t> tier2_compilations_{0};
    std::atomic<size_t> failed_compilations_{0};
    std::atomic<int64_t> total_compilation_time_us_{0};
    std::chrono::high_resolution_clock::time_point start_time_;

    /// Execution time per tier
    std::atomic<int64_t> interpreter_time_us_{0};
    std::atomic<int64_t> tier1_time_us_{0};
    std::atomic<int64_t> tier2_time_us_{0};

    /**
     * Emit a compilation event
     */
    void emitEvent(const CompilationEvent& event);

    /**
     * Decide whether to compile and to which tier
     */
    CompilationTier decideCompilationTier(uint64_t method_id,
                                         CompilationTier current_tier);

    /**
     * Record tier transition timing
     */
    void recordTierTransition(CompilationTier from_tier, CompilationTier to_tier,
                             int64_t duration_us);
};

} // namespace neutron::jit

#endif // NEUTRON_JIT_MANAGER_H
