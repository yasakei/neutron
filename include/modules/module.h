#pragma once

/*
 * Code Documentation: Module System (module.h)
 * ============================================
 * 
 * This header defines the Module class - the foundation of Neutron's
 * module system for code organization and reuse.
 * 
 * What This File Includes:
 * ------------------------
 * - Module class: Container for module code and exports
 * - Environment integration: Modules have isolated scopes
 * - Native module support: C++ modules via dynamic loading
 * 
 * How It Works:
 * -------------
 * Modules provide isolated namespaces for code:
 * 1. Each module has its own Environment for variables
 * 2. Exports are defined via module.define()
 * 3. Imports create references to exported values
 * 4. Native modules (.so/.dll) can register functions
 * 
 * Module Types:
 * - Source modules: Neutron code loaded from .nt files
 * - Native modules: C++ code loaded from shared libraries
 * - Built-in modules: Core modules (sys, json, http, etc.)
 * 
 * Adding Features:
 * ----------------
 * - New module types: Extend Module class or create subclasses
 * - Module lifecycle: Add init/shutdown hooks for native modules
 * - Module caching: Extend loader with caching strategies
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT share environments between modules (breaks isolation)
 * - Do NOT access module internals without proper locking
 * - Do NOT unload native modules while code is executing
 * - Do NOT modify module exports after initial loading
 */

#include <string>
#include <memory>
#include <vector>
#include "types/value.h"
#include "types/object.h"
#include "runtime/environment.h"
#include "stmt.h"

namespace neutron {

/**
 * @brief Module - Code Organization and Reuse Container.
 * 
 * A Module represents a loaded Neutron module with:
 * - Isolated environment for variables and functions
 * - Export table for public API
 * - Optional native library handle for C++ modules
 * 
 * Modules are created by:
 * - Loading .nt source files (compiled and executed)
 * - Loading native libraries (.so/.dll with registration)
 * - Creating built-in modules (sys, json, http, etc.)
 * 
 * Example Usage:
 * @code
 * // Create a module
 * auto env = std::make_shared<Environment>();
 * Module math("math", env);
 * 
 * // Define exports
 * math.define("PI", Value(3.14159));
 * math.define("sqrt", sqrt_function);
 * 
 * // Access exports
 * Value pi = math.get("PI");
 * @endcode
 */
class Module : public Object {
public:
    std::string name;                              ///< Module name (for imports and debugging)
    std::shared_ptr<Environment> env;              ///< Module's isolated environment
    void* handle;                                  ///< Native library handle (nullptr for source modules)

    /**
     * @brief Construct a new module.
     * @param name Module name.
     * @param env Environment for module scope.
     * @param handle Native library handle (optional, nullptr for source modules).
     */
    Module(const std::string& name, std::shared_ptr<Environment> env, void* handle = nullptr);
    
    /**
     * @brief Destroy the module, releasing native handle if present.
     */
    ~Module() override;

    /**
     * @brief Get an exported value from the module.
     * @param name Export name to look up.
     * @return The exported value, or NIL if not found.
     * 
     * Searches the module's environment for the named export.
     * Returns NIL for undefined exports (no error).
     */
    Value get(const std::string& name);

    /**
     * @brief Define an export in the module.
     * @param name Export name.
     * @param value Value to export.
     * 
     * Exports are values accessible to code that imports this module.
     * Functions, classes, and constants can all be exported.
     */
    void define(const std::string& name, const Value& value);

    /**
     * @brief Get string representation of the module.
     * @return Module description string.
     */
    std::string toString() const override;
    
    /**
     * @brief Mark module for garbage collection.
     * 
     * Marks the environment and any GC-tracked values.
     * Called during GC mark phase.
     */
    void mark() override;
};

}
