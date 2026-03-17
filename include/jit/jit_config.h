#ifndef NEUTRON_JIT_CONFIG_H
#define NEUTRON_JIT_CONFIG_H

#include <cstdint>
#include <chrono>
#include <atomic>

namespace neutron::jit {

/**
 * JIT Configuration - compile-time defaults and runtime-configurable settings.
 * 
 * Reference: "A Lightweight Method for Generating Multi-Tier JIT Compilation Virtual Machine"
 * 
 * Usage:
 * @code
 * // Read current thresholds
 * auto threshold = JITConfig::getTier1Threshold();
 * 
 * // Adjust at runtime based on workload
 * JITConfig::setTier1Threshold(100);  // More conservative compilation
 * JITConfig::setTier2Threshold(25);   // Aggressive Tier-2 compilation
 * @endcode
 */

// ============================================================================
// Compile-Time Default Configuration
// ============================================================================

// Tier-1: Lightweight Compilation Configuration
constexpr int32_t DEFAULT_TIER1_COMPILATION_THRESHOLD = 50;
constexpr size_t DEFAULT_TIER1_CODE_CACHE_SIZE = 1 * 1024 * 1024;  // 1MB
constexpr size_t DEFAULT_TIER1_FUNCTION_CACHE_CAPACITY = 1000;

// Tier-2: Tracing JIT Compilation Configuration
constexpr int32_t DEFAULT_TIER2_COMPILATION_THRESHOLD = 50;
constexpr size_t DEFAULT_TIER2_CODE_CACHE_SIZE = 4 * 1024 * 1024;  // 4MB
constexpr size_t DEFAULT_MAX_TRACE_LENGTH = 5000;
constexpr size_t DEFAULT_MAX_LOOP_TRACES = 500;

// Profiling Configuration
constexpr int32_t DEFAULT_PROFILE_SAMPLE_INTERVAL = 100;
constexpr size_t DEFAULT_MAX_PROFILED_METHODS = 10000;
constexpr size_t DEFAULT_MAX_PROFILED_LOOPS = 5000;

// Optimization Configuration
constexpr bool DEFAULT_ENABLE_INLINE_CACHING = true;
constexpr bool DEFAULT_ENABLE_LOOP_UNROLLING = true;
constexpr bool DEFAULT_ENABLE_METHOD_INLINING = true;
constexpr bool DEFAULT_ENABLE_SHALLOW_TRACING = true;
constexpr bool DEFAULT_ENABLE_TYPE_SPECIALIZATION = true;
constexpr int32_t DEFAULT_MAX_INLINING_DEPTH = 3;

// Performance Monitoring
constexpr bool DEFAULT_ENABLE_PERF_MONITORING = true;
constexpr int64_t DEFAULT_WARMUP_WINDOW_MS = 5000;
constexpr size_t DEFAULT_WARMUP_SAMPLE_SIZE = 100;

// ============================================================================
// Runtime-Configurable JIT Settings
// ============================================================================

/**
 * @brief JITConfig - Runtime configuration for JIT compilation.
 * 
 * Provides thread-safe runtime adjustment of JIT parameters.
 * All settings use atomics for lock-free access during execution.
 * 
 * Default values match the compile-time constants above.
 */
struct JITConfig {
    // Compilation thresholds (runtime adjustable)
    static std::atomic<int32_t> tier1CompilationThreshold;
    static std::atomic<int32_t> tier2CompilationThreshold;
    
    // Cache sizes (set at initialization, read-only during execution)
    static std::atomic<size_t> tier1CodeCacheSize;
    static std::atomic<size_t> tier2CodeCacheSize;
    
    // Trace limits
    static std::atomic<size_t> maxTraceLength;
    static std::atomic<size_t> maxLoopTraces;
    
    // Feature flags
    static std::atomic<bool> enableInlineCaching;
    static std::atomic<bool> enableLoopUnrolling;
    static std::atomic<bool> enableMethodInlining;
    static std::atomic<bool> enableShallowTracing;
    static std::atomic<bool> enableTypeSpecialization;
    
    // Monitoring
    static std::atomic<bool> enablePerfMonitoring;
    
    // Getters - Thread-safe, lock-free access
    
