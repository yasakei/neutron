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
    } else if (codepoint == ' ') {
        return UnicodeCategory::SEPARATOR_SPACE;
    } else if (codepoint == '\t' || codepoint == '\n' || codepoint == '\r') {
        // \t, \n, \r are control characters per Unicode
        return UnicodeCategory::OTHER_CONTROL;
    }
    
    return UnicodeCategory::OTHER_NOT_ASSIGNED;
}

bool UnicodeHandler::isCategory(uint32_t codepoint, UnicodeCategory category) {
    return getCategory(codepoint) == category;
}

std::vector<uint32_t> UnicodeHandler::utf8ToCodepoints(const std::string& str) {
    std::vector<uint32_t> codepoints;
    
    for (size_t i = 0; i < str.length(); ) {
        unsigned char c = str[i];
        
        if (c < 0x80) {  // ASCII (0xxxxxxx)
            codepoints.push_back(static_cast<uint32_t>(c));
            i++;
        } else if ((c & 0xE0) == 0xC0) {  // 2-byte sequence (110xxxxx 10xxxxxx)
            if (i + 1 < str.length()) {
                unsigned char c2 = str[i + 1];
                if ((c2 & 0xC0) == 0x80) {
                    uint32_t codepoint = ((c & 0x1F) << 6) | (c2 & 0x3F);
                    codepoints.push_back(codepoint);
                    i += 2;
                } else {
                    codepoints.push_back(0xFFFD);  // Invalid sequence
                    i++;
                }
            } else {
                codepoints.push_back(0xFFFD);  // Incomplete sequence
                i++;
            }
        } else if ((c & 0xF0) == 0xE0) {  // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
            if (i + 2 < str.length()) {
                unsigned char c2 = str[i + 1];
                unsigned char c3 = str[i + 2];
                if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
                    uint32_t codepoint = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                    codepoints.push_back(codepoint);
                    i += 3;
                } else {
                    codepoints.push_back(0xFFFD);  // Invalid sequence
                    i++;
                }
            } else {
                codepoints.push_back(0xFFFD);  // Incomplete sequence
                i++;
            }
        } else if ((c & 0xF8) == 0xF0) {  // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
            if (i + 3 < str.length()) {
                unsigned char c2 = str[i + 1];
                unsigned char c3 = str[i + 2];
                unsigned char c4 = str[i + 3];
                if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80 && (c4 & 0xC0) == 0x80) {
                    uint32_t codepoint = ((c & 0x07) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
                    codepoints.push_back(codepoint);
                    i += 4;
                } else {
                    codepoints.push_back(0xFFFD);  // Invalid sequence
                    i++;
                }
            } else {
                codepoints.push_back(0xFFFD);  // Incomplete sequence
                i++;
            }
        } else {
            codepoints.push_back(0xFFFD);  // Invalid start byte
            i++;
        }
    }
    
    return codepoints;
}

