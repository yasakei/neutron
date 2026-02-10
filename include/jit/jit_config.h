#ifndef NEUTRON_JIT_CONFIG_H
#define NEUTRON_JIT_CONFIG_H

#include <cstdint>
#include <chrono>

namespace neutron::jit {

/**
 * JIT Configuration constants based on multi-tier JIT compilation research
 * Reference: "A Lightweight Method for Generating Multi-Tier JIT Compilation Virtual Machine"
 */

// ============================================================================
// Tier-1: Lightweight Compilation Configuration
// ============================================================================

/// Threshold for backward jumps (loops) to trigger Tier-1 compilation
/// This represents hot-spot detection in Tier-1
constexpr int32_t TIER1_COMPILATION_THRESHOLD = 50;

/// Maximum size of threaded code cache in bytes (1MB)
constexpr size_t TIER1_CODE_CACHE_SIZE = 1 * 1024 * 1024;

/// Initial capacity for Tier-1 compiled functions
constexpr size_t TIER1_FUNCTION_CACHE_CAPACITY = 1000;

// ============================================================================
// Tier-2: Tracing JIT Compilation Configuration
// ============================================================================

/// Threshold for loop execution count to trigger Tier-2 compilation
/// When a backward jump exceeds this, code is compiled to Tier-2
constexpr int32_t TIER2_COMPILATION_THRESHOLD = 50;

/// Maximum size of tracing JIT code cache in bytes (4MB)
constexpr size_t TIER2_CODE_CACHE_SIZE = 4 * 1024 * 1024;

/// Maximum trace length (number of bytecode instructions)
constexpr size_t MAX_TRACE_LENGTH = 5000;

/// Maximum number of loop traces to keep
constexpr size_t MAX_LOOP_TRACES = 500;

// ============================================================================
// Profiling Configuration
// ============================================================================

/// Frequency of profiling samples
constexpr int32_t PROFILE_SAMPLE_INTERVAL = 100;

/// Number of method invocations to track
constexpr size_t MAX_PROFILED_METHODS = 10000;

/// Number of loop invocations to track
constexpr size_t MAX_PROFILED_LOOPS = 5000;

// ============================================================================
// Optimization Configuration
// ============================================================================

/// Enable inline caching for method dispatch
constexpr bool ENABLE_INLINE_CACHING = true;

/// Enable loop unrolling in Tier-2
constexpr bool ENABLE_LOOP_UNROLLING = true;

/// Enable method inlining in Tier-2
constexpr bool ENABLE_METHOD_INLINING = true;

/// Enable shallow tracing for Tier-1 (avoid side effects)
constexpr bool ENABLE_SHALLOW_TRACING = true;

/// Enable type specialization in Tier-2
constexpr bool ENABLE_TYPE_SPECIALIZATION = true;

/// Maximum inlining depth
constexpr int32_t MAX_INLINING_DEPTH = 3;

// ============================================================================
// Performance Monitoring
// ============================================================================

/// Enable performance monitoring and statistics
constexpr bool ENABLE_PERF_MONITORING = true;

/// Time window for warm-up phase detection (milliseconds)
constexpr int64_t WARMUP_WINDOW_MS = 5000;

/// Sample size for warmup detection
constexpr size_t WARMUP_SAMPLE_SIZE = 100;

// ============================================================================
// Compilation Strategies
// ============================================================================

enum class CompilationTier : uint8_t {
    INTERPRETER = 0,  ///< Bytecode interpretation only
    TIER1 = 1,        ///< Lightweight threaded code generation
    TIER2 = 2,        ///< Tracing JIT with aggressive optimizations
};

enum class OptimizationLevel : uint8_t {
    NONE = 0,      ///< No optimization
    QUICK = 1,     ///< Quick compilation, moderate optimization (Tier-1)
    FULL = 2,      ///< Full optimization with aggressive techniques (Tier-2)
};

} // namespace neutron::jit

#endif // NEUTRON_JIT_CONFIG_H
