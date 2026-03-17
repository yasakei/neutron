#ifndef NEUTRON_JIT_MANAGER_H
#define NEUTRON_JIT_MANAGER_H

/*
 * Code Documentation: Multi-Tier JIT Manager (jit_manager.h)
 * ==========================================================
 * 
 * This header defines the MultiTierJITManager - the conductor of Neutron's
 * Just-In-Time compilation orchestra. It coordinates compilation across
 * Tier-1 (threaded code) and Tier-2 (tracing JIT) compilers.
 * 
 * What This File Includes:
 * ------------------------
 * - MultiTierJITManager class: Main JIT coordination
 * - ExecutionFrame: Context for method execution
 * - CompilationEvent: Monitoring and profiling events
 * - JITStatistics: Performance metrics and profiling data
 * 
 * How It Works:
 * -------------
 * The JIT manager implements a multi-tier compilation strategy based on
 * the research paper "A Lightweight Method for Generating Multi-Tier JIT
 * Compilation Virtual Machine in a Meta-Tracing Compiler Framework":
 * 
 * 1. Interpreter Phase: All code starts here, being profiled
 * 2. Tier-1 Trigger: Hot loops (>TIER1_COMPILATION_THRESHOLD) → threaded code
 * 3. Tier-2 Trigger: Hot Tier-1 code (>TIER2_COMPILATION_THRESHOLD) → native trace
 * 4. Execution: Code runs at appropriate tier based on profiling
 * 
 * The manager tracks:
 * - Method execution counts and hot spots
 * - Compilation events and timing
 * - Tier transitions and performance metrics
 * - Code cache utilization
 * 
 * Adding Features:
 * ----------------
 * - New tiers: Add CompilationTier enum value, implement compiler class
 * - New metrics: Extend JITStatistics with additional counters
 * - Custom thresholds: Modify jit_config.h constants
 * - Event monitoring: Register listeners via registerEventListener()
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT call JIT methods from multiple threads without synchronization
 * - Do NOT modify bytecode during execution (creates inconsistent state)
 * - Do NOT bypass the profiler (tier decisions depend on accurate data)
 * - Do NOT clear code cache during active execution
 * 
 * Performance Considerations:
 * ---------------------------
 * - Warmup phase: JIT needs time to identify hot spots
 * - Compilation overhead: Balance optimization vs. compilation time
 * - Code cache: Limited executable memory, requires eviction policies
 * - Tier transitions: Have overhead, only transition when beneficial
 */

#include "jit_config.h"
#include "jit_profiler.h"
#include "jit_tier1.h"
#include "jit_tier2.h"
#include "jit_errors.h"
#include "compiler/bytecode.h"
#include <memory>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <vector>

namespace neutron::jit {

// Forward declarations
class Tier1Compiler;
class Tier2Compiler;
class HotSpotProfiler;

/**
 * @brief JITStatistics - Performance metrics and profiling data container.
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

/**
 * @brief MultiTierJITManager - The JIT Conductor.
 * 
 * Coordinates execution and compilation across Tier-1 and Tier-2 JIT compilers.
 * Manages the complete multi-tier JIT compilation workflow, making intelligent
 * decisions about when and what to compile based on runtime profiling.
 * 
 * Think of it as a smart manager that watches your code run, identifies the
 * hot spots, and compiles them to native code - but only when it's worth it.
 * 
 * Execution Flow:
 * @code
 * 1. Code starts in the interpreter (profiling enabled)
 * 2. When a backward jump (loop) exceeds TIER1_COMPILATION_THRESHOLD:
 *    → Compile to Tier-1 (lightweight threaded code)
 * 3. When Tier-1 code exceeds TIER2_COMPILATION_THRESHOLD:
 *    → Record execution trace
 *    → Compile to Tier-2 (tracing JIT with aggressive optimizations)
 * 4. Execution transitions to appropriate tier based on profiling
 * @endcode
 */
class MultiTierJITManager {
public:
    /**
     * @brief ExecutionFrame - Context for a method/function call.
     * 
     * Tracks the execution state of a method, including:
     * - Current compilation tier
     * - Bytecode program counter
     * - Stack and local variable pointers
     * - Timing information for profiling
     */
    struct ExecutionFrame {
        uint64_t method_id;                                          ///< Unique method identifier
        const Chunk* chunk;                                          ///< Bytecode chunk being executed
        uint64_t bytecode_pc;                                        ///< Current program counter
        void* stack_pointer;                                         ///< VM stack pointer
        void* local_variables;                                       ///< Local variable slot pointer
        CompilationTier current_tier;                                ///< Current compilation tier
        std::chrono::high_resolution_clock::time_point tier_enter_time;  ///< When we entered this tier
    };

