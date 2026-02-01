/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * For full license text, see LICENSE file in the root directory.
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace neutron {

/**
 * Unicode normalization forms
 */
enum class NormalizationForm {
    NFC,  // Canonical Decomposition, followed by Canonical Composition
    NFD,  // Canonical Decomposition
    NFKC, // Compatibility Decomposition, followed by Canonical Composition
    NFKD  // Compatibility Decomposition
};

/**
 * Unicode character categories
 */
enum class UnicodeCategory {
    LETTER_UPPERCASE,
    LETTER_LOWERCASE,
    LETTER_TITLECASE,
    LETTER_MODIFIER,
    LETTER_OTHER,
    MARK_NONSPACING,
    MARK_SPACING_COMBINING,
    MARK_ENCLOSING,
    NUMBER_DECIMAL_DIGIT,
    NUMBER_LETTER,
    NUMBER_OTHER,
    PUNCTUATION_CONNECTOR,
    PUNCTUATION_DASH,
    PUNCTUATION_OPEN,
    PUNCTUATION_CLOSE,
    PUNCTUATION_INITIAL_QUOTE,
    PUNCTUATION_FINAL_QUOTE,
    PUNCTUATION_OTHER,
    SYMBOL_MATH,
    SYMBOL_CURRENCY,
    SYMBOL_MODIFIER,
    SYMBOL_OTHER,
    SEPARATOR_SPACE,
    SEPARATOR_LINE,
    SEPARATOR_PARAGRAPH,
    OTHER_CONTROL,
    OTHER_FORMAT,
    OTHER_SURROGATE,
    OTHER_PRIVATE_USE,
    OTHER_NOT_ASSIGNED
};

/**
 * Unicode handler for string operations
 */
class UnicodeHandler {
public:
    /**
     * Normalize a UTF-8 string using the specified normalization form
     */
    static std::string normalize(const std::string& str, NormalizationForm form);
    
    /**
     * Perform Unicode case folding (more comprehensive than lowercase)
     */
    static std::string casefold(const std::string& str);
    
    /**
     * Get the Unicode category of a character
     */
    static UnicodeCategory getCategory(uint32_t codepoint);
    
    /**
     * Check if a character is in a specific category
     */
    static bool isCategory(uint32_t codepoint, UnicodeCategory category);
    
    /**
     * Convert UTF-8 string to UTF-32 codepoints
     */
    static std::vector<uint32_t> utf8ToCodepoints(const std::string& str);
    
    /**
     * Convert UTF-32 codepoints to UTF-8 string
     */
    static std::string codepointsToUtf8(const std::vector<uint32_t>& codepoints);
    
    /**
     * Get the uppercase version of a codepoint
     */
    static uint32_t toUpper(uint32_t codepoint);
    
    /**
     * Get the lowercase version of a codepoint
     */
    static uint32_t toLower(uint32_t codepoint);
    
    /**
     * Get the titlecase version of a codepoint
     */
    static uint32_t toTitle(uint32_t codepoint);
    
    /**
     * Check if a codepoint is whitespace
     */
    static bool isWhitespace(uint32_t codepoint);
    
    /**
     * Check if a codepoint is alphanumeric
     */
    static bool isAlphanumeric(uint32_t codepoint);
    
    /**
     * Check if a codepoint is alphabetic
     */
    static bool isAlphabetic(uint32_t codepoint);
    
    /**
     * Check if a codepoint is numeric
     */
    static bool isNumeric(uint32_t codepoint);

private:
    // Unicode property lookup tables (simplified for basic implementation)
    static std::unordered_map<uint32_t, UnicodeCategory> categoryMap;
    static std::unordered_map<uint32_t, uint32_t> uppercaseMap;
    static std::unordered_map<uint32_t, uint32_t> lowercaseMap;
    static std::unordered_map<uint32_t, uint32_t> titlecaseMap;
    
    // Initialize Unicode property tables
    static void initializeTables();
    static bool tablesInitialized;
};

} // namespace neutron