#ifndef NEUTRON_JIT_TIER1_COMPILER_H
#define NEUTRON_JIT_TIER1_COMPILER_H

/*
 * Code Documentation: Tier-1 JIT Compiler (jit_tier1.h)
 * =====================================================
 * 
 * This header defines the Tier1Compiler - the first tier of Neutron's
 * JIT compilation strategy. It generates lightweight threaded code
 * for fast compilation with moderate optimization.
 * 
 * What This File Includes:
 * ------------------------
 * - Tier1Compiler class: Threaded code generator
 * - ThreadedInstruction: Native code fragment representation
 * - ThreadedCode: Compiled method container
 * - Inline caching: Method dispatch optimization
 * 
 * How It Works:
 * -------------
 * Tier-1 compilation converts bytecode to subroutine-threaded code:
 * 1. Each bytecode instruction maps to a handler function
 * 2. Compiled code is a sequence of direct handler calls
 * 3. Inline caches optimize repeated method dispatch
 * 4. Execution profiling continues for Tier-2 promotion
 * 
 * Threaded Code Example:
 * @code
 * Bytecode: LOAD_LOCAL 0, LOAD_LOCAL 1, ADD, RETURN
 * Threaded Code:
 *   call handler_LOAD_LOCAL(0)
 *   call handler_LOAD_LOCAL(1)
 *   call handler_ADD
 *   call handler_RETURN
 * @endcode
 * 
 * Adding Features:
 * ----------------
 * - New handlers: Add to handler map, implement in VM
 * - Inline cache improvements: Extend cache entry structure
 * - Optimization passes: Add pre-compilation bytecode transforms
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT modify handlers during execution
 * - Do NOT exceed code cache limits (check isCodeCacheFull())
 * - Do NOT skip shallow tracing optimization
 * - Do NOT use Tier-1 for long-running hot loops (use Tier-2)
 * 
 * Performance Characteristics:
 * ----------------------------
 * - Compilation speed: Fast (direct bytecode-to-handler mapping)
 * - Execution speed: Moderate (function call overhead per instruction)
 * - Memory usage: Low (minimal code duplication)
 * - Best for: Warm code, short methods, quick startup
 */

#include "jit_config.h"
#include "jit_profiler.h"
#include "jit_memory.h"
#include "compiler/bytecode.h"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>
#include <functional>

namespace neutron::jit {

/**
 * @brief Cache statistics container for Tier1Compiler.
 */
struct CacheStats {
    size_t total_code_size;      ///< Total bytes of compiled code
    size_t max_code_size;        ///< Maximum cache capacity
    size_t num_compiled_methods; ///< Number of compiled methods
    size_t num_inline_caches;    ///< Number of inline cache entries
    float cache_hit_ratio;       ///< Inline cache hit rate (0.0-1.0)
};

/**
 * @brief Tier1Compiler - Lightweight Threaded Code Generator.
 * 
 * Generates subroutine-threaded code for quick compilation with moderate optimization.
 * This is the first-tier compilation in the multi-tier JIT strategy.
 * 
 * Key features:
 * - Fast compilation with low overhead (milliseconds, not seconds)
 * - Generates threaded code (direct calls to handlers)
 * - Profiles execution for transition to Tier-2
 * - Uses shallow tracing to avoid side effects
 * - Implements inline caching for method dispatch
 * 
 * Think of Tier-1 as the "quick and dirty" compilation tier - it's not
 * the most optimized, but it's ready fast and better than interpreting.
 * 
 * Reference: "A Lightweight Method for Generating Multi-Tier JIT Compilation..."
 */
class Tier1Compiler {
public:
    /**
     * @brief ThreadedInstruction - Native code fragment for a bytecode instruction.
     * 
     * Represents a single compiled instruction:
     * - opcode: Original bytecode opcode
     * - handler_address: Function pointer to the handler
     * - inline_cache_id: Cache entry for method dispatch (if applicable)
     * - embedded_data: Immediate values or constants
     */
    struct ThreadedInstruction {
        uint8_t opcode;                    ///< Original bytecode opcode
        uint64_t handler_address;          ///< Address of handler function
        uint32_t inline_cache_id;          ///< Inline cache entry for method dispatch
        std::vector<uint8_t> embedded_data;  ///< Embedded immediates/constants
    };

    /**
     * @brief ThreadedCode - Compiled threaded code container.
     * 
     * Holds all information for executing a compiled method:
     * - method_id: Unique identifier
     * - code_address: Entry point in executable memory
     * - instructions: Vector of threaded instructions
     * - compilation_time: Time spent compiling (for profiling)
     * - profile_counter: Execution count for Tier-2 promotion
     */
    struct ThreadedCode {
        uint64_t method_id;                      ///< Unique method identifier
        uint64_t code_address;                   ///< Entry point address
        size_t code_size;                        ///< Size of compiled code
        std::vector<ThreadedInstruction> instructions;  ///< Instruction sequence
        int64_t compilation_time_us;             ///< Compilation time (microseconds)
        uint64_t profile_counter;                ///< Execution count for Tier-2 promotion
    };

