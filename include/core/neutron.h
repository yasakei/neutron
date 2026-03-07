#ifndef neutron_h
#define neutron_h

/*
 * Code Documentation: Neutron C API (neutron.h)
 * =============================================
 * 
 * This header provides the C API for embedding Neutron in other applications
 * and for writing native modules. It exposes a stable ABI for interoperability.
 * 
 * What This File Includes:
 * ------------------------
 * - Opaque type declarations (NeutronVM, NeutronValue)
 * - Platform-specific export macros (NEUTRON_API, NEUTRON_MODULE_EXPORT)
 * - Value management functions (type checking, creation, access)
 * - Native function registration API
 * 
 * How It Works:
 * -------------
 * The C API uses opaque pointers to hide C++ implementation details.
 * Values are passed as NeutronValue* pointers, and the VM is managed
 * through NeutronVM handles. This provides ABI stability across
 * compiler versions and platforms.
 * 
 * Adding Features:
 * ----------------
 * - New value types: Add to NeutronType enum, add getter/creator functions
 * - New VM operations: Add functions taking NeutronVM* as first parameter
 * - Module exports: Use NEUTRON_MODULE_EXPORT for shared library symbols
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT access C++ internals directly from C code
 * - Do NOT mix C API and C++ API without proper conversion
 * - Do NOT forget to check return values for error conditions
 * - Do NOT use NEUTRON_API for your own module exports (use NEUTRON_MODULE_EXPORT)
 * 
 * Platform Notes:
 * ---------------
 * - Windows: Uses __declspec(dllexport/dllimport) for DLL visibility
 * - Unix: Uses __attribute__((visibility("default"))) for symbol export
 * - Modules: NEUTRON_MODULE_EXPORT ensures your native functions are visible
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque pointers for Neutron data structures
// These hide the C++ implementation details - treat them as black boxes
typedef struct NeutronVM NeutronVM;
typedef struct NeutronValue NeutronValue;

/**
 * @brief Platform-specific export macros for shared libraries.
 * 
 * NEUTRON_API: Used for the core Neutron library exports
 * NEUTRON_MODULE_EXPORT: Used for native module exports
 * 
 * On Windows, these expand to __declspec(dllexport/dllimport).
 * On Unix-like systems, they use visibility attributes.
 */
#ifdef _WIN32
    #ifdef BUILDING_NEUTRON
        #define NEUTRON_API __declspec(dllexport)
    #else
        #define NEUTRON_API __declspec(dllimport)
    #endif
    // For native module exports (always dllexport when building a module)
    #define NEUTRON_MODULE_EXPORT __declspec(dllexport)
#else
    #define NEUTRON_API
    // On Unix, use visibility attribute for shared library exports
    #define NEUTRON_MODULE_EXPORT __attribute__((visibility("default")))
    // Compatibility: define __declspec as no-op on non-Windows so modules
    // that hardcode __declspec(dllexport) still compile on Linux/macOS
    #ifndef __declspec
        #define __declspec(x)
    #endif
#endif

// --- Value Management ---

/**
 * @brief NeutronType - All types exposed through the C API.
 * 
 * This is a subset of the full ValueType enum, exposing only
 * the basic types suitable for C interop.
 */
typedef enum {
    NEUTRON_NIL,      ///< No value (nil)
    NEUTRON_BOOLEAN,  ///< Boolean (true/false)
    NEUTRON_NUMBER,   ///< Double-precision number
    NEUTRON_STRING    ///< String value
} NeutronType;

// --- Type Checking ---
// Use these to check what kind of value you're dealing with

/**
 * @brief Get the type of a value.
 * @param value The value to check.
 * @return The NeutronType of the value.
 */
NEUTRON_API NeutronType neutron_get_type(NeutronValue* value);

/**
 * @brief Check if a value is nil.
 * @param value The value to check.
 * @return true if the value is nil.
 */
NEUTRON_API bool neutron_is_nil(NeutronValue* value);

/**
 * @brief Check if a value is a boolean.
 * @param value The value to check.
 * @return true if the value is a boolean.
 */
NEUTRON_API bool neutron_is_boolean(NeutronValue* value);

/**
 * @brief Check if a value is a number.
 * @param value The value to check.
 * @return true if the value is a number.
 */
NEUTRON_API bool neutron_is_number(NeutronValue* value);

/**
 * @brief Check if a value is a string.
 * @param value The value to check.
 * @return true if the value is a string.
 */
NEUTRON_API bool neutron_is_string(NeutronValue* value);

// --- Value Getters ---
// Extract actual values from NeutronValue pointers

/**
 * @brief Get boolean value.
 * @param value The value (must be a boolean).
 * @return The boolean value (true or false).
 */
NEUTRON_API bool neutron_get_boolean(NeutronValue* value);

/**
 * @brief Get numeric value.
 * @param value The value (must be a number).
 * @return The numeric value as double.
 */
NEUTRON_API double neutron_get_number(NeutronValue* value);

/**
 * @brief Get string value.
 * @param value The value (must be a string).
 * @param length Optional output parameter for string length.
 * @return Null-terminated C string (do not free).
 */
NEUTRON_API const char* neutron_get_string(NeutronValue* value, size_t* length);

// --- Value Creators ---
// Create new NeutronValue instances

/**
 * @brief Create a nil value.
 * @return Pointer to a nil value.
 */
NEUTRON_API NeutronValue* neutron_new_nil();

/**
 * @brief Create a boolean value.
 * @param value The boolean value (true or false).
 * @return Pointer to the new boolean value.
 */
NEUTRON_API NeutronValue* neutron_new_boolean(bool value);

/**
 * @brief Create a numeric value.
 * @param value The numeric value.
 * @return Pointer to the new number value.
 */
NEUTRON_API NeutronValue* neutron_new_number(double value);

/**
 * @brief Create a string value.
 * @param vm The VM instance (for memory allocation).
 * @param chars The string content.
 * @param length The string length.
 * @return Pointer to the new string value.
 */
NEUTRON_API NeutronValue* neutron_new_string(NeutronVM* vm, const char* chars, size_t length);

// --- Native Functions ---

/**
 * @brief NeutronNativeFn - Function signature for native C functions.
 * 
 * Native functions receive:
 * - vm: The VM instance (for accessing globals, allocating, etc.)
 * - arg_count: Number of arguments passed
 * - args: Array of argument values
 * 
 * Return the result value, or nil if no result.
 * 
 * Example:
 * @code
 * NeutronValue* my_native_fn(NeutronVM* vm, int arg_count, NeutronValue** args) {
 *     if (arg_count != 2) return neutron_new_nil();
 *     double a = neutron_get_number(args[0]);
 *     double b = neutron_get_number(args[1]);
 *     return neutron_new_number(a + b);
 * }
 * @endcode
 */
typedef NeutronValue* (*NeutronNativeFn)(NeutronVM* vm, int arg_count, NeutronValue** args);

/**
 * @brief Register a native function with the VM.
 * 
 * Once registered, the function can be called from Neutron code
 * by name. The function persists for the lifetime of the VM.
 * 
 * @param vm The VM instance.
 * @param name The name to register the function under.
 * @param function The native function to register.
 * @param arity Expected number of arguments (-1 for variadic).
 */
NEUTRON_API void neutron_define_native(NeutronVM* vm, const char* name, NeutronNativeFn function, int arity);

#ifdef __cplusplus
}
#endif

#endif // neutron_h