std::string UnicodeHandler::codepointsToUtf8(const std::vector<uint32_t>& codepoints) {
    std::string result;
    
    for (uint32_t cp : codepoints) {
        if (cp < 0x80) {  // ASCII (0xxxxxxx)
            result += static_cast<char>(cp);
        } else if (cp < 0x800) {  // 2-byte sequence (110xxxxx 10xxxxxx)
            result += static_cast<char>(0xC0 | (cp >> 6));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {  // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
            result += static_cast<char>(0xE0 | (cp >> 12));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x110000) {  // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
            result += static_cast<char>(0xF0 | (cp >> 18));
            result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            // Invalid codepoint, use replacement character
            result += "\xEF\xBF\xBD";  // UTF-8 encoding of U+FFFD
        }
    }
    
    return result;
}

uint32_t UnicodeHandler::toUpper(uint32_t codepoint) {
    // Handle ASCII first
    if (codepoint >= 'a' && codepoint <= 'z') {
        return codepoint - 'a' + 'A';
    }
    
    // Handle common Unicode characters
    switch (codepoint) {
        // Latin-1 Supplement (U+00C0-U+00FF)
        case 0x00E0: return 0x00C0; // à → À
        case 0x00E1: return 0x00C1; // á → Á
        case 0x00E2: return 0x00C2; // â → Â
        case 0x00E3: return 0x00C3; // ã → Ã
        case 0x00E4: return 0x00C4; // ä → Ä
        case 0x00E5: return 0x00C5; // å → Å
        case 0x00E6: return 0x00C6; // æ → Æ
        case 0x00E7: return 0x00C7; // ç → Ç
        case 0x00E8: return 0x00C8; // è → È
        case 0x00E9: return 0x00C9; // é → É
        case 0x00EA: return 0x00CA; // ê → Ê
        case 0x00EB: return 0x00CB; // ë → Ë
        case 0x00EC: return 0x00CC; // ì → Ì
        case 0x00ED: return 0x00CD; // í → Í
        case 0x00EE: return 0x00CE; // î → Î
        case 0x00EF: return 0x00CF; // ï → Ï
        case 0x00F0: return 0x00D0; // ð → Ð
        case 0x00F1: return 0x00D1; // ñ → Ñ
        case 0x00F2: return 0x00D2; // ò → Ò
        case 0x00F3: return 0x00D3; // ó → Ó
        case 0x00F4: return 0x00D4; // ô → Ô
        case 0x00F5: return 0x00D5; // õ → Õ
        case 0x00F6: return 0x00D6; // ö → Ö
        case 0x00F8: return 0x00D8; // ø → Ø
        case 0x00F9: return 0x00D9; // ù → Ù
        case 0x00FA: return 0x00DA; // ú → Ú
        case 0x00FB: return 0x00DB; // û → Û
        case 0x00FC: return 0x00DC; // ü → Ü
        case 0x00FD: return 0x00DD; // ý → Ý
        case 0x00FE: return 0x00DE; // þ → Þ
        case 0x00FF: return 0x0178; // ÿ → Ÿ
        
        default:
            return codepoint; // No uppercase mapping
    }
}

uint32_t UnicodeHandler::toLower(uint32_t codepoint) {
    // Handle ASCII first
    if (codepoint >= 'A' && codepoint <= 'Z') {
        return codepoint - 'A' + 'a';
    }
    
    // Handle common Unicode characters
    switch (codepoint) {
        // Latin-1 Supplement (U+00C0-U+00FF)
        case 0x00C0: return 0x00E0; // À → à
        case 0x00C1: return 0x00E1; // Á → á
        case 0x00C2: return 0x00E2; // Â → â
        case 0x00C3: return 0x00E3; // Ã → ã
        case 0x00C4: return 0x00E4; // Ä → ä
        case 0x00C5: return 0x00E5; // Å → å
        case 0x00C6: return 0x00E6; // Æ → æ
        case 0x00C7: return 0x00E7; // Ç → ç
        case 0x00C8: return 0x00E8; // È → è
        case 0x00C9: return 0x00E9; // É → é
        case 0x00CA: return 0x00EA; // Ê → ê
        case 0x00CB: return 0x00EB; // Ë → ë
        case 0x00CC: return 0x00EC; // Ì → ì
        case 0x00CD: return 0x00ED; // Í → í
        case 0x00CE: return 0x00EE; // Î → î
        case 0x00CF: return 0x00EF; // Ï → ï
        case 0x00D0: return 0x00F0; // Ð → ð
        case 0x00D1: return 0x00F1; // Ñ → ñ
        case 0x00D2: return 0x00F2; // Ò → ò
        case 0x00D3: return 0x00F3; // Ó → ó
        case 0x00D4: return 0x00F4; // Ô → ô
        case 0x00D5: return 0x00F5; // Õ → õ
        case 0x00D6: return 0x00F6; // Ö → ö
        case 0x00D8: return 0x00F8; // Ø → ø
        case 0x00D9: return 0x00F9; // Ù → ù
        case 0x00DA: return 0x00FA; // Ú → ú
        case 0x00DB: return 0x00FB; // Û → û
        case 0x00DC: return 0x00FC; // Ü → ü
        case 0x00DD: return 0x00FD; // Ý → ý
        case 0x00DE: return 0x00FE; // Þ → þ
        case 0x0178: return 0x00FF; // Ÿ → ÿ
        
        default:
            return codepoint; // No lowercase mapping
    }
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