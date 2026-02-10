#ifndef NEUTRON_JIT_MEMORY_H
#define NEUTRON_JIT_MEMORY_H

#include <cstdint>
#include <cstddef>

namespace neutron::jit {

/**
 * Platform-specific JIT memory allocator.
 * 
 * Allocates memory suitable for JIT code execution using OS-specific APIs:
 * - Linux: mmap with PROT_READ|PROT_WRITE, then mprotect to add PROT_EXEC
 * - macOS: mmap with MAP_JIT flag + pthread_jit_write_protect_np for W^X
 * - Windows: VirtualAlloc with PAGE_EXECUTE_READWRITE
 * 
 * This replaces std::vector<uint8_t> which cannot reliably be made executable
 * across platforms.
 */
class JITMemory {
public:
    JITMemory();
    ~JITMemory();

    // Non-copyable, non-movable
    JITMemory(const JITMemory&) = delete;
    JITMemory& operator=(const JITMemory&) = delete;
    JITMemory(JITMemory&&) = delete;
    JITMemory& operator=(JITMemory&&) = delete;

    /**
     * Initialize the JIT memory region with the given capacity.
     * @param capacity Size in bytes of the JIT code cache
     * @return true if allocation succeeded
     */
    bool initialize(size_t capacity);

    /**
     * Allocate a contiguous block from the code cache.
     * @param size Number of bytes to allocate
     * @return Pointer to writable memory, or nullptr if full
     */
    uint8_t* allocate(size_t size);

    /**
     * Make the entire allocated region executable.
     * On macOS Apple Silicon, this toggles from write mode to execute mode.
     * @return true if the operation succeeded
     */
    bool makeExecutable();

    /**
     * Make the region writable again (for appending more code).
     * On macOS Apple Silicon, this toggles from execute mode to write mode.
     * @return true if the operation succeeded
     */
    bool makeWritable();

    /**
     * Get the base pointer of the code cache.
     */
    uint8_t* data() const { return base_; }

    /**
     * Get the current offset (bytes used).
     */
    size_t offset() const { return offset_; }

    /**
     * Get the total capacity.
     */
    size_t capacity() const { return capacity_; }

    /**
     * Check if initialized.
     */
    bool isInitialized() const { return base_ != nullptr; }

    /**
     * Reset the allocator (keeps the memory, resets offset).
     */
    void reset();

    /**
     * Free all memory and reset state.
     */
    void release();

private:
    uint8_t* base_ = nullptr;
    size_t capacity_ = 0;
    size_t offset_ = 0;
};

} // namespace neutron::jit

#endif // NEUTRON_JIT_MEMORY_H
