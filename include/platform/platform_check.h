#ifndef NEUTRON_PLATFORM_CHECK_H
#define NEUTRON_PLATFORM_CHECK_H

/*
 * Platform Compatibility Checks
 * 
 * This header provides compile-time and runtime checks for cross-platform
 * compatibility. Use these macros to catch platform-specific issues early.
 */

// ============================================================================
// Platform Detection
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define NEUTRON_PLATFORM_WINDOWS 1
    #define NEUTRON_PLATFORM_NAME "Windows"
#elif defined(__linux__)
    #define NEUTRON_PLATFORM_LINUX 1
    #define NEUTRON_PLATFORM_NAME "Linux"
#elif defined(__APPLE__)
    #define NEUTRON_PLATFORM_MACOS 1
    #define NEUTRON_PLATFORM_NAME "macOS"
#else
    #define NEUTRON_PLATFORM_UNKNOWN 1
    #define NEUTRON_PLATFORM_NAME "Unknown"
#endif

// Architecture detection
#if defined(__x86_64__) || defined(_M_X64)
    #define NEUTRON_ARCH_X64 1
    #define NEUTRON_ARCH_NAME "x86_64"
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm64__)
    #define NEUTRON_ARCH_ARM64 1
    #define NEUTRON_ARCH_NAME "ARM64"
#elif defined(__i386__) || defined(_M_IX86)
    #define NEUTRON_ARCH_X86 1
    #define NEUTRON_ARCH_NAME "x86"
#elif defined(__arm__)
    #define NEUTRON_ARCH_ARM 1
    #define NEUTRON_ARCH_NAME "ARM"
#else
    #define NEUTRON_ARCH_UNKNOWN 1
    #define NEUTRON_ARCH_NAME "Unknown"
#endif

// ============================================================================
// Compile-Time Assertions
// ============================================================================

