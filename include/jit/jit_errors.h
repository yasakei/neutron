#ifndef NEUTRON_JIT_ERRORS_H
#define NEUTRON_JIT_ERRORS_H

/*
 * JIT Error Handling - Standardized error codes and result types
 */

#include <string>
#include <cstdint>

namespace neutron::jit {

/**
 * @brief JIT error codes - standardized across all JIT components.
 */
enum class JITErrorCode : uint8_t {
    SUCCESS = 0,              ///< Operation completed successfully
    
    // General errors
    UNKNOWN_ERROR = 1,        ///< Unknown/unspecified error
    INVALID_ARGUMENT = 2,     ///< Invalid argument provided
    NULL_POINTER = 3,         ///< Null pointer where value expected
    
    // Compilation errors
    COMPILATION_FAILED = 10,      ///< General compilation failure
    TRACE_RECORDING_FAILED = 11,  ///< Failed to record execution trace
    TRACE_VALIDATION_FAILED = 12, ///< Trace validation failed
    TRACE_TOO_LONG = 13,          ///< Trace exceeds maximum length
    NESTED_LOOP_DETECTED = 14,    ///< Nested loops not supported
    UNSUPPORTED_OPERATION = 15,   ///< Operation not supported in JIT
    
    // Code generation errors
    CODEGEN_FAILED = 20,          ///< Native code generation failed
    REGISTER_ALLOCATION_FAILED = 21,  ///< No available registers
    INVALID_INSTRUCTION = 22,     ///< Invalid instruction encountered
    
    // Memory errors
    MEMORY_ALLOCATION_FAILED = 30,  ///< Failed to allocate executable memory
    CODE_CACHE_FULL = 31,           ///< Code cache is full
    MEMORY_PROTECTION_FAILED = 32,  ///< Failed to change memory protection
    
    // Runtime errors
    EXECUTION_FAILED = 40,          ///< Compiled code execution failed
    GUARD_FAILURE = 41,             ///< Type guard failed (deoptimization)
    OSR_FAILURE = 42,               ///< On-stack replacement failed
    
    // Lookup errors
    TRACE_NOT_FOUND = 50,           ///< Requested trace not found
    METHOD_NOT_FOUND = 51,          ///< Requested method not found
    GLOBAL_NOT_FOUND = 52,          ///< Global variable not found
    
    // Platform errors
    PLATFORM_NOT_SUPPORTED = 60,    ///< Current platform not supported
    ARCHITECTURE_MISMATCH = 61,     ///< Architecture mismatch
};

/**
 * @brief Human-readable error message for each error code.
 */
inline const char* getErrorMessage(JITErrorCode code) {
    switch (code) {
        case JITErrorCode::SUCCESS: return "Success";
        case JITErrorCode::UNKNOWN_ERROR: return "Unknown error";
        case JITErrorCode::INVALID_ARGUMENT: return "Invalid argument";
        case JITErrorCode::NULL_POINTER: return "Null pointer";
        case JITErrorCode::COMPILATION_FAILED: return "Compilation failed";
        case JITErrorCode::TRACE_RECORDING_FAILED: return "Trace recording failed";
        case JITErrorCode::TRACE_VALIDATION_FAILED: return "Trace validation failed";
        case JITErrorCode::TRACE_TOO_LONG: return "Trace too long";
        case JITErrorCode::NESTED_LOOP_DETECTED: return "Nested loop detected";
        case JITErrorCode::UNSUPPORTED_OPERATION: return "Unsupported operation";
        case JITErrorCode::CODEGEN_FAILED: return "Code generation failed";
        case JITErrorCode::REGISTER_ALLOCATION_FAILED: return "Register allocation failed";
        case JITErrorCode::INVALID_INSTRUCTION: return "Invalid instruction";
        case JITErrorCode::MEMORY_ALLOCATION_FAILED: return "Memory allocation failed";
        case JITErrorCode::CODE_CACHE_FULL: return "Code cache full";
        case JITErrorCode::MEMORY_PROTECTION_FAILED: return "Memory protection failed";
        case JITErrorCode::EXECUTION_FAILED: return "Execution failed";
        case JITErrorCode::GUARD_FAILURE: return "Guard failure (deoptimization)";
        case JITErrorCode::OSR_FAILURE: return "On-stack replacement failed";
        case JITErrorCode::TRACE_NOT_FOUND: return "Trace not found";
        case JITErrorCode::METHOD_NOT_FOUND: return "Method not found";
        case JITErrorCode::GLOBAL_NOT_FOUND: return "Global variable not found";
        case JITErrorCode::PLATFORM_NOT_SUPPORTED: return "Platform not supported";
        case JITErrorCode::ARCHITECTURE_MISMATCH: return "Architecture mismatch";
        default: return "Unknown error code";
    }
}

/**
 * @brief Simple result type for void-returning JIT operations.
 */
struct JITResult {
    JITErrorCode code;
    std::string message;
    
    JITResult() : code(JITErrorCode::SUCCESS) {}
    explicit JITResult(JITErrorCode c) : code(c) {}
    JITResult(JITErrorCode c, const std::string& m) : code(c), message(m) {}
    
    bool ok() const { return code == JITErrorCode::SUCCESS; }
    bool failed() const { return code != JITErrorCode::SUCCESS; }
    
    static JITResult OK() { return JITResult(); }
    static JITResult Err(JITErrorCode c, const std::string& m = "") { return JITResult(c, m); }
};

/**
 * @brief Result type for JIT operations returning a value.
 */
template<typename T>
struct JITResultT {
    JITErrorCode code;
    std::string message;
    T value;
    
    JITResultT() : code(JITErrorCode::SUCCESS), value() {}
    explicit JITResultT(const T& v) : code(JITErrorCode::SUCCESS), value(v) {}
    JITResultT(JITErrorCode c, const std::string& m) : code(c), message(m), value() {}
    
    bool ok() const { return code == JITErrorCode::SUCCESS; }
    bool failed() const { return code != JITErrorCode::SUCCESS; }
    
    static JITResultT OK(const T& v) { return JITResultT(v); }
    static JITResultT Err(JITErrorCode c, const std::string& m = "") { return JITResultT(c, m); }
    
    T& getValue() { return value; }
    const T& getValue() const { return value; }
};

// Convenience typedef for common return types
using JITResultU64 = JITResultT<uint64_t>;
using JITResultPtr = JITResultT<uint8_t*>;

} // namespace neutron::jit

#endif // NEUTRON_JIT_ERRORS_H
