/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * For full license text, see LICENSE file in the root directory.
 */

#include "types/string_error.h"
#include <sstream>
#include <algorithm>
#include <vector>

namespace neutron {

StringError::StringError(StringErrorCategory category, const std::string& message)
    : std::runtime_error(generateMessage(category, "", message)), 
      category_(category), method_name_(""), context_("") {
}

StringError::StringError(StringErrorCategory category, const std::string& method_name, 
                        const std::string& message)
    : std::runtime_error(generateMessage(category, method_name, message)), 
      category_(category), method_name_(method_name), context_("") {
}

StringError::StringError(StringErrorCategory category, const std::string& method_name,
                        const std::string& message, const std::string& context)
    : std::runtime_error(generateMessage(category, method_name, message, context)), 
      category_(category), method_name_(method_name), context_(context) {
}

std::string StringError::getUserMessage() const {
    std::ostringstream oss;
    
    switch (category_) {
        case StringErrorCategory::INDEX_OUT_OF_BOUNDS:
            oss << "String index out of range";
            break;
        case StringErrorCategory::INVALID_ARGUMENT:
            oss << "Invalid argument";
            break;
        case StringErrorCategory::ENCODING_ERROR:
            oss << "Character encoding error";
            break;
        case StringErrorCategory::FORMAT_ERROR:
            oss << "String formatting error";
            break;
        case StringErrorCategory::MEMORY_ERROR:
            oss << "Memory allocation error";
            break;
        case StringErrorCategory::UNICODE_ERROR:
            oss << "Unicode processing error";
            break;
        case StringErrorCategory::SLICE_ERROR:
            oss << "String slicing error";
            break;
        case StringErrorCategory::SEARCH_ERROR:
            oss << "String search error";
            break;
    }
    
    if (!method_name_.empty()) {
        oss << " in method '" << method_name_ << "'";
    }
    
    return oss.str();
}

StringError StringError::indexOutOfBounds(const std::string& method, int index, size_t length) {
    std::ostringstream oss;
    oss << "Index " << index << " is out of bounds for string of length " << length;
    return StringError(StringErrorCategory::INDEX_OUT_OF_BOUNDS, method, oss.str());
}

StringError StringError::invalidArgument(const std::string& method, const std::string& reason) {
    return StringError(StringErrorCategory::INVALID_ARGUMENT, method, reason);
}

StringError StringError::encodingError(const std::string& method, const std::string& encoding) {
    std::ostringstream oss;
    oss << "Unsupported or invalid encoding: '" << encoding << "'";
    return StringError(StringErrorCategory::ENCODING_ERROR, method, oss.str());
}

StringError StringError::formatError(const std::string& method, const std::string& format_str) {
    std::ostringstream oss;
    oss << "Invalid format string: '" << format_str << "'";
    return StringError(StringErrorCategory::FORMAT_ERROR, method, oss.str());
}

StringError StringError::memoryError(const std::string& method) {
    return StringError(StringErrorCategory::MEMORY_ERROR, method, "Failed to allocate memory for string operation");
}

StringError StringError::unicodeError(const std::string& method, const std::string& reason) {
    return StringError(StringErrorCategory::UNICODE_ERROR, method, reason);
}

StringError StringError::sliceError(const std::string& method, const std::string& reason) {
    return StringError(StringErrorCategory::SLICE_ERROR, method, reason);
}

StringError StringError::searchError(const std::string& method, const std::string& reason) {
    return StringError(StringErrorCategory::SEARCH_ERROR, method, reason);
}

std::string StringError::generateMessage(StringErrorCategory category, 
                                        const std::string& method_name,
                                        const std::string& message,
                                        const std::string& context) {
    std::ostringstream oss;
    
    // Add category prefix
    switch (category) {
        case StringErrorCategory::INDEX_OUT_OF_BOUNDS:
            oss << "[IndexError] ";
            break;
        case StringErrorCategory::INVALID_ARGUMENT:
            oss << "[ValueError] ";
            break;
        case StringErrorCategory::ENCODING_ERROR:
            oss << "[UnicodeError] ";
            break;
        case StringErrorCategory::FORMAT_ERROR:
            oss << "[FormatError] ";
            break;
        case StringErrorCategory::MEMORY_ERROR:
            oss << "[MemoryError] ";
            break;
        case StringErrorCategory::UNICODE_ERROR:
            oss << "[UnicodeError] ";
            break;
        case StringErrorCategory::SLICE_ERROR:
            oss << "[SliceError] ";
            break;
        case StringErrorCategory::SEARCH_ERROR:
            oss << "[SearchError] ";
            break;
    }
    
    // Add method name if provided
    if (!method_name.empty()) {
        oss << "string." << method_name << "(): ";
    }
    
    // Add main message
    oss << message;
    
    // Add context if provided
    if (!context.empty()) {
        oss << " (" << context << ")";
    }
    
    return oss.str();
}

// StringValidator implementation

void StringValidator::validateIndex(const std::string& method, int index, size_t length) {
    // Handle negative indices
    int normalized_index = index;
    if (index < 0) {
        normalized_index = static_cast<int>(length) + index;
    }
    
    if (normalized_index < 0 || normalized_index >= static_cast<int>(length)) {
        throw StringError::indexOutOfBounds(method, index, length);
    }
}

void StringValidator::validateSlice(const std::string& method, int start, int end, int step, size_t length) {
    if (step == 0) {
        throw StringError::sliceError(method, "Step cannot be zero");
    }
    
    // Additional slice validation can be added here
    // For now, we rely on the slice implementation to handle edge cases
}

void StringValidator::validateNotNull(const std::string& method, const void* ptr, const std::string& param_name) {
    if (ptr == nullptr) {
        throw StringError::invalidArgument(method, param_name + " cannot be null");
    }
}

void StringValidator::validateStringSize(const std::string& method, size_t size) {
    // Check for reasonable string size limits (1GB)
    const size_t MAX_STRING_SIZE = 1024 * 1024 * 1024;
    if (size > MAX_STRING_SIZE) {
        throw StringError::memoryError(method);
    }
}

void StringValidator::validateEncoding(const std::string& method, const std::string& encoding) {
    // List of supported encodings
    static const std::vector<std::string> supported_encodings = {
        "utf-8", "UTF-8", "ascii", "ASCII", "latin-1", "LATIN-1", "iso-8859-1", "ISO-8859-1"
    };
    
    auto it = std::find(supported_encodings.begin(), supported_encodings.end(), encoding);
    if (it == supported_encodings.end()) {
        throw StringError::encodingError(method, encoding);
    }
}

size_t StringValidator::normalizeIndex(const std::string& method, int index, size_t length) {
    validateIndex(method, index, length);
    
    if (index < 0) {
        return length + index;
    }
    return static_cast<size_t>(index);
}

void StringValidator::normalizeSlice(const std::string& method, int& start, int& end, int step, size_t length) {
    validateSlice(method, start, end, step, length);
    
    int len = static_cast<int>(length);
    
    // Normalize negative indices
    if (start < 0) start = std::max(0, len + start);
    if (end < 0) end = std::max(0, len + end);
    
    // Clamp to valid range
    start = std::max(0, std::min(start, len));
    end = std::max(0, std::min(end, len));
    
    // Handle reverse slicing
    if (step < 0) {
        if (start >= len) start = len - 1;
        if (end >= len) end = len - 1;
    }
}

} // namespace neutron