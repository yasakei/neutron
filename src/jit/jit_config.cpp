/*
 * JIT Configuration - Runtime settings implementation
 */

#include "jit/jit_config.h"

namespace neutron::jit {

// Initialize static atomic variables with default values
std::atomic<int32_t> JITConfig::tier1CompilationThreshold(DEFAULT_TIER1_COMPILATION_THRESHOLD);
std::atomic<int32_t> JITConfig::tier2CompilationThreshold(DEFAULT_TIER2_COMPILATION_THRESHOLD);
std::atomic<size_t> JITConfig::tier1CodeCacheSize(DEFAULT_TIER1_CODE_CACHE_SIZE);
std::atomic<size_t> JITConfig::tier2CodeCacheSize(DEFAULT_TIER2_CODE_CACHE_SIZE);
std::atomic<size_t> JITConfig::maxTraceLength(DEFAULT_MAX_TRACE_LENGTH);
std::atomic<size_t> JITConfig::maxLoopTraces(DEFAULT_MAX_LOOP_TRACES);
std::atomic<bool> JITConfig::enableInlineCaching(DEFAULT_ENABLE_INLINE_CACHING);
std::atomic<bool> JITConfig::enableLoopUnrolling(DEFAULT_ENABLE_LOOP_UNROLLING);
std::atomic<bool> JITConfig::enableMethodInlining(DEFAULT_ENABLE_METHOD_INLINING);
std::atomic<bool> JITConfig::enableShallowTracing(DEFAULT_ENABLE_SHALLOW_TRACING);
std::atomic<bool> JITConfig::enableTypeSpecialization(DEFAULT_ENABLE_TYPE_SPECIALIZATION);
std::atomic<bool> JITConfig::enablePerfMonitoring(DEFAULT_ENABLE_PERF_MONITORING);

} // namespace neutron::jit
