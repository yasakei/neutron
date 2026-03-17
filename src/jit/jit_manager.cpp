#include "jit/jit_manager.h"
#include <chrono>
#include <thread>
#include <cstdio>

namespace neutron::jit {

MultiTierJITManager::MultiTierJITManager()
    : profiler_(std::make_unique<HotSpotProfiler>()),
      tier1_compiler_(std::make_unique<Tier1Compiler>()),
      tier2_compiler_(std::make_unique<Tier2Compiler>()),
      monitoring_enabled_(false) {

    start_time_ = std::chrono::high_resolution_clock::now();
}

MultiTierJITManager::~MultiTierJITManager() = default;

bool MultiTierJITManager::initialize(bool enable_monitoring) {
    monitoring_enabled_ = enable_monitoring;
    return true;  // Initialization successful
}

bool MultiTierJITManager::requestCompilation(uint64_t method_id, 
                                             const Chunk& bytecode,
                                             CompilationTier current_tier) {

    auto start_time = std::chrono::high_resolution_clock::now();
    
    CompilationTier target_tier = decideCompilationTier(method_id, current_tier);

    if (target_tier == current_tier) {
        return false;  // No compilation needed
    }

    CompilationEvent event;
    event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start_time_).count();
    event.method_id = method_id;
    event.target_tier = target_tier;

    bool success = false;

    if (target_tier == CompilationTier::TIER1) {
        event.type = CompilationEvent::Type::TIER1_COMPILATION_TRIGGERED;
        emitEvent(event);

        // Compile using Tier-1 (lightweight threaded code generation)
        std::unordered_map<uint8_t, Tier1Compiler::HandlerFunction> handlers;
        // In a real implementation, populate handlers from the VM's bytecode handlers
        
        auto tier1_code = tier1_compiler_->compile(method_id, bytecode, handlers);

        if (tier1_code) {
            tier1_compilations_++;
            
            // Cache the compiled Tier-1 code for execution
            tier1_code_cache_[method_id] = std::move(tier1_code);

            event.type = CompilationEvent::Type::TIER1_COMPILATION_COMPLETED;
            auto end_time = std::chrono::high_resolution_clock::now();
            event.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - start_time).count();
            emitEvent(event);

            total_compilation_time_us_ += event.duration_us;
            success = true;
        } else {
            event.type = CompilationEvent::Type::COMPILATION_FAILED;
            emitEvent(event);
        }

    } else if (target_tier == CompilationTier::TIER2) {
        event.type = CompilationEvent::Type::TIER2_COMPILATION_TRIGGERED;
        emitEvent(event);

        // Compile using Tier-2 (tracing JIT with optimizations)
        auto trace = tier2_compiler_->recordTrace(method_id, bytecode, 0, *profiler_);
        
        if (trace) {
            auto optimized = tier2_compiler_->optimizeTrace(*trace);
            uint64_t code_addr = tier2_compiler_->compileTrace(*optimized);
            
            if (code_addr != 0) {
                tier2_compilations_++;
                
                event.type = CompilationEvent::Type::TIER2_COMPILATION_COMPLETED;
                auto end_time = std::chrono::high_resolution_clock::now();
                event.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    end_time - start_time).count();
                emitEvent(event);
                
                total_compilation_time_us_ += event.duration_us;
                success = true;
            } else {
                event.type = CompilationEvent::Type::COMPILATION_FAILED;
                emitEvent(event);
            }
        } else {
            event.type = CompilationEvent::Type::COMPILATION_FAILED;
            emitEvent(event);
        }
    }

    return success;
}

CompilationTier MultiTierJITManager::recordExecution(uint64_t method_id, 
                                                     uint64_t bytecode_pc) {
    profiler_->recordExecution(method_id, bytecode_pc);
    
    // Always return the current compiled tier of this hot spot
    const auto* profile = profiler_->getProfile(method_id, bytecode_pc);
    if (profile) {
        return profile->compiled_tier;
    }

    return CompilationTier::INTERPRETER;
}

