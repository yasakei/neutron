#pragma once

#include "types/string_method_handler.h"
#include <unordered_map>
#include <memory>
#include <string>

namespace neutron {

/**
 * Categories of string methods for organization and potential optimization
 */
enum class StringMethodCategory {
    BASIC,           // length, contains, split, etc.
    SEARCH,          // find, index, rfind, rindex
    CASE,            // upper, lower, capitalize, etc.
    CLASSIFICATION,  // isalnum, isdigit, etc.
    FORMATTING,      // format, center, ljust, etc.
    UNICODE,         // encode, casefold, etc.
    SEQUENCE         // slice operations, iteration
};

/**
 * Registry for managing all string methods
 * Provides centralized method lookup and registration
 */
class StringMethodRegistry {
private:
    std::unordered_map<std::string, std::unique_ptr<StringMethodHandler>> methods_;
    std::unordered_map<std::string, StringMethodCategory> categories_;
    
public:
    /**
     * Register a new string method handler
     * @param name The method name
     * @param handler The handler implementation
     * @param category The method category
     */
    void registerMethod(const std::string& name, 
                       std::unique_ptr<StringMethodHandler> handler,
                       StringMethodCategory category);
    
    /**
     * Get a method handler by name
     * @param name The method name
     * @return Pointer to handler, or nullptr if not found
     */
    StringMethodHandler* getMethod(const std::string& name);
    
    /**
     * Check if a method exists
     * @param name The method name
     * @return true if method exists, false otherwise
     */
    bool hasMethod(const std::string& name) const;
    
    /**
     * Get the category of a method
     * @param name The method name
     * @return The method category, or BASIC if not found
     */
    StringMethodCategory getMethodCategory(const std::string& name) const;
    
    /**
     * Initialize all basic string methods (existing ones)
     * This maintains backward compatibility
     */
    void initializeBasicMethods();
    
    /**
     * Get all registered method names
     * @return Vector of method names
     */
    std::vector<std::string> getMethodNames() const;
    
    /**
     * Get singleton instance
     * @return Reference to the global registry
     */
    static StringMethodRegistry& getInstance();
};

}