    /**
     * @brief HandlerFunction - Type for bytecode handler functions.
     * 
     * Each handler receives:
     * - context: Execution context (stack, locals, etc.)
     * - operands: Pointer to instruction operands
     * 
     * Handlers are responsible for:
     * - Executing the instruction semantics
     * - Updating the program counter
     * - Handling any side effects
     */
    using HandlerFunction = std::function<void(void*, const uint8_t*)>;

    Tier1Compiler();
    ~Tier1Compiler();

    /**
     * @brief Compile a method using threaded code generation.
     * @param method_id Unique method identifier.
     * @param bytecode The bytecode chunk to compile.
     * @param handlers Map of opcodes to handler functions.
     * @return Compiled threaded code, or nullptr if compilation failed.
     * 
     * Compilation process:
     * 1. Iterate through bytecode instructions
     * 2. Look up handler for each opcode
     * 3. Generate threaded instruction with handler address
     * 4. Allocate executable memory and copy code
     * 5. Return ThreadedCode structure
     */
    std::unique_ptr<ThreadedCode> compile(
        uint64_t method_id,
        const Chunk& bytecode,
        const std::unordered_map<uint8_t, HandlerFunction>& handlers);

    /**
     * @brief Execute compiled threaded code.
     * @param code The compiled code to execute.
     * @param context Execution context (stack, frame, etc.).
     * @return true if execution succeeded, false on error.
     * 
     * Execution jumps to code_address and runs handler calls.
     * Handlers manage the instruction sequence internally.
     */
    bool execute(const ThreadedCode& code, void* context);

    /**
     * @brief Optimize for shallow tracing - prevent side effects.
     * @param bytecode The bytecode being compiled.
     * @return Modified bytecode suitable for shallow tracing.
     * 
     * Shallow tracing avoids:
     * - Inlining across method boundaries
     * - Speculative optimizations that assume type stability
     * - Aggressive loop transformations
     * 
     * This ensures clean handoff to Tier-2 for deep tracing.
     */
    Chunk optimizeShallowTracing(const Chunk& bytecode);

    /**
     * @brief Add inline caching entry for method dispatch.
     * @param call_offset Bytecode offset of the call.
     * @param method_id ID of the method being called.
     * @param handler_address Address of compiled method.
     * @return Inline cache entry ID.
     * 
     * Inline caches optimize repeated calls to the same method
     * by caching the resolved handler address.
     */
    uint32_t addInlineCacheEntry(uint64_t call_offset,
                                  uint64_t method_id,
                                  uint64_t handler_address);

    /**
     * @brief Validate inline cache entry.
     * @param cache_id The cache entry ID.
     * @param method_id Expected method ID.
     * @return true if cache is valid (hit), false on miss.
     * 
     * Called during execution to check if cached method is still valid.
     * On miss, the cache is updated and execution continues.
     */
    bool validateInlineCache(uint32_t cache_id, uint64_t method_id);

    /**
     * @brief Check if code cache is full.
     * @return true if cache needs eviction or expansion.
     */
    bool isCodeCacheFull() const;

    /**
     * @brief Get code cache statistics.
     * @return CacheStats with current metrics.
     */
    CacheStats getCacheStats() const;

    /**
     * @brief Clear code cache for recompilation or memory management.
     * 
     * Frees all compiled threaded code. Use when:
     * - Memory pressure requires freeing executable memory
     * - Profiling data has changed significantly
     * - Debugging requires clean state
     */
    void clearCodeCache();

private:
    /// Code cache for compiled threaded code (platform-specific executable memory)
    JITMemory code_cache_;

    /// Map from method_id to compiled threaded code
    std::unordered_map<uint64_t, std::unique_ptr<ThreadedCode>> compiled_methods_;

    /// Inline cache entries for method dispatch optimization
    struct InlineCacheEntry {
        uint64_t call_offset;       ///< Bytecode offset of call site
        uint64_t expected_method_id; ///< Expected method ID for validation
        uint64_t handler_address;   ///< Cached handler address
        int64_t hit_count;          ///< Cache hit count
        int64_t miss_count;         ///< Cache miss count
    };
    std::unordered_map<uint32_t, InlineCacheEntry> inline_caches_;
    uint32_t next_cache_id_;        ///< Next cache ID to allocate

    /// Statistics
    size_t total_compilations_;     ///< Total compilations performed
    int64_t total_compilation_time_us_;  ///< Total compilation time

    /**
     * @brief Generate machine code for a bytecode instruction.
     * @param opcode Bytecode opcode.
     * @param handler Handler function for this opcode.
     * @return Machine code bytes for this instruction.
     */
    std::vector<uint8_t> generateThreadedInstruction(
        uint8_t opcode, const HandlerFunction& handler);

    /**
     * @brief Emit direct call to handler function.
     * @param handler_address Address of handler to call.
     * @return Machine code bytes for the call instruction.
     */
    std::vector<uint8_t> emitHandlerCall(uint64_t handler_address);

    /**
     * @brief Allocate space in code cache.
     * @param size Bytes to allocate.
     * @return Pointer to allocated executable memory.
     */
    uint8_t* allocateCodeSpace(size_t size);
};

} // namespace neutron::jit

#endif // NEUTRON_JIT_TIER1_COMPILER_H
