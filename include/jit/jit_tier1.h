#ifndef NEUTRON_JIT_TIER1_COMPILER_H
#define NEUTRON_JIT_TIER1_COMPILER_H

#include "jit_config.h"
#include "jit_profiler.h"
#include "compiler/bytecode.h"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>
#include <functional>

namespace neutron::jit {

/**
 * Tier-1 JIT Compiler: Lightweight Threaded Code Generation
 * 
 * Generates subroutine-threaded code for quick compilation with moderate optimization.
 * This is the first-tier compilation in the multi-tier JIT strategy.
 * 
 * Key features:
 * - Fast compilation with low overhead
 * - Generates threaded code (direct calls to handlers)
 * - Profiles execution for transition to Tier-2
 * - Uses shallow tracing to avoid side effects
 * - Implements inline caching for method dispatch
 * 
 * Reference: "A Lightweight Method for Generating Multi-Tier JIT Compilation..."
 */
class Tier1Compiler {
public:
    /**
     * Native code fragment for compiled bytecode instruction
     */
    struct ThreadedInstruction {
        uint8_t opcode;
        uint64_t handler_address;  ///< Address of handler function
        uint32_t inline_cache_id;  ///< Inline cache entry for method dispatch
        std::vector<uint8_t> embedded_data;  ///< Embedded immediates/constants
    };

    /**
     * Compiled threaded code
     */
    struct ThreadedCode {
        uint64_t method_id;
        uint64_t code_address;
        size_t code_size;
        std::vector<ThreadedInstruction> instructions;
        int64_t compilation_time_us;
        uint64_t profile_counter;  ///< For tracking execution
    };

    /**
     * Handler function type for bytecode operations
     */
    using HandlerFunction = std::function<void(void*, const uint8_t*)>;

    Tier1Compiler();
    ~Tier1Compiler();

    /**
     * Compile a method using threaded code generation
     * @param method_id Unique method identifier
     * @param bytecode The bytecode chunk to compile
     * @param handlers Map of opcodes to handler functions
     * @return Compiled threaded code, or nullptr if compilation failed
     */
    std::unique_ptr<ThreadedCode> compile(
        uint64_t method_id,
        const Chunk& bytecode,
        const std::unordered_map<uint8_t, HandlerFunction>& handlers);

    /**
     * Execute compiled threaded code
     * @param code The compiled code to execute
     * @param context Execution context (stack, frame, etc.)
     * @return true if execution succeeded, false on error
     */
    bool execute(const ThreadedCode& code, void* context);

    /**
     * Optimize shallow tracing: prevent side effects during method-level tracing
     * @param bytecode The bytecode being compiled
     * @return Modified bytecode suitable for shallow tracing
     */
    Chunk optimizeShallowTracing(const Chunk& bytecode);

    /**
     * Add inline caching entry for method dispatch
     * @param call_offset Bytecode offset of the call
     * @param method_id ID of the method being called
     * @param handler_address Address of compiled method
     * @return Inline cache entry ID
     */
    uint32_t addInlineCacheEntry(uint64_t call_offset, 
                                  uint64_t method_id, 
                                  uint64_t handler_address);

    /**
     * Validate inline cache entry
     * @param cache_id The cache entry ID
     * @param method_id Expected method ID
     * @return true if cache is valid
     */
    bool validateInlineCache(uint32_t cache_id, uint64_t method_id);

    /**
     * Check if code cache is full
     */
    bool isCodeCacheFull() const;

    /**
     * Get code cache statistics
     */
    struct CacheStats {
        size_t total_code_size;
        size_t max_code_size;
        size_t num_compiled_methods;
        size_t num_inline_caches;
        float cache_hit_ratio;
    };

    CacheStats getCacheStats() const;

    /**
     * Clear code cache (for recompilation or memory management)
     */
    void clearCodeCache();

private:
    /// Code cache for compiled threaded code
    std::vector<uint8_t> code_cache_;
    size_t code_cache_offset_;

    /// Map from method_id to compiled threaded code
    std::unordered_map<uint64_t, std::unique_ptr<ThreadedCode>> compiled_methods_;

    /// Inline cache entries
    struct InlineCacheEntry {
        uint64_t call_offset;
        uint64_t expected_method_id;
        uint64_t handler_address;
        int64_t hit_count;
        int64_t miss_count;
    };
    std::unordered_map<uint32_t, InlineCacheEntry> inline_caches_;
    uint32_t next_cache_id_;

    /// Statistics
    size_t total_compilations_;
    int64_t total_compilation_time_us_;

    /**
     * Generate machine code for a bytecode instruction
     * @param instr Bytecode instruction
     * @param handler Handler function for this opcode
     * @return Machine code to emit
     */
    std::vector<uint8_t> generateThreadedInstruction(
        uint8_t opcode, const HandlerFunction& handler);

    /**
     * Emit direct call to handler
     */
    std::vector<uint8_t> emitHandlerCall(uint64_t handler_address);

    /**
     * Allocate space in code cache
     */
    uint8_t* allocateCodeSpace(size_t size);
};

} // namespace neutron::jit

#endif // NEUTRON_JIT_TIER1_COMPILER_H