    /**
     * @brief CompilationEvent - JIT compilation monitoring events.
     * 
     * Emitted when compilation events occur. Useful for:
     * - Performance monitoring
     * - Debugging JIT behavior
     * - Profiling compilation overhead
     */
    struct CompilationEvent {
        enum class Type : uint8_t {
            TIER1_COMPILATION_TRIGGERED,   ///< Tier-1 compilation started
            TIER1_COMPILATION_COMPLETED,   ///< Tier-1 compilation finished
            TIER2_COMPILATION_TRIGGERED,   ///< Tier-2 compilation started
            TIER2_COMPILATION_COMPLETED,   ///< Tier-2 compilation finished
            TIER_TRANSITION,               ///< Code moved between tiers
            COMPILATION_FAILED,            ///< Compilation error occurred
        };

        Type type;              ///< Event type
        uint64_t method_id;     ///< Method being compiled
        uint64_t timestamp_us;  ///< Event timestamp (microseconds)
        CompilationTier target_tier;  ///< Target compilation tier
        int64_t duration_us;    ///< Compilation duration (if applicable)
    };

    MultiTierJITManager();
    ~MultiTierJITManager();

    /**
     * @brief Initialize the JIT manager.
     * @param enable_monitoring Enable compilation event monitoring.
     * @return JITResult with error code if initialization failed.
     *
     * Must be called before any JIT operations. Initializes:
     * - Tier-1 and Tier-2 compilers
     * - Profiler for hot spot detection
     * - Code caches for compiled code
     */
    JITResult initialize(bool enable_monitoring = false);

    /**
     * @brief Request compilation for a method/function.
     * @param method_id Unique method identifier.
     * @param bytecode The bytecode of the method.
     * @param current_tier Current execution tier.
     * @return JITResult indicating success or compilation error.
     *
     * The JIT manager decides whether to compile based on profiling data.
     * Not all requests result in compilation (cold code may be ignored).
     */
    JITResult requestCompilation(uint64_t method_id, const Chunk& bytecode,
                           CompilationTier current_tier);

    /**
     * @brief Record bytecode execution for profiling.
     * @param method_id The method being executed.
     * @param bytecode_pc The program counter (instruction offset).
     * @return The next tier to execute in (may be different from current).
     * 
     * Called during interpretation to track execution frequency.
     * When thresholds are exceeded, triggers compilation.
     */
    CompilationTier recordExecution(uint64_t method_id, uint64_t bytecode_pc);

    /**
     * @brief Handle tier transition.
     * @param from_tier Source compilation tier.
     * @param to_tier Target compilation tier.
     * @param method_id The method transitioning.
     * @param frame Current execution frame.
     * @return JITResult with error code if transition failed.
     *
     * Transitions occur when profiling indicates a tier change is beneficial.
     * The manager handles state transfer between tiers.
     */
    JITResult transitionTier(CompilationTier from_tier, CompilationTier to_tier,
                       uint64_t method_id, ExecutionFrame& frame);

    /**
     * @brief Execute compiled code at specified tier.
     * @param method_id The method to execute.
     * @param tier The compilation tier to use.
     * @param frame Execution context.
     * @return JITResult indicating success or execution error.
     *
     * Dispatches to the appropriate compiled code based on tier.
     * Tier-1 uses threaded code, Tier-2 uses native traces.
     */
    JITResult executeCompiledCode(uint64_t method_id, CompilationTier tier,
                            ExecutionFrame& frame);

    /**
     * @brief Get profiling information for a method.
     * @param method_id The method to query.
     * @return Method profile, or nullptr if not found.
     */
    const HotSpotProfiler::MethodProfile* getMethodProfile(uint64_t method_id) const;