bool MultiTierJITManager::transitionTier(CompilationTier from_tier,
                                         CompilationTier to_tier,
                                         uint64_t method_id,
                                         ExecutionFrame& frame) {

    auto start_time = std::chrono::high_resolution_clock::now();

    CompilationEvent event;
    event.type = CompilationEvent::Type::TIER_TRANSITION;
    event.method_id = method_id;
    event.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start_time_).count();
    event.target_tier = to_tier;

    // Update frame's execution tier
    frame.current_tier = to_tier;
    frame.tier_enter_time = std::chrono::high_resolution_clock::now();

    auto end_time = std::chrono::high_resolution_clock::now();
    event.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();

    emitEvent(event);
    recordTierTransition(from_tier, to_tier, event.duration_us);

    return true;
}

bool MultiTierJITManager::executeCompiledCode(uint64_t method_id,
                                              CompilationTier tier,
                                              ExecutionFrame& frame) {

    frame.current_tier = tier;
    frame.tier_enter_time = std::chrono::high_resolution_clock::now();

    bool success = false;

    switch (tier) {
        case CompilationTier::INTERPRETER:
            // Execute in interpreter (handled by VM)
            success = true;
            break;

        case CompilationTier::TIER1: {
            // Execute Tier-1 compiled code
            // Check if we have compiled code cached
            auto it = tier1_code_cache_.find(method_id);
            if (it == tier1_code_cache_.end() && frame.chunk) {
                // Compile on demand using Tier-1
                std::unordered_map<uint8_t, Tier1Compiler::HandlerFunction> handlers;
                // Note: Handler functions would need to be registered from the VM
                // For now, we'll compile but execution will be limited
                auto tier1_result = tier1_compiler_->compile(method_id, *frame.chunk, handlers);
                if (tier1_result.ok()) {
                    tier1_code_cache_[method_id] = std::move(tier1_result.getValue());
                    it = tier1_code_cache_.find(method_id);
                }
            }

            // Execute compiled Tier-1 code if available
            if (it != tier1_code_cache_.end() && it->second) {
                auto exec_result = tier1_compiler_->execute(*it->second, &frame);
                success = exec_result.ok();
            } else {
                // Fallback to interpreter if compilation failed
                success = true;
            }
            break;
        }

        case CompilationTier::TIER2: {
            // Execute Tier-2 compiled code
            // Look for compiled trace for this method and PC
            auto trace_result = tier2_compiler_->findTrace(method_id, frame.bytecode_pc);
            uint64_t trace_id = 0;
            if (trace_result.ok()) {
                trace_id = trace_result.getValue();
            }
            if (trace_id == 0 && frame.chunk) {
                // Compilation on demand!
                // Compile on demand
                auto trace = tier2_compiler_->recordTrace(method_id, *frame.chunk, frame.bytecode_pc, *profiler_);
                if (trace) {
                    auto optimized = tier2_compiler_->optimizeTrace(*trace);
                    if (optimized) {
                        auto compile_result = tier2_compiler_->compileTrace(*optimized);
                        if (compile_result.ok()) {
                            trace_id = compile_result.getValue();
                        }
                    }
                }
            }

            if (trace_id != 0) {
                 // Pass execution frame as context
                 // The JIT code expects ExecutionFrame* in RDI (void* context)
                 auto exec_result = tier2_compiler_->executeTrace(trace_id, &frame);
                 success = exec_result.ok();
            }
            break;
        }
    }

    // Record execution time for this tier
    auto end_time = std::chrono::high_resolution_clock::now();
    int64_t duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - frame.tier_enter_time).count();

    switch (tier) {
        case CompilationTier::INTERPRETER:
            interpreter_time_us_ += duration_us;
            break;
        case CompilationTier::TIER1:
            tier1_time_us_ += duration_us;
            break;
        case CompilationTier::TIER2:
            tier2_time_us_ += duration_us;
            break;
    }

    return success;
}

const HotSpotProfiler::MethodProfile* 
MultiTierJITManager::getMethodProfile(uint64_t method_id) const {
    return profiler_->getMethodProfile(method_id);
}

