/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, for both open source and commercial purposes.
 * 
 * Conditions:
 * 
 * 1. The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 * 
 * 2. Attribution is appreciated but NOT required.
 *    Suggested (optional) credit:
 *    "Built using Neutron Programming Language (c) yasakei"
 * 
 * 3. The name "Neutron" and the name of the copyright holder may not be used
 *    to endorse or promote products derived from this Software without prior
 *    written permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include "types/string_formatter.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

namespace neutron {

std::string StringFormatter::format(const std::string& format_str, const std::vector<Value>& args) {
    std::string result;
    result.reserve(format_str.length() * 2); // Reserve space for efficiency
    
    size_t pos = 0;
    int auto_index = 0;
    
    while (pos < format_str.length()) {
        size_t brace_start = format_str.find('{', pos);
        
        if (brace_start == std::string::npos) {
            // No more format fields, append rest of string
            result += format_str.substr(pos);
            break;
        }
        
        // Append text before the brace
        result += format_str.substr(pos, brace_start - pos);
        
        // Handle escaped braces
        if (brace_start + 1 < format_str.length() && format_str[brace_start + 1] == '{') {
            result += '{';
            pos = brace_start + 2;
            continue;
        }
        
        // Find closing brace
        size_t brace_end = format_str.find('}', brace_start);
        if (brace_end == std::string::npos) {
            throw std::runtime_error("Unmatched '{' in format string");
        }
        
        // Handle escaped closing braces
        if (brace_end + 1 < format_str.length() && format_str[brace_end + 1] == '}') {
            result += '}';
            pos = brace_end + 2;
            continue;
        }
        
        // Extract format specification
        std::string spec = format_str.substr(brace_start + 1, brace_end - brace_start - 1);
        FormatSpec format_spec = parseFormatSpec(spec);
        
        // Determine argument index
        int arg_index = format_spec.index;
        if (arg_index == -1) {
            arg_index = auto_index++;
        }
        
        // Validate argument index
        if (arg_index < 0 || arg_index >= static_cast<int>(args.size())) {
            throw std::runtime_error("Format argument index out of range");
        }
        
        // Apply formatting
        std::string formatted = applyFormat(args[arg_index], format_spec);
        result += formatted;
        
        pos = brace_end + 1;
    }
    
    return result;
}

std::string StringFormatter::printf_format(const std::string& format_str, const std::vector<Value>& args) {
    std::string result;
    result.reserve(format_str.length() * 2);
    
    size_t pos = 0;
    size_t arg_index = 0;
    
    while (pos < format_str.length()) {
        size_t percent_pos = format_str.find('%', pos);
        
        if (percent_pos == std::string::npos) {
            // No more format specifiers
            result += format_str.substr(pos);
            break;
        }
        
        // Append text before %
        result += format_str.substr(pos, percent_pos - pos);
        
        // Handle escaped %
        if (percent_pos + 1 < format_str.length() && format_str[percent_pos + 1] == '%') {
            result += '%';
            pos = percent_pos + 2;
            continue;
        }
        
        // Find end of format specifier
        size_t spec_end = percent_pos + 1;
        while (spec_end < format_str.length()) {
            char c = format_str[spec_end];
            if (c == 'd' || c == 'i' || c == 'o' || c == 'x' || c == 'X' ||
                c == 'f' || c == 'F' || c == 'e' || c == 'E' || c == 'g' || c == 'G' ||
                c == 'c' || c == 's') {
                spec_end++;
                break;
            }
            spec_end++;
        }
        
        if (arg_index >= args.size()) {
            throw std::runtime_error("Not enough arguments for format string");
        }
        
        // Extract and parse format specifier
        std::string spec = format_str.substr(percent_pos, spec_end - percent_pos);
        std::string formatted = parsePrintfSpec(spec, args[arg_index]);
        result += formatted;
        
        arg_index++;
        pos = spec_end;
    }
    
    return result;
}

StringFormatter::FormatSpec StringFormatter::parseFormatSpec(const std::string& spec) {
    FormatSpec format_spec;
    
    if (spec.empty()) {
        return format_spec;
    }
    
    size_t pos = 0;
    
    // Parse index (if present)
    if (std::isdigit(spec[0])) {
        size_t colon_pos = spec.find(':');
        std::string index_str = (colon_pos != std::string::npos) ? 
                               spec.substr(0, colon_pos) : spec;
        format_spec.index = std::stoi(index_str);
        pos = (colon_pos != std::string::npos) ? colon_pos + 1 : spec.length();
    } else if (spec[0] == ':') {
        pos = 1;
    }
    
    // For now, just handle basic type specifiers
    if (pos < spec.length()) {
        char last_char = spec.back();
        if (last_char == 's' || last_char == 'd' || last_char == 'f') {
            format_spec.type = last_char;
        }
    }
    
    return format_spec;
}

std::string StringFormatter::applyFormat(const Value& value, const FormatSpec& spec) {
    std::string str = valueToString(value, spec.type, spec.precision);
    return applyAlignment(str, spec);
}

std::string StringFormatter::parsePrintfSpec(const std::string& spec, const Value& value) {
    char type = spec.back();
    
    switch (type) {
        case 'd':
        case 'i':
            if (value.type == ValueType::NUMBER) {
                return std::to_string(static_cast<int>(value.as.number));
            }
            break;
        case 'f':
            if (value.type == ValueType::NUMBER) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(6) << value.as.number;
                return oss.str();
            }
            break;
        case 's':
            return value.toString();
        case 'c':
            if (value.type == ValueType::NUMBER) {
                return std::string(1, static_cast<char>(value.as.number));
            }
            break;
    }
    
    return value.toString();
}

std::string StringFormatter::valueToString(const Value& value, char type, int precision) {
    switch (type) {
        case 'd':
            if (value.type == ValueType::NUMBER) {
                return std::to_string(static_cast<int>(value.as.number));
            }
            break;
        case 'f':
            if (value.type == ValueType::NUMBER) {
                std::ostringstream oss;
                if (precision >= 0) {
                    oss << std::fixed << std::setprecision(precision);
                }
                oss << value.as.number;
                return oss.str();
            }
            break;
        case 's':
        default:
            return value.toString();
    }
    
    return value.toString();
}

std::string StringFormatter::applyAlignment(const std::string& str, const FormatSpec& spec) {
    if (spec.width <= 0 || static_cast<int>(str.length()) >= spec.width) {
        return str;
    }
    
    int padding = spec.width - str.length();
    
    switch (spec.align) {
        case '<': // Left align
            return str + std::string(padding, spec.fill[0]);
        case '>': // Right align
            return std::string(padding, spec.fill[0]) + str;
        case '^': // Center align
            {
                int left_pad = padding / 2;
                int right_pad = padding - left_pad;
                return std::string(left_pad, spec.fill[0]) + str + std::string(right_pad, spec.fill[0]);
            }
        default:
            return str;
    }
}

} // namespace neutron