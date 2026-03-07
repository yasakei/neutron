#pragma once

/*
 * Code Documentation: Object Base Class (object.h)
 * ================================================
 * 
 * This header defines the base Object class - the foundation of all
 * heap-allocated, garbage-collected types in Neutron.
 * 
 * What This File Includes:
 * ------------------------
 * - ObjType enum: Runtime type identification without RTTI
 * - Object class: Abstract base for all GC-managed objects
 * 
 * How It Works:
 * -------------
 * Every heap-allocated object in Neutron inherits from Object. The base
 * class provides:
 * - Type tagging for fast runtime type checks
 * - GC mark flag for garbage collection
 * - Virtual destructor for proper cleanup
 * - toString() for debugging and string conversion
 * 
 * The GC uses the 'is_marked' flag during mark-and-sweep collection.
 * Objects not marked after the mark phase are swept (deleted).
 * 
 * Adding Features:
 * ----------------
 * - New object types: Add to ObjType enum, inherit from Object
 * - Custom GC behavior: Override mark() to mark child objects
 * - Custom cleanup: Override sweep() for non-trivial destructors
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT instantiate Object directly (it's abstract anyway)
 * - Do NOT forget to call base class mark() when overriding
 * - Do NOT use dynamic_cast - use obj_type for type checks
 * - Do NOT store Object pointers without GC tracking
 */

#include <string>
#include <cstdint>

namespace neutron {

/**
 * @brief ObjType - Runtime type identification for all objects.
 * 
 * This enum provides fast type identification without using C++ RTTI.
 * Each object type has a unique tag that can be checked in O(1) time.
 * 
 * The JIT compiler uses these types for guard specialization.
 * Values are ordered roughly by frequency of use for cache efficiency.
 */
enum class ObjType : uint8_t {
    OBJ_GENERIC = 0,      ///< Base object type (rarely used directly)
    OBJ_FUNCTION,         ///< Function/closure object
    OBJ_NATIVE_FN,        ///< Native C++ function wrapper
    OBJ_BOUND_METHOD,     ///< Bound method (method + receiver)
    OBJ_BOUND_ARRAY_METHOD,   ///< Bound array method
    OBJ_BOUND_STRING_METHOD,  ///< Bound string method
    OBJ_CLASS,            ///< Class definition
    OBJ_INSTANCE,         ///< Class instance (most common in OOP code)
    OBJ_STRING,           ///< Interned string object
    OBJ_ARRAY,            ///< Dynamic array
    OBJ_JSON_OBJECT,      ///< JSON-style object {key: value}
    OBJ_JSON_ARRAY,       ///< JSON-style array [values]
    OBJ_MODULE,           ///< Loaded module
    OBJ_BUFFER,           ///< Binary data buffer
};

/**
 * @brief Object - Base class for all garbage-collected objects.
 * 
 * Every heap-allocated object in Neutron inherits from this class.
 * It provides the infrastructure for:
 * - Runtime type identification (via obj_type)
 * - Garbage collection marking (via is_marked)
 * - String conversion (via toString())
 * 
 * Memory Management:
 * Objects are allocated via VM::allocate<T>() which automatically
 * registers them with the garbage collector. Never delete objects
 * manually - let the GC handle it.
 * 
 * Type Checking:
 * Use the obj_type field instead of dynamic_cast for performance:
 * @code
 * if (obj->obj_type == ObjType::OBJ_STRING) {
 *     ObjString* str = static_cast<ObjString*>(obj);
 *     // Use str...
 * }
 * @endcode
 * 
 * Garbage Collection:
 * During GC, objects are marked if they're reachable from roots.
 * Override mark() to mark any child objects your type holds.
 * The default implementation just sets is_marked = true.
 */
class Object {
public:
    ObjType obj_type = ObjType::OBJ_GENERIC;  ///< Runtime type tag - check this instead of using RTTI
    bool is_marked = false;                    ///< GC mark flag - true if object is reachable
    
    /**
     * @brief Virtual destructor - ensures proper cleanup of derived types.
     */
    virtual ~Object() = default;
    
    /**
     * @brief Convert object to string representation.
     * @return String suitable for printing/debugging.
     * 
     * Pure virtual - every object type must implement this.
     * Think of it as the object's way of introducing itself.
     */
    virtual std::string toString() const = 0;
    
    /**
     * @brief Mark this object as reachable during GC.
     * 
     * Override this method to mark any child objects your type holds.
     * The default implementation just sets is_marked = true.
     * 
     * Example for a type with children:
     * @code
     * void MyClass::mark() {
     *     Object::mark();  // Always call base first
     *     if (child_object) child_object->mark();
     * }
     * @endcode
     */
    virtual void mark() { is_marked = true; }
    
    /**
     * @brief Sweep - cleanup called on unmarked objects during GC.
     * 
     * Override this for non-trivial cleanup that can't be handled
     * by the destructor alone. Most types don't need to override this.
     */
    virtual void sweep() {} // Default implementation, can be overridden
};

}
