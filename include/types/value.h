#pragma once

/*
 * Code Documentation: Value Type System (value.h)
 * ===============================================
 * 
 * This header defines the fundamental Value type - the atomic unit of data
 * in Neutron. Every variable, expression, and function argument is a Value.
 * 
 * What This File Includes:
 * ------------------------
 * - ValueType enum: Discriminated types (NIL, BOOLEAN, NUMBER, strings, objects, etc.)
 * - ValueUnion: Tagged union holding the actual data
 * - Value struct: The main value wrapper with type-safe accessors
 * 
 * How It Works:
 * -------------
 * Value uses a tagged union pattern. The 'type' field indicates what kind of
 * data is stored, and the 'as' union provides type-specific access. This avoids
 * the overhead of std::variant while maintaining type safety through accessor
 * methods.
 * 
 * Adding Features:
 * ----------------
 * - New primitive types: Add to ValueType enum, extend ValueUnion, add constructor
 * - New object types: Add to ValueType, add pointer field to ValueUnion
 * - Type conversion: Add methods to Value or implement in separate utility
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT access union members directly without checking type first
 * - Do NOT store raw pointers without GC tracking (use VM::allocate)
 * - Do NOT assume type stability - values can be reassigned different types
 * 
 * Memory Notes:
 * -------------
 * - Object types (strings, arrays, etc.) are GC-managed pointers
 * - Primitive types (bool, double) are stored inline
 * - sizeof(Value) should be kept small for stack efficiency
 */

#include <string>
#include <vector>

namespace neutron {

// Forward declarations - because circular dependencies love company
class Array;
class Object;
class ObjString;
class Callable;
class Module;
class Class;
class Instance;
class Buffer;

/**
 * @brief ValueType - All the ways a Value can be valuable (or nil).
 * 
 * This enum represents every type in the language. The JIT compiler
 * uses these for type specialization guards.
 */
enum class ValueType {
    NIL,         ///< The absence of value (like null, but more existential)
    BOOLEAN,     ///< true or false - the building blocks of decision-making
    NUMBER,      ///< Double-precision floating point (because integers are overrated)
    OBJ_STRING,  ///< Interned string object
    ARRAY,       ///< Dynamic array type
    OBJECT,      ///< Generic object (JSON-style key-value storage)
    CALLABLE,    ///< Function or callable object
    MODULE,      ///< Loaded module
    CLASS,       ///< Class definition
    INSTANCE,    ///< Class instance
    BUFFER       ///< Binary buffer for raw data
};

/**
 * @brief ValueUnion - Tagged union holding actual value data.
 * 
 * This union stores the payload for each Value type. Only one member
 * is active at a time, determined by the ValueType in the containing Value.
 * 
 * Warning: Accessing the wrong union member is undefined behavior.
 * Always check Value::type before accessing.
 */
union ValueUnion {
    bool boolean;       ///< Boolean value
    double number;      ///< Numeric value (double precision)
    ObjString* obj_string;  ///< String object pointer
    Array* array;       ///< Array object pointer
    Object* object;     ///< Generic object pointer
    Callable* callable; ///< Function/callable pointer
    Module* module;     ///< Module pointer
    Class* klass;       ///< Class pointer (named 'klass' to avoid C++ keyword)
    Instance* instance; ///< Class instance pointer
    Buffer* buffer;     ///< Binary buffer pointer
};

/**
 * @brief Value - The fundamental data type of Neutron.
 * 
 * Every value in Neutron - from numbers to functions to modules - is
 * represented by this struct. It's a tagged union that provides type-safe
 * access to the underlying data.
 * 
 * Design Philosophy:
 * - Small enough to pass by value (currently 16 bytes on 64-bit)
 * - Type field enables fast type checks without RTTI
 * - Object pointers are GC-tracked (don't store them outside GC roots)
 * 
 * Usage Example:
 * @code
 * Value v(42.0);
 * if (v.type == ValueType::NUMBER) {
 *     std::cout << v.as.number;  // Prints: 42
 * }
 * @endcode
 */
struct Value {
    ValueType type;  ///< The type tag - check this before accessing 'as'
    ValueUnion as;   ///< The actual data - access based on 'type'

    // Constructors - because values need to come from somewhere
    Value();                      ///< Default constructor (NIL)
    Value(std::nullptr_t);        ///< Construct from nullptr (NIL)
    Value(bool value);            ///< Construct from boolean
    Value(double value);          ///< Construct from number
    Value(ObjString* string);     ///< Construct from string object
    Value(const std::string& value);  ///< Construct from std::string (allocates)
    Value(Array* array);          ///< Construct from array
    Value(Object* object);        ///< Construct from generic object
    Value(Callable* callable);    ///< Construct from callable
    Value(Module* module);        ///< Construct from module
    Value(Class* klass);          ///< Construct from class
    Value(Instance* instance);    ///< Construct from instance
    Value(Buffer* buffer);        ///< Construct from buffer

    /**
     * @brief Convert value to string representation.
     * @return String representation suitable for printing/debugging.
     */
    std::string toString() const;

    // Type-specific accessors - use these instead of accessing 'as' directly
    /**
     * @brief Check if value is a string.
     * @return true if type is OBJ_STRING.
     */
    bool isString() const;
    
    /**
     * @brief Get value as string pointer.
     * @return Pointer to ObjString, or nullptr if not a string.
     */
    ObjString* asString() const;

    /**
     * @brief Check if value is a module.
     * @return true if type is MODULE.
     */
    bool isModule() const;
    
    /**
     * @brief Get value as module pointer.
     * @return Pointer to Module, or nullptr if not a module.
     */
    Module* asModule() const;

    /**
     * @brief Check if value is a buffer.
     * @return true if type is BUFFER.
     */
    bool isBuffer() const;
    
    /**
     * @brief Get value as buffer pointer.
     * @return Pointer to Buffer, or nullptr if not a buffer.
     */
    Buffer* asBuffer() const;
};

}
