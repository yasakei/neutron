#pragma once

/*
 * Code Documentation: Environment/Scope Management (environment.h)
 * ================================================================
 * 
 * This header defines the Environment class - the foundation of
 * Neutron's variable scoping and lexical environment system.
 * 
 * What This File Includes:
 * ------------------------
 * - Environment class: Lexical scope container
 * - Variable storage: Map-based value storage
 * - Scope chaining: Enclosing environment for closures
 * 
 * How It Works:
 * -------------
 * Environments form a chain representing lexical scope:
 * 
 * @code
 * Global Environment
 * └── Function Environment (enclosing = Global)
 *     └── Block Environment (enclosing = Function)
 *         └── Inner Block (enclosing = Block)
 * @endcode
 * 
 * Variable Resolution:
 * 1. Check current environment's values map
 * 2. If not found, check enclosing environment
 * 3. Continue up the chain until found or global
 * 4. If not found anywhere → undefined reference error
 * 
 * Adding Features:
 * ----------------
 * - New scope types: Extend Environment with type-specific behavior
 * - Variable watchers: Add callbacks for variable changes
 * - Scope introspection: Add methods for debugging/REPL
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT modify enclosing chain after creation (breaks scoping)
 * - Do NOT share environments between threads without locking
 * - Do NOT store raw Value pointers (use Value objects)
 * - Do NOT bypass the chain for variable lookup
 */

#include <memory>
#include <string>
#include <unordered_map>
#include "types/value.h"

namespace neutron {

/**
 * @brief Environment - Lexical Scope Container.
 * 
 * An Environment represents a single lexical scope containing:
 * - Variable bindings (name → Value map)
 * - Reference to enclosing scope (for nested scopes)
 * 
 * Environments are created for:
 * - Global scope (one per VM)
 * - Function calls (one per invocation)
 * - Block scopes (for block-local variables)
 * - Class and module scopes
 * 
 * Variable Lookup Example:
 * @code
 * var x = 10;           // Global env: x = 10
 * {
 *     var x = 20;       // Block env: x = 20, enclosing → Global
 *     print(x);         // Finds block's x (20)
 * }
 * print(x);             // Finds global x (10)
 * @endcode
 */
class Environment {
public:
    /// Enclosing environment (nullptr for global scope)
    std::shared_ptr<Environment> enclosing;
    
    /// Variable bindings in this scope
    std::unordered_map<std::string, Value> values;

    /**
     * @brief Create a global environment (no enclosing).
     */
    Environment();
    
    /**
     * @brief Create a nested environment.
     * @param enclosing Parent environment for variable lookup.
     */
    Environment(std::shared_ptr<Environment> enclosing);

    /**
     * @brief Define a new variable in this environment.
     * @param name Variable name.
     * @param value Initial value.
     * 
     * Variables are always defined in the current environment,
     * never in the enclosing scope.
     */
    void define(const std::string& name, const Value& value);
    
    /**
     * @brief Get a variable's value by looking up the scope chain.
     * @param name Variable name to look up.
     * @return The variable's value.
     * 
     * Searches current environment, then enclosing environments
     * up to the global scope. Throws if not found.
     */
    Value get(const std::string& name);
    
    /**
     * @brief Assign a new value to an existing variable.
     * @param name Variable name.
     * @param value New value.
     * 
     * Searches the scope chain for the variable's definition,
     * then updates it in place. Throws if not found.
     */
    void assign(const std::string& name, const Value& value);
};

}
