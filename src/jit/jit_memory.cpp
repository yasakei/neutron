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

bool JITMemory::initialize(size_t capacity) {
    if (base_) {
        release();
    }

#ifdef _WIN32
    // Windows: VirtualAlloc with PAGE_EXECUTE_READWRITE
    base_ = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, capacity, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!base_) {
        return false;
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
        return false;
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
    return true;
}

uint8_t* JITMemory::allocate(size_t size) {
    if (!base_ || offset_ + size > capacity_) {
        return nullptr;
    }
    uint8_t* ptr = base_ + offset_;
    offset_ += size;
    return ptr;
}

bool JITMemory::makeExecutable() {
    if (!base_) return false;

#ifdef _WIN32
    // Windows: memory was already allocated with PAGE_EXECUTE_READWRITE
    // Flush instruction cache to ensure coherency
    FlushInstructionCache(GetCurrentProcess(), base_, offset_);
    return true;
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
    return true;
#else
    // Linux: memory was already mapped with PROT_EXEC
    // Flush instruction cache
    #if defined(__GNUC__)
        __builtin___clear_cache(reinterpret_cast<char*>(base_),
                                reinterpret_cast<char*>(base_ + offset_));
    #endif
    return true;
#endif
}

bool JITMemory::makeWritable() {
    if (!base_) return false;

#ifdef _WIN32
    // Windows: already PAGE_EXECUTE_READWRITE
    return true;
#elif defined(__APPLE__)
    #if defined(__aarch64__) || defined(__arm64__)
        // Apple Silicon: toggle W^X to writable mode
        pthread_jit_write_protect_np(0); // 0 = writable
    #endif
    return true;
#else
    // Linux: already mapped RWX
    return true;
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

} // namespace neutron::jit