    /**
     * @brief Get Tier-1 compilation threshold.
     * @return Number of backward jumps before Tier-1 compilation.
     */
    static int32_t getTier1Threshold() {
        return tier1CompilationThreshold.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get Tier-2 compilation threshold.
     * @return Execution count before Tier-2 compilation.
     */
    static int32_t getTier2Threshold() {
        return tier2CompilationThreshold.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get Tier-1 code cache size.
     * @return Cache size in bytes.
     */
    static size_t getTier1CodeCacheSize() {
        return tier1CodeCacheSize.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get Tier-2 code cache size.
     * @return Cache size in bytes.
     */
    static size_t getTier2CodeCacheSize() {
        return tier2CodeCacheSize.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get maximum trace length.
     * @return Maximum IR instructions per trace.
     */
    static size_t getMaxTraceLength() {
        return maxTraceLength.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get maximum number of loop traces.
     * @return Maximum traces to keep.
     */
    static size_t getMaxLoopTraces() {
        return maxLoopTraces.load(std::memory_order_relaxed);
    }
    
    // Setters - Thread-safe modification
    
    /**
     * @brief Set Tier-1 compilation threshold.
     * @param threshold Number of backward jumps before Tier-1 compilation.
     * 
     * Higher values = more conservative compilation (less aggressive).
     * Lower values = more aggressive compilation (more code compiled).
     * 
     * Typical range: 10-500
     */
    static void setTier1Threshold(int32_t threshold) {
        tier1CompilationThreshold.store(
            threshold > 0 ? threshold : DEFAULT_TIER1_COMPILATION_THRESHOLD,
            std::memory_order_relaxed
        );
    }
    
    /**
     * @brief Set Tier-2 compilation threshold.
     * @param threshold Execution count before Tier-2 compilation.
     * 
     * Higher values = wait for hotter code before aggressive optimization.
     * Lower values = compile to Tier-2 more aggressively.
     * 
     * Typical range: 10-500
     */
    static void setTier2Threshold(int32_t threshold) {
        tier2CompilationThreshold.store(
            threshold > 0 ? threshold : DEFAULT_TIER2_COMPILATION_THRESHOLD,
            std::memory_order_relaxed
        );
    }
    
    /**
     * @brief Set maximum trace length.
     * @param length Maximum IR instructions per trace.
     * 
     * Longer traces = more optimization opportunities but slower compilation.
     * Shorter traces = faster compilation but may miss optimization chances.
     * 
     * Typical range: 100-10000
     */
    static void setMaxTraceLength(size_t length) {
        maxTraceLength.store(
            length > 0 ? length : DEFAULT_MAX_TRACE_LENGTH,
            std::memory_order_relaxed
        );
    }
    
    /**
     * @brief Set maximum number of loop traces.
     * @param count Maximum traces to keep in cache.
     * 
     * More traces = better coverage but more memory usage.
     * Fewer traces = less memory but may recompile more often.
     * 
     * Typical range: 50-2000
     */
    static void setMaxLoopTraces(size_t count) {
        maxLoopTraces.store(
            count > 0 ? count : DEFAULT_MAX_LOOP_TRACES,
            std::memory_order_relaxed
        );
    }
    
    /**
     * @brief Enable or disable inline caching.
     * @param enabled True to enable inline caching.
     */
    static void setInlineCachingEnabled(bool enabled) {
        enableInlineCaching.store(enabled, std::memory_order_relaxed);
    }
    
    /**
     * @brief Enable or disable loop unrolling.
     * @param enabled True to enable loop unrolling.
     */
    static void setLoopUnrollingEnabled(bool enabled) {
        enableLoopUnrolling.store(enabled, std::memory_order_relaxed);
    }
    
    /**
     * @brief Enable or disable method inlining.
     * @param enabled True to enable method inlining.
     */
    static void setMethodInliningEnabled(bool enabled) {
        enableMethodInlining.store(enabled, std::memory_order_relaxed);
    }
    
    /**
     * @brief Enable or disable type specialization.
     * @param enabled True to enable type specialization.
     */
    static void setTypeSpecializationEnabled(bool enabled) {
        enableTypeSpecialization.store(enabled, std::memory_order_relaxed);
    }
    
    /**
     * @brief Enable or disable performance monitoring.
     * @param enabled True to enable monitoring.
     * 
     * Monitoring adds overhead but provides valuable statistics.
     * Disable for production builds if statistics aren't needed.
     */
    static void setPerfMonitoringEnabled(bool enabled) {
        enablePerfMonitoring.store(enabled, std::memory_order_relaxed);
    }
    
    /**
     * @brief Check if inline caching is enabled.
     * @return True if inline caching is enabled.
     */
    static bool isInlineCachingEnabled() {
        return enableInlineCaching.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Check if loop unrolling is enabled.
     * @return True if loop unrolling is enabled.
     */
    static bool isLoopUnrollingEnabled() {
        return enableLoopUnrolling.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Check if method inlining is enabled.
     * @return True if method inlining is enabled.
     */
    static bool isMethodInliningEnabled() {
        return enableMethodInlining.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Check if type specialization is enabled.
     * @return True if type specialization is enabled.
     */
    static bool isTypeSpecializationEnabled() {
        return enableTypeSpecialization.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Check if performance monitoring is enabled.
     * @return True if performance monitoring is enabled.
     */
    static bool isPerfMonitoringEnabled() {
        return enablePerfMonitoring.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Reset all settings to default values.
     */
    static void resetToDefaults() {
        tier1CompilationThreshold.store(DEFAULT_TIER1_COMPILATION_THRESHOLD, std::memory_order_relaxed);
        tier2CompilationThreshold.store(DEFAULT_TIER2_COMPILATION_THRESHOLD, std::memory_order_relaxed);
        tier1CodeCacheSize.store(DEFAULT_TIER1_CODE_CACHE_SIZE, std::memory_order_relaxed);
        tier2CodeCacheSize.store(DEFAULT_TIER2_CODE_CACHE_SIZE, std::memory_order_relaxed);
        maxTraceLength.store(DEFAULT_MAX_TRACE_LENGTH, std::memory_order_relaxed);
        maxLoopTraces.store(DEFAULT_MAX_LOOP_TRACES, std::memory_order_relaxed);
        enableInlineCaching.store(DEFAULT_ENABLE_INLINE_CACHING, std::memory_order_relaxed);
        enableLoopUnrolling.store(DEFAULT_ENABLE_LOOP_UNROLLING, std::memory_order_relaxed);
        enableMethodInlining.store(DEFAULT_ENABLE_METHOD_INLINING, std::memory_order_relaxed);
        enableShallowTracing.store(DEFAULT_ENABLE_SHALLOW_TRACING, std::memory_order_relaxed);
        enableTypeSpecialization.store(DEFAULT_ENABLE_TYPE_SPECIALIZATION, std::memory_order_relaxed);
        enablePerfMonitoring.store(DEFAULT_ENABLE_PERF_MONITORING, std::memory_order_relaxed);
    }
    
    /**
     * @brief Initialize configuration with default values.
     * 
     * Call once at program startup to ensure atomic variables are initialized.
     */
    static void initialize() {
        resetToDefaults();
    }
    
    // Preset configurations for common scenarios
    
    /**
     * @brief Use aggressive compilation settings.
     * 
     * Best for: Long-running applications, compute-heavy workloads
     * Trade-off: Higher compilation overhead, better peak performance
     */
    static void useAggressiveMode() {
        setTier1Threshold(25);    // Compile to Tier-1 quickly
        setTier2Threshold(25);    // Compile to Tier-2 quickly
        setMaxTraceLength(10000); // Allow longer traces for more optimization
        setMaxLoopTraces(1000);   // Keep more traces
        setLoopUnrollingEnabled(true);
        setMethodInliningEnabled(true);
        setTypeSpecializationEnabled(true);
    }
    
    /**
     * @brief Use conservative compilation settings.
     * 
     * Best for: Short-running scripts, memory-constrained environments
     * Trade-off: Lower compilation overhead, may miss some optimizations
     */
    static void useConservativeMode() {
        setTier1Threshold(200);   // Wait for hotter code
        setTier2Threshold(200);   // Only compile very hot code
        setMaxTraceLength(1000);  // Shorter traces
        setMaxLoopTraces(100);    // Fewer traces
        setLoopUnrollingEnabled(false);  // Disable aggressive opts
    }
    
    /**
     * @brief Use balanced settings (default).
     * 
     * Best for: General-purpose workloads
     * Trade-off: Balanced compilation overhead vs. performance
     */
    static void useBalancedMode() {
        resetToDefaults();
    }
};

// ============================================================================
// Legacy constants (for backward compatibility)
// ============================================================================
// These are now macros that reference the runtime config
#define TIER1_COMPILATION_THRESHOLD (JITConfig::getTier1Threshold())
#define TIER2_COMPILATION_THRESHOLD (JITConfig::getTier2Threshold())
#define TIER1_CODE_CACHE_SIZE (JITConfig::getTier1CodeCacheSize())
#define TIER2_CODE_CACHE_SIZE (JITConfig::getTier2CodeCacheSize())
#define MAX_TRACE_LENGTH (JITConfig::getMaxTraceLength())
#define MAX_LOOP_TRACES (JITConfig::getMaxLoopTraces())

// ============================================================================
// Compilation Strategies (enum kept for compatibility)
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
