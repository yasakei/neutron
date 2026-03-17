#include "jit/jit_memory.h"
#include <cstring>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
    #ifdef __APPLE__
        #include <pthread.h>
        #include <libkern/OSCacheControl.h>
    #endif
#endif

namespace neutron::jit {

JITMemory::JITMemory() = default;

JITMemory::~JITMemory() {
    release();
}

JITResult JITMemory::initialize(size_t capacity) {
    if (base_) {
        release();
    }

#ifdef _WIN32
    // Windows: VirtualAlloc with PAGE_EXECUTE_READWRITE
    base_ = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, capacity, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!base_) {
        return JITResult::Err(JITErrorCode::MEMORY_ALLOCATION_FAILED, "VirtualAlloc failed");
    }
#elif defined(__APPLE__)
    // macOS: Use MAP_JIT for Apple Silicon W^X support
    // MAP_JIT allows toggling between writable and executable via pthread_jit_write_protect_np
    int flags = MAP_PRIVATE | MAP_ANON;
    #ifdef MAP_JIT
        flags |= MAP_JIT;
    #endif
    base_ = static_cast<uint8_t*>(
        mmap(nullptr, capacity, PROT_READ | PROT_WRITE | PROT_EXEC,
             flags, -1, 0));
    if (base_ == MAP_FAILED) {
        base_ = nullptr;
        return JITResult::Err(JITErrorCode::MEMORY_ALLOCATION_FAILED, "mmap failed");
    }
    // Start in writable mode on Apple Silicon
    #if defined(__aarch64__) || defined(__arm64__)
        pthread_jit_write_protect_np(0); // 0 = writable
    #endif
#else
    // Linux: mmap with RWX permissions
    base_ = static_cast<uint8_t*>(
        mmap(nullptr, capacity, PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_PRIVATE | MAP_ANON, -1, 0));
    if (base_ == MAP_FAILED) {
        base_ = nullptr;
        return false;
    }
#endif

    capacity_ = capacity;
    offset_ = 0;
    return JITResult::OK();
}

uint8_t* JITMemory::allocate(size_t size) {
    if (!base_ || offset_ + size > capacity_) {
        return nullptr;
    }
    uint8_t* ptr = base_ + offset_;
    offset_ += size;
    return ptr;
}

JITResultPtr JITMemory::allocateWithResult(size_t size) {
    if (!base_) {
        return JITResultPtr::Err(
            JITErrorCode::MEMORY_ALLOCATION_FAILED,
            "JIT memory not initialized"
        );
    }
    
    if (offset_ + size > capacity_) {
        return JITResultPtr::Err(
            JITErrorCode::CODE_CACHE_FULL,
            "Insufficient space in code cache (need " + std::to_string(size) + 
            " bytes, have " + std::to_string(availableSpace()) + " bytes available)"
        );
    }
    
    uint8_t* ptr = base_ + offset_;
    offset_ += size;
    return JITResultPtr::OK(ptr);
}

JITResult JITMemory::makeExecutable() {
    if (!base_) return JITResult::Err(JITErrorCode::MEMORY_ALLOCATION_FAILED, "Not initialized");

#ifdef _WIN32
    // Windows: memory was already allocated with PAGE_EXECUTE_READWRITE
    // Flush instruction cache to ensure coherency
    FlushInstructionCache(GetCurrentProcess(), base_, offset_);
    return JITResult::OK();
#elif defined(__APPLE__)
    #if defined(__aarch64__) || defined(__arm64__)
        // Apple Silicon: toggle W^X to executable mode
        pthread_jit_write_protect_np(1); // 1 = executable
        // Flush instruction cache
        sys_icache_invalidate(base_, offset_);
    #else
        // Intel Mac: just flush instruction cache
        // Memory was already mapped with PROT_EXEC
    #endif
    return JITResult::OK();
#else
    // Linux: memory was already mapped with PROT_EXEC
    // Flush instruction cache
    #if defined(__GNUC__)
        __builtin___clear_cache(reinterpret_cast<char*>(base_),
                                reinterpret_cast<char*>(base_ + offset_));
    #endif
    return JITResult::OK();
#endif
}

JITResult JITMemory::makeWritable() {
    if (!base_) return JITResult::Err(JITErrorCode::MEMORY_ALLOCATION_FAILED, "Not initialized");

#ifdef _WIN32
    // Windows: already PAGE_EXECUTE_READWRITE
    return JITResult::OK();
#elif defined(__APPLE__)
    #if defined(__aarch64__) || defined(__arm64__)
        // Apple Silicon: toggle W^X to writable mode
        pthread_jit_write_protect_np(0); // 0 = writable
    #endif
    return JITResult::OK();
#else
    // Linux: already mapped RWX
    return JITResult::OK();
#endif
}

void JITMemory::reset() {
    offset_ = 0;
}

void JITMemory::release() {
    if (!base_) return;

#ifdef _WIN32
    VirtualFree(base_, 0, MEM_RELEASE);
#else
    munmap(base_, capacity_);
#endif

    base_ = nullptr;
    capacity_ = 0;
    offset_ = 0;
}

bool JITMemory::hasSpace(size_t size) const {
    return base_ != nullptr && (offset_ + size) <= capacity_;
}

size_t JITMemory::availableSpace() const {
    if (!base_) return 0;
    return capacity_ - offset_;
}

float JITMemory::utilizationRatio() const {
    if (!base_ || capacity_ == 0) return 0.0f;
    return static_cast<float>(offset_) / static_cast<float>(capacity_);
}

void JITMemory::compact() {
    // Simple compaction: reset offset to 0
    // This effectively discards all allocated code
    // A more sophisticated implementation would track live regions
    // and copy them to the beginning of the cache
    offset_ = 0;
}

} // namespace neutron::jit
