#ifndef NEUTRON_JIT_PROFILER_H
#define NEUTRON_JIT_PROFILER_H

#include "jit_config.h"
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <memory>
#include <atomic>
#include <chrono>

namespace neutron::jit {

/**
 * Hot-spot profiler for identifying compilation candidates
 * Implements profiling mechanism from the research paper
 */
class HotSpotProfiler {
public:
    struct ExecutionProfile {
        uint64_t bytecode_offset;  ///< Bytecode offset (e.g., PC of backward jump)
        int64_t execution_count;   ///< Number of times executed
        int64_t last_compiled_at;  ///< Timestamp of last compilation
        CompilationTier compiled_tier; ///< Current compilation tier
        
        ExecutionProfile()
            : bytecode_offset(0), execution_count(0), 
              last_compiled_at(0), compiled_tier(CompilationTier::INTERPRETER) {}
    };

    struct MethodProfile {
        uint64_t method_id;
        int64_t call_count;
        int64_t total_execution_time_us;
        std::vector<uint64_t> loop_offsets;
        
        MethodProfile(uint64_t id) 
            : method_id(id), call_count(0), total_execution_time_us(0) {}
    };

    HotSpotProfiler();
    ~HotSpotProfiler();

    /**
     * Record execution of a bytecode location (e.g., backward jump)
     * @param method_id The method being executed
     * @param bytecode_offset The bytecode PC
     * @return true if this location should be compiled to next tier
     */
    bool recordExecution(uint64_t method_id, uint64_t bytecode_offset);

    /**
     * Record method invocation
     * @param method_id The method being called
     */
    void recordMethodCall(uint64_t method_id);

    /**
     * Record method execution time
     * @param method_id The method
     * @param duration_us Duration in microseconds
     */
    void recordMethodExecutionTime(uint64_t method_id, int64_t duration_us);

    /**
     * Check if a location is hot (should be compiled)
     * @param method_id The method
     * @param bytecode_offset The bytecode offset
     * @param current_tier Current compilation tier
     * @return true if should advance to next tier
     */
    bool isHot(uint64_t method_id, uint64_t bytecode_offset, 
               CompilationTier current_tier) const;

    /**
     * Get the execution profile for a location
     */
    const ExecutionProfile* getProfile(uint64_t method_id, 
                                        uint64_t bytecode_offset) const;

    /**
     * Get method profile
     */
    const MethodProfile* getMethodProfile(uint64_t method_id) const;

    /**
     * Reset statistics for a method (after recompilation)
     */
    void resetMethodProfile(uint64_t method_id);

    /**
     * Get total execution time across all methods
     */
    int64_t getTotalExecutionTime() const;

    /**
     * Get warm-up metrics
     */
    struct WarmupMetrics {
        int64_t warmup_duration_ms;
        size_t tier1_compilations;
        size_t tier2_compilations;
        int64_t peak_throughput;
    };

    WarmupMetrics getWarmupMetrics() const;

private:
    /// Map from (method_id, bytecode_offset) to execution profile
    std::unordered_map<uint64_t, ExecutionProfile> execution_profiles_;
    
    /// Map from method_id to method profile
    std::unordered_map<uint64_t, std::unique_ptr<MethodProfile>> method_profiles_;

    /// Timestamps for warmup detection
    std::chrono::high_resolution_clock::time_point start_time_;
    
    /// Total execution time
    std::atomic<int64_t> total_execution_time_us_{0};

    /// Number of compilations at each tier
    std::atomic<size_t> tier1_compilations_{0};
    std::atomic<size_t> tier2_compilations_{0};

    /// Get or create execution profile
    ExecutionProfile& getOrCreateProfile(uint64_t method_id, 
                                         uint64_t bytecode_offset);

    /// Get or create method profile
    MethodProfile& getOrCreateMethodProfile(uint64_t method_id);

    /// Hash function for (method_id, offset) pair
    static uint64_t hashKey(uint64_t method_id, uint64_t bytecode_offset);
};

} // namespace neutron::jit

#endif // NEUTRON_JIT_PROFILER_H
