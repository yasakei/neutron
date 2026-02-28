#include "jit/jit_tier1.h"
#include "jit/jit_codegen.h"
#include "types/value.h"
#include <chrono>
#include <cstring>
#include <algorithm>

namespace neutron::jit {

Tier1Compiler::Tier1Compiler() 
    : next_cache_id_(1), 
      total_compilations_(0), total_compilation_time_us_(0) {
    // Lazy init: don't allocate code cache until needed
}

Tier1Compiler::~Tier1Compiler() = default;

std::unique_ptr<Tier1Compiler::ThreadedCode> Tier1Compiler::compile(
    uint64_t method_id,
    const Chunk& bytecode,
    const std::unordered_map<uint8_t, HandlerFunction>& handlers) {

    auto start_time = std::chrono::high_resolution_clock::now();

    if (bytecode.code.empty()) {
        return nullptr;
    }

    auto code = std::make_unique<ThreadedCode>();
    code->method_id = method_id;
    code->profile_counter = 0;

    X86_64CodeGen codegen;
    std::vector<uint8_t> native_code;

    // Generate x86-64 threaded code for each bytecode instruction
    // Pattern: MOV RAX, handler_address; CALL RAX
    for (size_t i = 0; i < bytecode.code.size(); ++i) {
        uint8_t opcode = bytecode.code[i];

        auto it = handlers.find(opcode);
        if (it == handlers.end()) {
            continue;  // Skip unknown opcodes
        }

        ThreadedInstruction instr;
        instr.opcode = opcode;
        
        // Get handler function address
        const void* handler_ptr = it->second.target<void*>();
        uint64_t handler_addr = handler_ptr ? reinterpret_cast<uint64_t>(const_cast<void*>(handler_ptr)) : 0;
        
        if (handler_addr == 0) {
            continue;  // Skip if handler is null
        }

        instr.handler_address = handler_addr;

        // Save offset for inline caching
        uint32_t instr_offset = native_code.size();

        // Generate native code: MOV RAX, handler_address; CALL RAX
        codegen.emitMovReg64Imm64(native_code, 0, handler_addr);  // RAX = handler_addr
        codegen.emitCallReg(native_code, 0);  // CALL RAX

        // Add inline cache slot for call instructions
        if (opcode == static_cast<uint8_t>(OpCode::OP_CALL)) {
            instr.inline_cache_id = addInlineCacheEntry(instr_offset, method_id, handler_addr);
        } else {
            instr.inline_cache_id = 0;
        }

        // For backward jumps (loops), add profiling entry
        if (opcode == static_cast<uint8_t>(OpCode::OP_LOOP)) {
            instr.inline_cache_id = next_cache_id_++;
        }

        code->instructions.push_back(instr);
    }

    // Add function epilogue: RET
    codegen.emitRet(native_code);

    code->code_size = native_code.size();
    
    // Check code cache space
    if (code->code_size > TIER1_CODE_CACHE_SIZE - code_cache_.offset()) {
        return nullptr;  // Code cache full
    }

    uint8_t* code_space = allocateCodeSpace(code->code_size);
    if (!code_space) {
        return nullptr;
    }

    // Copy native code to code cache
    code_cache_.makeWritable();
    std::memcpy(code_space, native_code.data(), code->code_size);
    code->code_address = reinterpret_cast<uint64_t>(code_space);

    // Make code executable
    if (!code_cache_.makeExecutable()) {
        return nullptr;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    code->compilation_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();

    total_compilation_time_us_ += code->compilation_time_us;
    total_compilations_++;

    compiled_methods_[method_id] = std::make_unique<ThreadedCode>(*code);

    return code;
}

bool Tier1Compiler::execute(const ThreadedCode& code, void* context) {
    // Cast to function pointer and execute the threaded code
    // The code_address points to native x86-64 code
    typedef void(*ThreadedFunc)(void*);
    ThreadedFunc func = reinterpret_cast<ThreadedFunc>(code.code_address);
    
    try {
        func(context);
        return true;
    } catch (...) {
        return false;
    }
    
    if (code.instructions.empty() || context == nullptr) {
        return false;
    }

    // Simulate execution of threaded code
    // In a real implementation, this would jump to code.code_address
    // and let the CPU execute the compiled code
    
    // For now, iterate through instructions and count them
    for (const auto& instr : code.instructions) {
        if (instr.handler_address != 0) {
            // In a real implementation, call the handler via function pointer
            // void (*handler)(void*) = reinterpret_cast<void(*)(void*)>(instr.handler_address);
            // handler(context);
        }
    }

    return true;
}

Chunk Tier1Compiler::optimizeShallowTracing(const Chunk& bytecode) {
    Chunk optimized = bytecode;
    
    // Shallow tracing optimization:
    // Prevent side effects when tracing both branches of conditionals
    // by inserting dummy flag checks in handler functions
    
    // Strategy:
    // 1. For each handler that might have side effects (store, call, throw)
    // 2. Wrap execution in a we_are_jitted() check
    // 3. If jitted (tracing), return dummy value without side effects
    // 4. If not jitted, execute normally
    
    // Mark instructions that have side effects
    std::vector<uint8_t> has_side_effects;
    for (uint8_t opcode : optimized.code) {
        bool is_sideeffect = false;
        switch (opcode) {
            // Store operations
            case static_cast<uint8_t>(OpCode::OP_SET_LOCAL):
            case static_cast<uint8_t>(OpCode::OP_SET_GLOBAL):
            case static_cast<uint8_t>(OpCode::OP_SET_PROPERTY):
            case static_cast<uint8_t>(OpCode::OP_INDEX_SET):
            // Call operations
            case static_cast<uint8_t>(OpCode::OP_CALL):
            // Exception handling
            case static_cast<uint8_t>(OpCode::OP_THROW):
                is_sideeffect = true;
                break;
            default:
                break;
        }
        has_side_effects.push_back(is_sideeffect ? 1 : 0);
    }
    
    // For instructions with side effects, the interpreter should:
    // 1. Check we_are_jitted() before execution
    // 2. Skip execution if in jitted mode (return dummy value)
    // 3. This prevents corrupting interpreter state during trace recording
    
    return optimized;
}

uint32_t Tier1Compiler::addInlineCacheEntry(uint64_t call_offset,
                                             uint64_t method_id,
                                             uint64_t handler_address) {
    InlineCacheEntry entry;
    entry.call_offset = call_offset;
    entry.expected_method_id = method_id;
    entry.handler_address = handler_address;
    entry.hit_count = 0;
    entry.miss_count = 0;

    uint32_t cache_id = next_cache_id_++;
    inline_caches_[cache_id] = entry;

    return cache_id;
}

bool Tier1Compiler::validateInlineCache(uint32_t cache_id, uint64_t method_id) {
    auto it = inline_caches_.find(cache_id);
    if (it == inline_caches_.end()) {
        return false;
    }

    auto& entry = it->second;
    
    // Fast path: type matches
    if (entry.expected_method_id == method_id) {
        entry.hit_count++;
        return true;
    }
    
    // Slow path: type mismatch, requires dynamic lookup
    entry.miss_count++;
    
    // In a real system, on miss we'd:
    // 1. Perform dynamic method lookup
    // 2. If same method called again, update cache (megamorphic)
    // 3. If different method, mark as megamorphic (don't cache)
    
    return false;
}

bool Tier1Compiler::isCodeCacheFull() const {
    return code_cache_.offset() >= TIER1_CODE_CACHE_SIZE * 0.9;  // 90% full
}

Tier1Compiler::CacheStats Tier1Compiler::getCacheStats() const {
    CacheStats stats;
    stats.total_code_size = code_cache_.offset();
    stats.max_code_size = TIER1_CODE_CACHE_SIZE;
    stats.num_compiled_methods = compiled_methods_.size();
    stats.num_inline_caches = inline_caches_.size();

    // Calculate cache hit ratio
    int64_t total_hits = 0, total_accesses = 0;
    for (const auto& [cache_id, entry] : inline_caches_) {
        total_hits += entry.hit_count;
        total_accesses += entry.hit_count + entry.miss_count;
    }

    stats.cache_hit_ratio = total_accesses > 0 ? 
        static_cast<float>(total_hits) / total_accesses : 0.0f;

    return stats;
}

void Tier1Compiler::clearCodeCache() {
    // Clear all caches and reset state
    code_cache_.reset();
    
    compiled_methods_.clear();
    inline_caches_.clear();
    next_cache_id_ = 1;
    
    // Reset statistics
    total_compilations_ = 0;
    total_compilation_time_us_ = 0;
}

std::vector<uint8_t> Tier1Compiler::generateThreadedInstruction(
    uint8_t opcode, const HandlerFunction& handler) {
    
    std::vector<uint8_t> code;
    
    // Threaded code simply stores the handler address
    // Platform-specific code generation would emit:
    // - x86-64: call qword [handler_addr]
    // - ARM64: bl handler_addr
    // - etc.
    
    // For simulation, store handler address
    code.push_back(opcode);
    
    return code;
}

std::vector<uint8_t> Tier1Compiler::emitHandlerCall(uint64_t handler_address) {
    std::vector<uint8_t> code;
    
    // Emit platform-specific call instruction
    // This is architecture-dependent
    
    // x86-64 example: CALL rel32 (0xE8 + 4-byte offset)
    if (handler_address != 0) {
        code.push_back(0xE8);  // CALL rel32 opcode
        
        // 4-byte relative offset (placeholder)
        uint32_t offset = static_cast<uint32_t>(handler_address & 0xFFFFFFFF);
        code.push_back(offset & 0xFF);
        code.push_back((offset >> 8) & 0xFF);
        code.push_back((offset >> 16) & 0xFF);
        code.push_back((offset >> 24) & 0xFF);
    }
    
    return code;
}

uint8_t* Tier1Compiler::allocateCodeSpace(size_t size) {
    // Lazy-init code cache on first allocation
    if (!code_cache_.isInitialized()) {
        if (!code_cache_.initialize(TIER1_CODE_CACHE_SIZE)) {
            return nullptr;
        }
    }
    return code_cache_.allocate(size);
}

} // namespace neutron::jit
