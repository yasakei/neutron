/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Public License 1.0.
 * For full license text, see LICENSE file in the root directory.
 */

#include "types/unicode_handler.h"
#include <algorithm>
#include <cctype>
#include <locale>
#include <codecvt>
#include <mutex>

namespace neutron {

// Static member initialization
std::unordered_map<uint32_t, UnicodeCategory> UnicodeHandler::categoryMap;
std::unordered_map<uint32_t, uint32_t> UnicodeHandler::uppercaseMap;
std::unordered_map<uint32_t, uint32_t> UnicodeHandler::lowercaseMap;
std::unordered_map<uint32_t, uint32_t> UnicodeHandler::titlecaseMap;
bool UnicodeHandler::tablesInitialized = false;

void UnicodeHandler::initializeTables() {
    if (tablesInitialized) return;
    
    // Basic ASCII character mappings (simplified implementation)
    // In a full implementation, this would load from Unicode database
    
    // Uppercase mappings
    for (uint32_t c = 'a'; c <= 'z'; ++c) {
        uppercaseMap[c] = c - 'a' + 'A';
        lowercaseMap[c - 'a' + 'A'] = c;
        titlecaseMap[c] = c - 'a' + 'A';
    }
    
    // Category mappings for ASCII
    for (uint32_t c = 'A'; c <= 'Z'; ++c) {
        categoryMap[c] = UnicodeCategory::LETTER_UPPERCASE;
    }
    for (uint32_t c = 'a'; c <= 'z'; ++c) {
        categoryMap[c] = UnicodeCategory::LETTER_LOWERCASE;
    }
    for (uint32_t c = '0'; c <= '9'; ++c) {
        categoryMap[c] = UnicodeCategory::NUMBER_DECIMAL_DIGIT;
    }
    
    // Whitespace characters
    categoryMap[' '] = UnicodeCategory::SEPARATOR_SPACE;
    categoryMap['\t'] = UnicodeCategory::OTHER_CONTROL;
    categoryMap['\n'] = UnicodeCategory::OTHER_CONTROL;
    categoryMap['\r'] = UnicodeCategory::OTHER_CONTROL;
    
    tablesInitialized = true;
}

std::string UnicodeHandler::normalize(const std::string& str, NormalizationForm form) {
    // Completely avoid static initialization - just return the string
    // In a production implementation, this would use ICU or similar
    (void)form; // Suppress unused parameter warning
    return str;
}

std::string UnicodeHandler::casefold(const std::string& str) {
    // Avoid static initialization - simple case folding
    std::string result;
    result.reserve(str.length());
    
    for (char c : str) {
        if (c >= 'A' && c <= 'Z') {
            result += (c - 'A' + 'a');
        } else {
            result += c;
        }
    }
    
    return result;
}

UnicodeCategory UnicodeHandler::getCategory(uint32_t codepoint) {
    // Avoid static initialization - simple category detection
    if (codepoint >= 'A' && codepoint <= 'Z') {
        return UnicodeCategory::LETTER_UPPERCASE;
    } else if (codepoint >= 'a' && codepoint <= 'z') {
        return UnicodeCategory::LETTER_LOWERCASE;
    } else if (codepoint >= '0' && codepoint <= '9') {
        return UnicodeCategory::NUMBER_DECIMAL_DIGIT;
    } else if (codepoint == ' ' || codepoint == '\t' || codepoint == '\n' || codepoint == '\r') {
        return UnicodeCategory::SEPARATOR_SPACE;
    }
    
    return UnicodeCategory::OTHER_NOT_ASSIGNED;
}

bool UnicodeHandler::isCategory(uint32_t codepoint, UnicodeCategory category) {
    return getCategory(codepoint) == category;
}

std::vector<uint32_t> UnicodeHandler::utf8ToCodepoints(const std::string& str) {
    std::vector<uint32_t> codepoints;
    
    // Simplified UTF-8 decoding (handles ASCII only for now)
    // Full implementation would properly decode multi-byte UTF-8 sequences
    for (unsigned char c : str) {
        if (c < 0x80) {  // ASCII
            codepoints.push_back(static_cast<uint32_t>(c));
        } else {
            // For now, treat non-ASCII as replacement character
            codepoints.push_back(0xFFFD);  // Unicode replacement character
        }
    }
    
    return codepoints;
}

std::string UnicodeHandler::codepointsToUtf8(const std::vector<uint32_t>& codepoints) {
    std::string result;
    
    // Simplified UTF-8 encoding (handles ASCII only for now)
    for (uint32_t cp : codepoints) {
        if (cp < 0x80) {  // ASCII
            result += static_cast<char>(cp);
        } else {
            // For now, use replacement character for non-ASCII
            result += '?';
        }
    }
    
    return result;
}

uint32_t UnicodeHandler::toUpper(uint32_t codepoint) {
    // Avoid static initialization - simple uppercase conversion
    if (codepoint >= 'a' && codepoint <= 'z') {
        return codepoint - 'a' + 'A';
    }
    return codepoint;
}

uint32_t UnicodeHandler::toLower(uint32_t codepoint) {
    // Avoid static initialization - simple lowercase conversion
    if (codepoint >= 'A' && codepoint <= 'Z') {
        return codepoint - 'A' + 'a';
    }
    return codepoint;
}

uint32_t UnicodeHandler::toTitle(uint32_t codepoint) {
    // Avoid static initialization - simple titlecase conversion
    if (codepoint >= 'a' && codepoint <= 'z') {
        return codepoint - 'a' + 'A';
    }
    return codepoint;
}

bool UnicodeHandler::isWhitespace(uint32_t codepoint) {
    // Avoid static initialization - simple whitespace check
    return codepoint == ' ' || codepoint == '\t' || codepoint == '\n' || 
           codepoint == '\r' || codepoint == '\f' || codepoint == '\v';
}

bool UnicodeHandler::isAlphanumeric(uint32_t codepoint) {
    return isAlphabetic(codepoint) || isNumeric(codepoint);
}

bool UnicodeHandler::isAlphabetic(uint32_t codepoint) {
    UnicodeCategory cat = getCategory(codepoint);
    return cat == UnicodeCategory::LETTER_UPPERCASE ||
           cat == UnicodeCategory::LETTER_LOWERCASE ||
           cat == UnicodeCategory::LETTER_TITLECASE ||
           cat == UnicodeCategory::LETTER_MODIFIER ||
           cat == UnicodeCategory::LETTER_OTHER;
}

bool UnicodeHandler::isNumeric(uint32_t codepoint) {
    UnicodeCategory cat = getCategory(codepoint);
    return cat == UnicodeCategory::NUMBER_DECIMAL_DIGIT ||
           cat == UnicodeCategory::NUMBER_LETTER ||
           cat == UnicodeCategory::NUMBER_OTHER;
}

} // namespace neutron