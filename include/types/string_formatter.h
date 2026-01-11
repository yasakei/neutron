/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Public License 1.0.
 * For full license text, see LICENSE file in the root directory.
 */

#pragma once

#include "types/value.h"
#include <string>
#include <vector>

namespace neutron {

/**
 * String formatter for Python-style string formatting
 */
class StringFormatter {
public:
    /**
     * Format a string using positional arguments
     * Example: "Hello {0}, you are {1} years old".format("Alice", 25)
     */
    static std::string format(const std::string& format_str, const std::vector<Value>& args);
    
    /**
     * Format a string using printf-style formatting
     * Example: "Hello %s, you are %d years old" % ["Alice", 25]
     */
    static std::string printf_format(const std::string& format_str, const std::vector<Value>& args);

private:
    /**
     * Parse format specification
     */
    struct FormatSpec {
        int index = -1;           // Argument index
        std::string fill = " ";   // Fill character
        char align = '<';         // Alignment (<, >, ^, =)
        char sign = '-';          // Sign (+, -, space)
        bool alternate = false;   // Alternate form (#)
        bool zero_pad = false;    // Zero padding (0)
        int width = -1;           // Minimum width
        int precision = -1;       // Precision
        char type = 's';          // Type specifier
    };
    
    /**
     * Parse a format field like {0:>10.2f}
     */
    static FormatSpec parseFormatSpec(const std::string& spec);
    
    /**
     * Apply format specification to a value
     */
    static std::string applyFormat(const Value& value, const FormatSpec& spec);
    
    /**
     * Parse printf-style format specifier like %d, %s, %.2f
     */
    static std::string parsePrintfSpec(const std::string& spec, const Value& value);
    
    /**
     * Convert value to string with type specifier
     */
    static std::string valueToString(const Value& value, char type, int precision = -1);
    
    /**
     * Apply alignment and padding
     */
    static std::string applyAlignment(const std::string& str, const FormatSpec& spec);
};

} // namespace neutron