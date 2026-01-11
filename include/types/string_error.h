/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Public License 1.0.
 * For full license text, see LICENSE file in the root directory.
 */

#pragma once

#include <stdexcept>
#include <string>

namespace neutron {

/**
 * Categories of string-related errors
 */
enum class StringErrorCategory {
    INDEX_OUT_OF_BOUNDS,    // String index out of range
    INVALID_ARGUMENT,       // Invalid argument passed to string method
    ENCODING_ERROR,         // Character encoding/decoding error
    FORMAT_ERROR,           // String formatting error
    MEMORY_ERROR,           // Memory allocation error
    UNICODE_ERROR,          // Unicode processing error
    SLICE_ERROR,            // String slicing error
    SEARCH_ERROR            // String search operation error
};

/**
 * String-specific error class with categorization and detailed messages
 */
class StringError : public std::runtime_error {
private:
    StringErrorCategory category_;
    std::string method_name_;
    std::string context_;

public:
    /**
     * Construct a StringError with category and message
     */
    StringError(StringErrorCategory category, const std::string& message);
    
    /**
     * Construct a StringError with category, method name, and message
     */
    StringError(StringErrorCategory category, const std::string& method_name, 
                const std::string& message);
    
    /**
     * Construct a StringError with full context
     */
    StringError(StringErrorCategory category, const std::string& method_name,
                const std::string& message, const std::string& context);
    
    /**
     * Get the error category
     */
    StringErrorCategory getCategory() const { return category_; }
    
    /**
     * Get the method name where error occurred
     */
    const std::string& getMethodName() const { return method_name_; }
    
    /**
     * Get additional context information
     */
    const std::string& getContext() const { return context_; }
    
    /**
     * Get a user-friendly error message
     */
    std::string getUserMessage() const;
    
    /**
     * Static factory methods for common error types
     */
    static StringError indexOutOfBounds(const std::string& method, int index, size_t length);
    static StringError invalidArgument(const std::string& method, const std::string& reason);
    static StringError encodingError(const std::string& method, const std::string& encoding);
    static StringError formatError(const std::string& method, const std::string& format_str);
    static StringError memoryError(const std::string& method);
    static StringError unicodeError(const std::string& method, const std::string& reason);
    static StringError sliceError(const std::string& method, const std::string& reason);
    static StringError searchError(const std::string& method, const std::string& reason);

private:
    /**
     * Generate detailed error message
     */
    static std::string generateMessage(StringErrorCategory category, 
                                     const std::string& method_name,
                                     const std::string& message,
                                     const std::string& context = "");
};

/**
 * Utility functions for string validation and bounds checking
 */
class StringValidator {
public:
    /**
     * Validate string index and throw appropriate error if invalid
     */
    static void validateIndex(const std::string& method, int index, size_t length);
    
    /**
     * Validate string slice parameters
     */
    static void validateSlice(const std::string& method, int start, int end, int step, size_t length);
    
    /**
     * Validate string method arguments
     */
    static void validateNotNull(const std::string& method, const void* ptr, const std::string& param_name);
    
    /**
     * Validate string size limits
     */
    static void validateStringSize(const std::string& method, size_t size);
    
    /**
     * Validate encoding name
     */
    static void validateEncoding(const std::string& method, const std::string& encoding);
    
    /**
     * Convert negative index to positive, with validation
     */
    static size_t normalizeIndex(const std::string& method, int index, size_t length);
    
    /**
     * Normalize slice parameters with validation
     */
    static void normalizeSlice(const std::string& method, int& start, int& end, int step, size_t length);
};

} // namespace neutron