neutron::jit::JITStatistics MultiTierJITManager::getStatistics() const {
    JITStatistics stats;

    stats.total_tier1_compilations = tier1_compilations_.load();
    stats.total_tier2_compilations = tier2_compilations_.load();
    stats.failed_compilations = failed_compilations_.load();

    stats.total_compilation_time_us = total_compilation_time_us_.load();
    stats.total_execution_time_us = profiler_->getTotalExecutionTime();

    auto now = std::chrono::high_resolution_clock::now();
    stats.warmup_duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        now - start_time_).count();

    auto tier1_stats = tier1_compiler_->getCacheStats();
    stats.tier1_code_cache_size = tier1_stats.total_code_size;
    stats.cache_hit_ratio = tier1_stats.cache_hit_ratio;

    auto tier2_stats = tier2_compiler_->getTraceStats();
    stats.tier2_code_cache_size = tier2_stats.total_compiled_code_size;

    stats.time_in_interpreter = interpreter_time_us_.load();
    stats.time_in_tier1 = tier1_time_us_.load();
    stats.time_in_tier2 = tier2_time_us_.load();

    // Optimization counts from Tier2Compiler
    stats.loop_unrollings = tier2_compiler_->getLoopUnrollings();
    stats.method_inlinings = tier2_compiler_->getMethodInlinings();
    stats.type_specializations = tier2_compiler_->getTypeSpecializations();

    return stats;
}

bool MultiTierJITManager::isInWarmupPhase() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - start_time_).count();

    // Warmup phase is the first 5 seconds or until tier-2 compilations start
    return elapsed_ms < 5000 && tier2_compilations_.load() == 0;
}

JITResult MultiTierJITManager::recompile(uint64_t method_id, CompilationTier force_tier) {
    // Force recompilation at a specific tier
    // This is useful for optimization or debugging

    auto start_time = std::chrono::high_resolution_clock::now();

    // Reset profiling data to trigger fresh compilation
    profiler_->resetMethodProfile(method_id);

    // Clear any cached compiled code for this method
    if (force_tier == CompilationTier::TIER1 || force_tier == CompilationTier::INTERPRETER) {
        tier1_code_cache_.erase(method_id);
    }
    if (force_tier == CompilationTier::TIER2) {
        tier2_compiler_->clearTraceCache();
    }

    // Note: Actual recompilation would require access to the original bytecode chunk
    // which is typically stored elsewhere. This implementation resets the state
    // so that the next execution will trigger fresh compilation.

    auto end_time = std::chrono::high_resolution_clock::now();
    int64_t duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();

    total_compilation_time_us_ += duration_us;

    return JITResult::OK();
}

void MultiTierJITManager::reset() {
    tier1_compiler_->clearCodeCache();
    tier2_compiler_->clearTraceCache();
    profiler_ = std::make_unique<HotSpotProfiler>();
    
    // Clear Tier-1 code cache
    tier1_code_cache_.clear();

    tier1_compilations_ = 0;
    tier2_compilations_ = 0;
    failed_compilations_ = 0;
    total_compilation_time_us_ = 0;

    interpreter_time_us_ = 0;
    tier1_time_us_ = 0;
    tier2_time_us_ = 0;

    start_time_ = std::chrono::high_resolution_clock::now();
}

void MultiTierJITManager::registerEventListener(EventCallback callback) {
    event_listeners_.push_back(callback);
}

void MultiTierJITManager::emitEvent(const CompilationEvent& event) {
    if (!monitoring_enabled_) {
        return;
    }

    for (auto& callback : event_listeners_) {
        callback(event);
    }
}

CompilationTier MultiTierJITManager::decideCompilationTier(
    uint64_t method_id, CompilationTier current_tier) {

    // Use profiler data to decide next compilation tier
    auto profile = profiler_->getMethodProfile(method_id);
    
    if (!profile) {
        return current_tier;
    }

    switch (current_tier) {
        case CompilationTier::INTERPRETER:
            // Compile to Tier-1 if method is called frequently
            if (profile->call_count >= TIER1_COMPILATION_THRESHOLD) {
                return CompilationTier::TIER1;
            }
            break;
            
        case CompilationTier::TIER1: {
            // Compile to Tier-2 if execution time in Tier-1 is significant
            // and method is still hot
            if (profile->call_count >= TIER2_COMPILATION_THRESHOLD &&
                profile->total_execution_time_us > 10000) {  // >10ms in Tier-1
                return CompilationTier::TIER2;
            }
            break;
        }
            
        case CompilationTier::TIER2:
            // Already at peak optimization
            break;
    }

    return current_tier;
}

void MultiTierJITManager::recordTierTransition(CompilationTier from_tier,
                                               CompilationTier to_tier,
                                               int64_t duration_us) {
    // Record tier transition for performance analysis
    // This is useful for understanding warmup characteristics
}

} // namespace neutron::jit