    /**
     * @brief Get current JIT statistics.
     * @return JITStatistics with current metrics.
     * 
     * Statistics include:
     * - Compilation counts (tier1, tier2, failed)
     * - Performance metrics (compilation time, execution time)
     * - Cache metrics (sizes, hit ratios)
     * - Optimization counts (inlining, unrolling, specialization)
     * - Tier distribution (time spent in each tier)
     */
    JITStatistics getStatistics() const;

    /**
     * @brief Check if JIT is in warmup phase.
     * @return true if still warming up.
     * 
     * During warmup, the JIT collects baseline profiling data
     * before making compilation decisions.
     */
    bool isInWarmupPhase() const;

    /**
     * @brief Force recompilation of a method.
     * @param method_id The method to recompile.
     * @param force_tier Which tier to compile to.
     * @return JITResult indicating success or recompilation error.
     *
     * Use for debugging or when profiling data has changed significantly.
     * Normal operation should not require forced recompilation.
     */
    JITResult recompile(uint64_t method_id, CompilationTier force_tier);

    /**
     * @brief Clear all compiled code and reset JIT state.
     * 
     * Frees all compiled code from caches and resets profiling.
     * Use sparingly - causes all hot code to recompile from scratch.
     */
    void reset();

    /**
     * @brief Register compilation event listener.
     * @param callback Function to call on compilation events.
     * 
     * Useful for monitoring, debugging, or adaptive optimization.
     * Callbacks are invoked synchronously during compilation.
     */
    using EventCallback = std::function<void(const CompilationEvent&)>;
    void registerEventListener(EventCallback callback);

    /**
     * @brief Access the Tier-2 compiler directly for fast-path execution.
     * @return Pointer to Tier2Compiler instance.
     * 
     * Advanced use only - typically managed automatically.
     */
    Tier2Compiler* getTier2Compiler() { return tier2_compiler_.get(); }

private:
    /// Profiler for hot-spot detection - watches which code is hot
    std::unique_ptr<HotSpotProfiler> profiler_;

    /// Tier-1 (lightweight) compiler - fast compilation, moderate optimization
    std::unique_ptr<Tier1Compiler> tier1_compiler_;

    /// Tier-2 (tracing JIT) compiler - slower compilation, aggressive optimization
    std::unique_ptr<Tier2Compiler> tier2_compiler_;

    /// Cache for Tier-1 compiled code (method_id -> ThreadedCode)
    std::unordered_map<uint64_t, std::unique_ptr<Tier1Compiler::ThreadedCode>> tier1_code_cache_;

    /// Execution frames stack for context management - tracks call hierarchy
    std::vector<ExecutionFrame> frame_stack_;

    /// Compilation event listeners - for monitoring and debugging
    std::vector<EventCallback> event_listeners_;

    /// Whether monitoring is enabled
    bool monitoring_enabled_;

    // Statistics - atomic for thread safety
    std::atomic<size_t> tier1_compilations_{0};   ///< Tier-1 compilations completed
    std::atomic<size_t> tier2_compilations_{0};   ///< Tier-2 compilations completed
    std::atomic<size_t> failed_compilations_{0};  ///< Failed compilations
    std::atomic<int64_t> total_compilation_time_us_{0};  ///< Total compilation time
    std::chrono::high_resolution_clock::time_point start_time_;  ///< JIT start time

    /// Execution time per tier - for performance analysis
    std::atomic<int64_t> interpreter_time_us_{0};  ///< Time spent interpreting
    std::atomic<int64_t> tier1_time_us_{0};        ///< Time in Tier-1 code
    std::atomic<int64_t> tier2_time_us_{0};        ///< Time in Tier-2 code

    /**
     * @brief Emit a compilation event to all listeners.
     * @param event The event to emit.
     */
    void emitEvent(const CompilationEvent& event);

    /**
     * @brief Decide whether to compile and to which tier.
     * @param method_id The method being considered.
     * @param current_tier Current execution tier.
     * @return Target compilation tier (or current if no change).
     */
    CompilationTier decideCompilationTier(uint64_t method_id,
                                         CompilationTier current_tier);

    /**
     * @brief Record tier transition timing for statistics.
     * @param from_tier Source tier.
     * @param to_tier Target tier.
     * @param duration_us Transition duration in microseconds.
     */
    void recordTierTransition(CompilationTier from_tier, CompilationTier to_tier,
                             int64_t duration_us);
};

} // namespace neutron::jit

#endif // NEUTRON_JIT_MANAGER_H
