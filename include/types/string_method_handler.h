#pragma once

#include "types/value.h"
#include <vector>
#include <string>

namespace neutron {

class VM; // Forward declaration

/**
 * Base interface for string method handlers
 * Provides consistent execution interface for all string methods
 */
class StringMethodHandler {
public:
    virtual ~StringMethodHandler() = default;
    
    /**
     * Execute the string method with given arguments
     * @param vm The VM instance for error reporting and memory allocation
     * @param str The string value to operate on
     * @param args The arguments passed to the method
     * @return The result value of the method execution
     */
    virtual Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) = 0;
    
    /**
     * Get the expected number of arguments for this method
     * @return The arity of the method, or -1 for variable arguments
     */
    virtual int getArity() const = 0;
    
    /**
     * Validate the arguments before execution
     * @param args The arguments to validate
     * @return true if arguments are valid, false otherwise
     */
    virtual bool validateArgs(const std::vector<Value>& args) const = 0;
    
    /**
     * Get a description of this method for error messages
     * @return A string describing the method signature
     */
    virtual std::string getDescription() const = 0;
};

}