// Static assertion for struct sizes (catches ABI mismatches)
#define NEUTRON_STATIC_ASSERT_SIZE(struct_name, expected_size) \
    static_assert(sizeof(struct_name) == (expected_size), \
                  #struct_name " size mismatch: expected " #expected_size " bytes")

// Static assertion for struct alignment
#define NEUTRON_STATIC_ASSERT_ALIGN(struct_name, expected_align) \
    static_assert(alignof(struct_name) == (expected_align), \
                  #struct_name " alignment mismatch: expected " #expected_align " bytes")

// Static assertion for enum values
#define NEUTRON_STATIC_ASSERT_ENUM(enum_type, enum_value, expected) \
    static_assert(static_cast<int>(enum_type::enum_value) == (expected), \
                  #enum_type "::" #enum_value " value mismatch")

// ============================================================================
// JIT Platform Support Checks
// ============================================================================

// Check if JIT is supported on current platform
#if (NEUTRON_ARCH_X64 || NEUTRON_ARCH_ARM64) && (NEUTRON_PLATFORM_WINDOWS || NEUTRON_PLATFORM_LINUX || NEUTRON_PLATFORM_MACOS)
    #define NEUTRON_JIT_SUPPORTED 1
#else
    #define NEUTRON_JIT_SUPPORTED 0
    #warning "JIT compilation not supported on this platform"
#endif

// Check if ARM64 backend is available
#if NEUTRON_ARCH_ARM64
    #define NEUTRON_JIT_ARM64_AVAILABLE 1
#else
    #define NEUTRON_JIT_ARM64_AVAILABLE 0
#endif

// Check if x86-64 backend is available
#if NEUTRON_ARCH_X64
    #define NEUTRON_JIT_X64_AVAILABLE 1
#else
    #define NEUTRON_JIT_X64_AVAILABLE 0
#endif

// ============================================================================
// Cross-Compilation Helpers
// ============================================================================

// Endianness detection
#if defined(_WIN32)
    #define NEUTRON_LITTLE_ENDIAN 1  // Windows is always little-endian
#elif defined(__BYTE_ORDER__)
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define NEUTRON_LITTLE_ENDIAN 1
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        #define NEUTRON_BIG_ENDIAN 1
    #endif
#endif

#ifndef NEUTRON_LITTLE_ENDIAN
    #define NEUTRON_LITTLE_ENDIAN 1  // Default assumption
#endif

// ============================================================================
// Runtime Platform Info
// ============================================================================

namespace neutron::platform {

struct PlatformInfo {
    const char* platform_name;
    const char* arch_name;
    bool jit_supported;
    bool jit_x64_available;
    bool jit_arm64_available;
    bool is_little_endian;
    
    // Runtime-detected features
    bool has_sse2;
    bool has_avx;
    bool has_neon;
};

/**
 * Get current platform information.
 * Useful for debugging and logging.
 */
inline PlatformInfo getPlatformInfo() {
    PlatformInfo info;
    info.platform_name = NEUTRON_PLATFORM_NAME;
    info.arch_name = NEUTRON_ARCH_NAME;
    info.jit_supported = (NEUTRON_JIT_SUPPORTED != 0);
    info.jit_x64_available = (NEUTRON_JIT_X64_AVAILABLE != 0);
    info.jit_arm64_available = (NEUTRON_JIT_ARM64_AVAILABLE != 0);
    info.is_little_endian = (NEUTRON_LITTLE_ENDIAN != 0);
    
    // Feature detection (simplified - can be extended with CPUID)
    #if defined(__SSE2__) || (defined(_MSC_VER) && defined(_M_X64))
        info.has_sse2 = true;
    #else
        info.has_sse2 = false;
    #endif
    
    #if defined(__AVX__) || (defined(_MSC_VER) && defined(__AVX__))
        info.has_avx = true;
    #else
        info.has_avx = false;
    #endif
    
    #if defined(__ARM_NEON) || defined(__ARM_NEON__)
        info.has_neon = true;
    #else
        info.has_neon = false;
    #endif
    
    return info;
}

/**
 * Print platform information to stdout.
 * Useful for debugging cross-platform builds.
 */
inline void printPlatformInfo() {
    PlatformInfo info = getPlatformInfo();
    
    printf("=== Neutron Platform Information ===\n");
    printf("Platform: %s\n", info.platform_name);
    printf("Architecture: %s\n", info.arch_name);
    printf("JIT Supported: %s\n", info.jit_supported ? "Yes" : "No");
    printf("JIT x86-64 Backend: %s\n", info.jit_x64_available ? "Yes" : "No");
    printf("JIT ARM64 Backend: %s\n", info.jit_arm64_available ? "Yes" : "No");
    printf("Endianness: %s\n", info.is_little_endian ? "Little" : "Big");
    printf("SSE2: %s\n", info.has_sse2 ? "Yes" : "No");
    printf("AVX: %s\n", info.has_avx ? "Yes" : "No");
    printf("NEON: %s\n", info.has_neon ? "Yes" : "No");
    printf("====================================\n");
}

} // namespace neutron::platform

// ============================================================================
// Build Verification Macros
// ============================================================================

// Verify that critical structs have expected sizes (catches ABI issues)
#define NEUTRON_VERIFY_BUILD() \
    namespace neutron { \
        static_assert(sizeof(Value) <= 24, "Value struct too large - impacts performance"); \
        static_assert(sizeof(ValueUnion) == 8, "ValueUnion size mismatch on this platform"); \
    }

// ============================================================================
// Cross-Compilation Test Mode
// ============================================================================

// When NEUTRON_CROSS_COMPILE_TEST is defined, simulate another platform
#ifdef NEUTRON_CROSS_COMPILE_TEST
    #if NEUTRON_CROSS_COMPILE_TEST == 1
        // Simulate Linux x64
        #undef NEUTRON_PLATFORM_WINDOWS
        #define NEUTRON_PLATFORM_LINUX 1
        #undef NEUTRON_PLATFORM_NAME
        #define NEUTRON_PLATFORM_NAME "Linux (simulated)"
    #elif NEUTRON_CROSS_COMPILE_TEST == 2
        // Simulate macOS ARM64
        #undef NEUTRON_PLATFORM_WINDOWS
        #define NEUTRON_PLATFORM_MACOS 1
        #undef NEUTRON_ARCH_X64
        #define NEUTRON_ARCH_ARM64 1
        #undef NEUTRON_PLATFORM_NAME
        #define NEUTRON_PLATFORM_NAME "macOS ARM64 (simulated)"
        #undef NEUTRON_ARCH_NAME
        #define NEUTRON_ARCH_NAME "ARM64 (simulated)"
    #endif
#endif

#endif // NEUTRON_PLATFORM_CHECK_H
