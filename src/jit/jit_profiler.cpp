#include "jit/jit_profiler.h"
#include <algorithm>
#include <cmath>
#include <mutex>

namespace neutron::jit {

HotSpotProfiler::HotSpotProfiler() 
    : start_time_(std::chrono::high_resolution_clock::now()),
      tier1_compilations_(0),
      tier2_compilations_(0),
      total_execution_time_us_(0) {
}

HotSpotProfiler::~HotSpotProfiler() = default;

bool HotSpotProfiler::recordExecution(uint64_t method_id, uint64_t bytecode_offset) {
    auto& profile = getOrCreateProfile(method_id, bytecode_offset);
    profile.execution_count++;

    // Check if should compile to next tier
    if (profile.compiled_tier == CompilationTier::INTERPRETER) {
        if (profile.execution_count >= TIER1_COMPILATION_THRESHOLD) {
            profile.compiled_tier = CompilationTier::TIER1;
            profile.last_compiled_at = std::chrono::high_resolution_clock::now()
                .time_since_epoch().count();
            tier1_compilations_++;
            return true;
        }
    } else if (profile.compiled_tier == CompilationTier::TIER1) {
        if (profile.execution_count >= TIER2_COMPILATION_THRESHOLD) {
            profile.compiled_tier = CompilationTier::TIER2;
            profile.last_compiled_at = std::chrono::high_resolution_clock::now()
                .time_since_epoch().count();
            tier2_compilations_++;
            return true;
        }
    }

    return false;
}

void HotSpotProfiler::recordMethodCall(uint64_t method_id) {
    auto& method = getOrCreateMethodProfile(method_id);
    method.call_count++;
}

void HotSpotProfiler::recordMethodExecutionTime(uint64_t method_id, 
                                                int64_t duration_us) {
    auto& method = getOrCreateMethodProfile(method_id);
    method.total_execution_time_us += duration_us;
    total_execution_time_us_ += duration_us;
}

bool HotSpotProfiler::isHot(uint64_t method_id, uint64_t bytecode_offset,
                             CompilationTier current_tier) const {
    uint64_t key = hashKey(method_id, bytecode_offset);
    auto it = execution_profiles_.find(key);
    
    if (it == execution_profiles_.end()) {
        return false;
    }

    const auto& profile = it->second;
    
    if (current_tier == CompilationTier::INTERPRETER) {
        return profile.execution_count >= TIER1_COMPILATION_THRESHOLD;
    } else if (current_tier == CompilationTier::TIER1) {
        return profile.execution_count >= TIER2_COMPILATION_THRESHOLD;
    }

    return false;
}

const HotSpotProfiler::ExecutionProfile* 
HotSpotProfiler::getProfile(uint64_t method_id, uint64_t bytecode_offset) const {
    uint64_t key = hashKey(method_id, bytecode_offset);
    auto it = execution_profiles_.find(key);
    
    if (it == execution_profiles_.end()) {
        return nullptr;
    }

    return &it->second;
}

const HotSpotProfiler::MethodProfile* 
HotSpotProfiler::getMethodProfile(uint64_t method_id) const {
    auto it = method_profiles_.find(method_id);
    
    if (it == method_profiles_.end()) {
        return nullptr;
    }

    return it->second.get();
}

void HotSpotProfiler::resetMethodProfile(uint64_t method_id) {
    auto it = method_profiles_.find(method_id);
    
    if (it != method_profiles_.end()) {
        it->second->call_count = 0;
        it->second->total_execution_time_us = 0;
        it->second->loop_offsets.clear();
    }
}

int64_t HotSpotProfiler::getTotalExecutionTime() const {
    return total_execution_time_us_.load();
}

HotSpotProfiler::WarmupMetrics HotSpotProfiler::getWarmupMetrics() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - start_time_);

    WarmupMetrics metrics;
    metrics.warmup_duration_ms = duration.count();
    metrics.tier1_compilations = tier1_compilations_.load();
    metrics.tier2_compilations = tier2_compilations_.load();
    
    if (duration.count() > 0) {
        metrics.peak_throughput = total_execution_time_us_.load() / 
                                 (duration.count() * 1000);  // ops per ms
    } else {
        metrics.peak_throughput = 0;
    }

    return metrics;
}

HotSpotProfiler::ExecutionProfile& 
HotSpotProfiler::getOrCreateProfile(uint64_t method_id, uint64_t bytecode_offset) {
    uint64_t key = hashKey(method_id, bytecode_offset);
    
    auto it = execution_profiles_.find(key);
    if (it != execution_profiles_.end()) {
        return it->second;
    }

    ExecutionProfile profile;
    profile.bytecode_offset = bytecode_offset;
    execution_profiles_[key] = profile;
    
    return execution_profiles_[key];
}

HotSpotProfiler::MethodProfile& 
HotSpotProfiler::getOrCreateMethodProfile(uint64_t method_id) {
    auto it = method_profiles_.find(method_id);
    if (it != method_profiles_.end()) {
        return *it->second;
    }

    auto profile = std::make_unique<MethodProfile>(method_id);
    auto& ref = *profile;
    method_profiles_[method_id] = std::move(profile);
    
    return ref;
}

uint64_t HotSpotProfiler::hashKey(uint64_t method_id, uint64_t bytecode_offset) {
    // Combine method_id and bytecode_offset into a single hash key
    // Use a simple but effective hash function
    return (method_id << 32) | (bytecode_offset & 0xFFFFFFFF);
}

} // namespace neutron::jit
