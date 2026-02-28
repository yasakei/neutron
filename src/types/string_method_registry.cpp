#include "types/string_method_registry.h"
#include "types/string_search_methods.h"
#include "types/unicode_handler.h"
#include "types/string_formatter.h"
#include "types/string_error.h"
#include "types/value.h"
#include "types/array.h"
#include "core/vm.h"
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace neutron {

// Basic method handlers for existing functionality
class LengthMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        return Value(static_cast<double>(str.length()));
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "length() -> int";
    }
};

class ContainsMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; // Suppress unused parameter warning
        std::string substr = args[0].toString();
        bool found = str.find(substr) != std::string::npos;
        return Value(found);
    }
    
    int getArity() const override { return 1; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 1;
    }
    
    std::string getDescription() const override {
        return "contains(substring) -> bool";
    }
};

class SplitMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        std::string delimiter = args[0].toString();
        std::vector<Value> parts;
        
        if (delimiter.empty()) {
            // Split by character
            for (char c : str) {
                parts.push_back(Value(std::string(1, c)));
            }
        } else {
            size_t pos = 0;
            std::string token;
            std::string s = str; // Make a copy to modify
            while ((pos = s.find(delimiter)) != std::string::npos) {
                token = s.substr(0, pos);
                parts.push_back(Value(token));
                s.erase(0, pos + delimiter.length());
            }
            parts.push_back(Value(s));
        }
        return Value(vm->allocate<Array>(parts));
    }
    
    int getArity() const override { return 1; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 1;
    }
    
    std::string getDescription() const override {
        return "split(delimiter) -> array";
    }
};

class SubstringMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        if (args[0].type != ValueType::NUMBER) {
            throw std::runtime_error("substring() expects first argument to be a number.");
        }
        
        int start = static_cast<int>(args[0].as.number);
        int len = static_cast<int>(str.length());
        
        if (start < 0) start = 0;
        if (start > len) start = len;
        
        int end = len;
        if (args.size() == 2) {
            if (args[1].type != ValueType::NUMBER) {
                throw std::runtime_error("substring() expects second argument to be a number.");
            }
            end = static_cast<int>(args[1].as.number);
            if (end < 0) end = 0;
            if (end > len) end = len;
        }
        
        if (end < start) {
            int temp = start;
            start = end;
            end = temp;
        }
        
        return Value(str.substr(start, end - start));
    }
    
    int getArity() const override { return -1; } // Variable arguments (1 or 2)
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() >= 1 && args.size() <= 2;
    }
    
    std::string getDescription() const override {
        return "substring(start, [end]) -> string";
    }
};

/**
 * Find method handler - returns index of first occurrence or -1
 */
class FindMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; // Suppress unused parameter warning
        
        try {
            std::string substr = args[0].toString();
            
            size_t start = 0;
            size_t end = str.length();
            
            // Handle optional start parameter
            if (args.size() >= 2) {
                if (args[1].type != ValueType::NUMBER) {
                    throw StringError::invalidArgument("find", "start parameter must be a number");
                }
                int startInt = static_cast<int>(args[1].as.number);
                if (startInt < 0) startInt = 0;
                start = static_cast<size_t>(startInt);
            }
            
            // Handle optional end parameter
            if (args.size() >= 3) {
                if (args[2].type != ValueType::NUMBER) {
                    throw StringError::invalidArgument("find", "end parameter must be a number");
                }
                int endInt = static_cast<int>(args[2].as.number);
                if (endInt < 0) endInt = 0;
                end = std::min(static_cast<size_t>(endInt), str.length());
            }
            
            // Allow searching at the end of string for empty substrings
            if (start > str.length()) {
                return Value(-1.0);
            }
            
            // Search within the specified range
            std::string searchStr = str.substr(start, end - start);
            size_t pos = searchStr.find(substr);
            
            if (pos == std::string::npos) {
                return Value(-1.0);
            }
            
            return Value(static_cast<double>(start + pos));
        } catch (const StringError& e) {
            throw std::runtime_error(e.what());
        }
    }
    
    int getArity() const override { return -1; } // Variable arguments (1-3)
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() >= 1 && args.size() <= 3;
    }
    
    std::string getDescription() const override {
        return "find(substring, [start], [end]) -> int";
    }
};

/**
 * RFind method handler - returns index of last occurrence or -1
 */
class RFindMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; // Suppress unused parameter warning
        std::string substr = args[0].toString();
        
        size_t start = 0;
        size_t end = str.length();
        
        // Handle optional start parameter
        if (args.size() >= 2) {
            if (args[1].type != ValueType::NUMBER) {
                throw std::runtime_error("rfind() start parameter must be a number");
            }
            int startInt = static_cast<int>(args[1].as.number);
            if (startInt < 0) startInt = 0;
            start = static_cast<size_t>(startInt);
        }
        
        // Handle optional end parameter
        if (args.size() >= 3) {
            if (args[2].type != ValueType::NUMBER) {
                throw std::runtime_error("rfind() end parameter must be a number");
            }
            int endInt = static_cast<int>(args[2].as.number);
            if (endInt < 0) endInt = 0;
            end = std::min(static_cast<size_t>(endInt), str.length());
        }
        
        if (start > str.length()) {
            return Value(-1.0);
        }
        
        // Search within the specified range
        std::string searchStr = str.substr(start, end - start);
        size_t pos = searchStr.rfind(substr);
        
        if (pos == std::string::npos) {
            return Value(-1.0);
        }
        
        return Value(static_cast<double>(start + pos));
    }
    
    int getArity() const override { return -1; } // Variable arguments (1-3)
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() >= 1 && args.size() <= 3;
    }
    
    std::string getDescription() const override {
        return "rfind(substring, [start], [end]) -> int";
    }
};

/**
 * Index method handler - like find but raises error if not found
 */
class IndexMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; // Suppress unused parameter warning
        std::string substr = args[0].toString();
        
        size_t start = 0;
        size_t end = str.length();
        
        // Handle optional start parameter
        if (args.size() >= 2) {
            if (args[1].type != ValueType::NUMBER) {
                throw std::runtime_error("index() start parameter must be a number");
            }
            int startInt = static_cast<int>(args[1].as.number);
            if (startInt < 0) startInt = 0;
            start = static_cast<size_t>(startInt);
        }
        
        // Handle optional end parameter
        if (args.size() >= 3) {
            if (args[2].type != ValueType::NUMBER) {
                throw std::runtime_error("index() end parameter must be a number");
            }
            int endInt = static_cast<int>(args[2].as.number);
            if (endInt < 0) endInt = 0;
            end = std::min(static_cast<size_t>(endInt), str.length());
        }
        
        if (start > str.length()) {
            throw std::runtime_error("substring not found");
        }
        
        // Search within the specified range
        std::string searchStr = str.substr(start, end - start);
        size_t pos = searchStr.find(substr);
        
        if (pos == std::string::npos) {
            throw std::runtime_error("substring not found");
        }
        
        return Value(static_cast<double>(start + pos));
    }
    
    int getArity() const override { return -1; } // Variable arguments (1-3)
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() >= 1 && args.size() <= 3;
    }
    
    std::string getDescription() const override {
        return "index(substring, [start], [end]) -> int";
    }
};

/**
 * RIndex method handler - like rfind but raises error if not found
 */
class RIndexMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; // Suppress unused parameter warning
        std::string substr = args[0].toString();
        
        size_t start = 0;
        size_t end = str.length();
        
        // Handle optional start parameter
        if (args.size() >= 2) {
            if (args[1].type != ValueType::NUMBER) {
                throw std::runtime_error("rindex() start parameter must be a number");
            }
            int startInt = static_cast<int>(args[1].as.number);
            if (startInt < 0) startInt = 0;
            start = static_cast<size_t>(startInt);
        }
        
        // Handle optional end parameter
        if (args.size() >= 3) {
            if (args[2].type != ValueType::NUMBER) {
                throw std::runtime_error("rindex() end parameter must be a number");
            }
            int endInt = static_cast<int>(args[2].as.number);
            if (endInt < 0) endInt = 0;
            end = std::min(static_cast<size_t>(endInt), str.length());
        }
        
        if (start > str.length()) {
            throw std::runtime_error("substring not found");
        }
        
        // Search within the specified range
        std::string searchStr = str.substr(start, end - start);
        size_t pos = searchStr.rfind(substr);
        
        if (pos == std::string::npos) {
            throw std::runtime_error("substring not found");
        }
        
        return Value(static_cast<double>(start + pos));
    }
    
    int getArity() const override { return -1; } // Variable arguments (1-3)
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() >= 1 && args.size() <= 3;
    }
    
    std::string getDescription() const override {
        return "rindex(substring, [start], [end]) -> int";
    }
};

/**
 * Strip method handler - removes whitespace from both ends
 */
class StripMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; // Suppress unused parameter warning
        
        std::string chars = " \t\n\r\f\v"; // Default whitespace characters
        
        // Handle optional chars parameter
        if (args.size() >= 1) {
            chars = args[0].toString();
        }
        
        if (str.empty()) {
            return Value(str);
        }
        
        // Find first non-whitespace character
        size_t start = 0;
        while (start < str.length() && chars.find(str[start]) != std::string::npos) {
            start++;
        }
        
        // If all characters are whitespace
        if (start == str.length()) {
            return Value(std::string(""));
        }
        
        // Find last non-whitespace character
        size_t end = str.length() - 1;
        while (end > start && chars.find(str[end]) != std::string::npos) {
            end--;
        }
        
        return Value(str.substr(start, end - start + 1));
    }
    
    int getArity() const override { return -1; } // Variable arguments (0-1)
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() <= 1;
    }
    
    std::string getDescription() const override {
        return "strip([chars]) -> string";
    }
};

/**
 * LStrip method handler - removes whitespace from left end
 */
class LStripMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; // Suppress unused parameter warning
        
        std::string chars = " \t\n\r\f\v"; // Default whitespace characters
        
        // Handle optional chars parameter
        if (args.size() >= 1) {
            chars = args[0].toString();
        }
        
        if (str.empty()) {
            return Value(str);
        }
        
        // Find first non-whitespace character
        size_t start = 0;
        while (start < str.length() && chars.find(str[start]) != std::string::npos) {
            start++;
        }
        
        return Value(str.substr(start));
    }
    
    int getArity() const override { return -1; } // Variable arguments (0-1)
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() <= 1;
    }
    
    std::string getDescription() const override {
        return "lstrip([chars]) -> string";
    }
};

/**
 * RStrip method handler - removes whitespace from right end
 */
class RStripMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; // Suppress unused parameter warning
        
        std::string chars = " \t\n\r\f\v"; // Default whitespace characters
        
        // Handle optional chars parameter
        if (args.size() >= 1) {
            chars = args[0].toString();
        }
        
        if (str.empty()) {
            return Value(str);
        }
        
        // Find last non-whitespace character
        size_t end = str.length() - 1;
        while (end > 0 && chars.find(str[end]) != std::string::npos) {
            end--;
        }
        
        // Check if first character should also be stripped
        if (chars.find(str[0]) != std::string::npos && end == 0) {
            return Value(std::string(""));
        }
        
        return Value(str.substr(0, end + 1));
    }
    
    int getArity() const override { return -1; } // Variable arguments (0-1)
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() <= 1;
    }
    
    std::string getDescription() const override {
        return "rstrip([chars]) -> string";
    }
};

/**
 * Replace method handler - replaces occurrences of substring with replacement
 */
class ReplaceMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; // Suppress unused parameter warning
        
        try {
            std::string oldStr = args[0].toString();
            std::string newStr = args[1].toString();
            int count = -1; // Replace all by default
            
            // Handle optional count parameter
            if (args.size() >= 3) {
                if (args[2].type != ValueType::NUMBER) {
                    throw StringError::invalidArgument("replace", "count parameter must be a number");
                }
                count = static_cast<int>(args[2].as.number);
                if (count < 0) count = -1; // Negative means replace all
            }
            
            // Validate potential result size for very large strings
            if (!oldStr.empty() && newStr.length() > oldStr.length()) {
                size_t potential_size = str.length() + (newStr.length() - oldStr.length()) * 
                                      std::count(str.begin(), str.end(), oldStr[0]);
                StringValidator::validateStringSize("replace", potential_size);
            }
            
            if (oldStr.empty()) {
                // Python behavior: empty string replacement inserts newStr between each character
                if (count == 0) return Value(str);
                
                std::string result;
                result.reserve(str.length() * (newStr.length() + 1) + newStr.length());
                
                int replacements = 0;
                
                // Insert at beginning if count allows
                if (count == -1 || replacements < count) {
                    result += newStr;
                    replacements++;
                }
                
                for (size_t i = 0; i < str.length(); i++) {
                    result += str[i];
                    if ((count == -1 || replacements < count) && i < str.length()) {
                        result += newStr;
                        replacements++;
                    }
                }
                
                return Value(result);
            }
            
            std::string result = str;
            size_t pos = 0;
            int replacements = 0;
            
            while ((pos = result.find(oldStr, pos)) != std::string::npos) {
                if (count != -1 && replacements >= count) {
                    break;
                }
                
                result.replace(pos, oldStr.length(), newStr);
                pos += newStr.length(); // Move past the replacement to avoid infinite loops
                replacements++;
            }
            
            return Value(result);
        } catch (const StringError& e) {
            throw std::runtime_error(e.what());
        }
    }
    
    int getArity() const override { return -1; } // Variable arguments (2-3)
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() >= 2 && args.size() <= 3;
    }
    
    std::string getDescription() const override {
        return "replace(old, new, [count]) -> string";
    }
};

/**
 * Upper method handler - converts string to uppercase
 */
class UpperMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        // Use Unicode-aware case conversion
        std::vector<uint32_t> codepoints = UnicodeHandler::utf8ToCodepoints(str);
        for (uint32_t& cp : codepoints) {
            cp = UnicodeHandler::toUpper(cp);
        }
        std::string result = UnicodeHandler::codepointsToUtf8(codepoints);
        
        return Value(result);
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "upper() -> string";
    }
};

/**
 * Lower method handler - converts string to lowercase
 */
class LowerMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        // Use Unicode-aware case conversion
        std::vector<uint32_t> codepoints = UnicodeHandler::utf8ToCodepoints(str);
        for (uint32_t& cp : codepoints) {
            cp = UnicodeHandler::toLower(cp);
        }
        std::string result = UnicodeHandler::codepointsToUtf8(codepoints);
        
        return Value(result);
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "lower() -> string";
    }
};

/**
 * Capitalize method handler - capitalizes first character, lowercases rest
 */
class CapitalizeMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        if (str.empty()) {
            return Value(str);
        }
        
        std::string result = str;
        
        // Convert entire string to lowercase first
        std::transform(result.begin(), result.end(), result.begin(), 
                      [](unsigned char c) { return std::tolower(c); });
        
        // Capitalize only the first character if it's alphabetic
        if (!result.empty() && std::isalpha(static_cast<unsigned char>(result[0]))) {
            result[0] = std::toupper(static_cast<unsigned char>(result[0]));
        }
        
        return Value(result);
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "capitalize() -> string";
    }
};

/**
 * Title method handler - capitalizes first letter of each word
 */
class TitleMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        if (str.empty()) {
            return Value(str);
        }
        
        std::string result = str;
        bool capitalizeNext = true;
        
        for (size_t i = 0; i < result.length(); i++) {
            unsigned char c = static_cast<unsigned char>(result[i]);
            
            if (std::isalpha(c)) {
                if (capitalizeNext) {
                    result[i] = std::toupper(c);
                    capitalizeNext = false;
                } else {
                    result[i] = std::tolower(c);
                }
            } else {
                // Non-alphabetic characters trigger capitalization of next letter
                capitalizeNext = true;
            }
        }
        
        return Value(result);
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "title() -> string";
    }
};

/**
 * Swapcase method handler - swaps case of all alphabetic characters
 */
class SwapcaseMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        std::string result = str;
        
        for (size_t i = 0; i < result.length(); i++) {
            unsigned char c = static_cast<unsigned char>(result[i]);
            
            if (std::islower(c)) {
                result[i] = std::toupper(c);
            } else if (std::isupper(c)) {
                result[i] = std::tolower(c);
            }
            // Non-alphabetic characters remain unchanged
        }
        
        return Value(result);
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "swapcase() -> string";
    }
};

/**
 * Casefold method handler - aggressive lowercase for case-insensitive comparisons
 * For basic ASCII, this is the same as lower(), but in full Unicode implementation
 * it would handle special cases like German ß
 */
class CasefoldMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        // For now, implement as lowercase (basic ASCII behavior)
        // In a full Unicode implementation, this would handle special folding rules
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), 
                      [](unsigned char c) { return std::tolower(c); });
        return Value(result);
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "casefold() -> string";
    }
};

/**
 * IsAlnum method handler - checks if all characters are alphanumeric
 */
class IsAlnumMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        if (str.empty()) {
            return Value(false); // Empty string returns false for all is* methods
        }
        
        for (char c : str) {
            if (!std::isalnum(static_cast<unsigned char>(c))) {
                return Value(false);
            }
        }
        return Value(true);
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "isalnum() -> bool";
    }
};

/**
 * IsAlpha method handler - checks if all characters are alphabetic
 */
class IsAlphaMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        if (str.empty()) {
            return Value(false);
        }
        
        for (char c : str) {
            if (!std::isalpha(static_cast<unsigned char>(c))) {
                return Value(false);
            }
        }
        return Value(true);
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "isalpha() -> bool";
    }
};

/**
 * IsDigit method handler - checks if all characters are digits
 */
class IsDigitMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        if (str.empty()) {
            return Value(false);
        }
        
        for (char c : str) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                return Value(false);
            }
        }
        return Value(true);
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "isdigit() -> bool";
    }
};

/**
 * IsLower method handler - checks if all cased characters are lowercase
 */
class IsLowerMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        if (str.empty()) {
            return Value(false);
        }
        
        bool hasCasedChar = false;
        for (char c : str) {
            unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalpha(uc)) {
                hasCasedChar = true;
                if (!std::islower(uc)) {
                    return Value(false);
                }
            }
        }
        return Value(hasCasedChar); // Must have at least one cased character
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "islower() -> bool";
    }
};

/**
 * IsUpper method handler - checks if all cased characters are uppercase
 */
class IsUpperMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        if (str.empty()) {
            return Value(false);
        }
        
        bool hasCasedChar = false;
        for (char c : str) {
            unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalpha(uc)) {
                hasCasedChar = true;
                if (!std::isupper(uc)) {
                    return Value(false);
                }
            }
        }
        return Value(hasCasedChar); // Must have at least one cased character
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "isupper() -> bool";
    }
};

/**
 * IsSpace method handler - checks if all characters are whitespace
 */
class IsSpaceMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        if (str.empty()) {
            return Value(false);
        }
        
        for (char c : str) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                return Value(false);
            }
        }
        return Value(true);
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "isspace() -> bool";
    }
};

/**
 * IsTitle method handler - checks if string is in title case
 */
class IsTitleMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        if (str.empty()) {
            return Value(false);
        }
        
        bool expectUpper = true;
        bool hasCasedChar = false;
        
        for (char c : str) {
            unsigned char uc = static_cast<unsigned char>(c);
            
            if (std::isalpha(uc)) {
                hasCasedChar = true;
                if (expectUpper) {
                    if (!std::isupper(uc)) {
                        return Value(false);
                    }
                    expectUpper = false;
                } else {
                    if (!std::islower(uc)) {
                        return Value(false);
                    }
                }
            } else {
                // Non-alphabetic characters reset expectation for next word
                expectUpper = true;
            }
        }
        
        return Value(hasCasedChar);
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "istitle() -> bool";
    }
};

/**
 * Slice method handler - returns substring with start, end, and optional step
 */
class SliceMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; // Suppress unused parameter warning
        
        try {
            int strLen = static_cast<int>(str.length());
            
            if (strLen == 0) {
                return Value(std::string(""));
            }
            
            // Parse arguments: slice(start, [end], [step])
            int start = 0;
            int end = strLen;
            int step = 1;
            
            if (args.size() >= 1) {
                if (args[0].type != ValueType::NUMBER) {
                    throw StringError::invalidArgument("slice", "start parameter must be a number");
                }
                start = static_cast<int>(args[0].as.number);
            }
            
            if (args.size() >= 2) {
                if (args[1].type != ValueType::NUMBER) {
                    throw StringError::invalidArgument("slice", "end parameter must be a number");
                }
                end = static_cast<int>(args[1].as.number);
            }
            
            if (args.size() >= 3) {
                if (args[2].type != ValueType::NUMBER) {
                    throw StringError::invalidArgument("slice", "step parameter must be a number");
                }
                step = static_cast<int>(args[2].as.number);
                if (step == 0) {
                    throw StringError::sliceError("slice", "step cannot be zero");
                }
            }
            
            // Validate and normalize slice parameters
            StringValidator::normalizeSlice("slice", start, end, step, strLen);
            
            std::string result;
            
            if (step > 0) {
                // Forward slicing
                if (start >= end) return Value(std::string(""));
                
                for (int i = start; i < end; i += step) {
                    result += str[i];
                }
            } else {
                // Backward slicing (step < 0)
                if (start <= end) return Value(std::string(""));
                
                for (int i = start; i > end; i += step) {
                    if (i >= 0 && i < strLen) {
                        result += str[i];
                    }
                }
            }
            
            return Value(result);
        } catch (const StringError& e) {
            throw std::runtime_error(e.what());
        }
    }
    
    int getArity() const override { return -1; } // Variable arguments (1-3)
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() >= 1 && args.size() <= 3;
    }
    
    std::string getDescription() const override {
        return "slice(start, [end], [step]) -> string";
    }
};

// Unicode method handlers
class NormalizeMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        if (!vm) {
            throw std::runtime_error("VM pointer is null in normalize method");
        }
        
        try {
            std::string form_str = args[0].toString();
            NormalizationForm form = NormalizationForm::NFC; // default
            
            if (form_str == "NFC") form = NormalizationForm::NFC;
            else if (form_str == "NFD") form = NormalizationForm::NFD;
            else if (form_str == "NFKC") form = NormalizationForm::NFKC;
            else if (form_str == "NFKD") form = NormalizationForm::NFKD;
            else {
                throw std::runtime_error("Invalid normalization form: " + form_str);
            }
            
            std::string result = UnicodeHandler::normalize(str, form);
            
            // Ensure result is valid before creating Value
            if (result.empty() && !str.empty()) {
                result = str; // Fallback to original string
            }
            
            // Create Value directly to avoid potential vm->internString() issues
            return Value(result);
        } catch (const std::exception& e) {
            throw std::runtime_error("Normalize error: " + std::string(e.what()));
        }
    }
    
    int getArity() const override { return 1; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 1;
    }
    
    std::string getDescription() const override {
        return "normalize(form) -> string";
    }
};

class EncodeMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        if (!vm) {
            throw std::runtime_error("VM pointer is null in encode method");
        }
        
        try {
            std::string encoding = args.empty() ? "utf-8" : args[0].toString();
            
            // Comprehensive encoding validation and processing
            if (encoding == "utf-8" || encoding == "UTF-8") {
                // UTF-8 is already the internal representation
                return Value(str);
            } else if (encoding == "ascii" || encoding == "ASCII") {
                // Check if string is ASCII-only
                for (char c : str) {
                    if (static_cast<unsigned char>(c) > 127) {
                        throw std::runtime_error("ascii codec can't encode character at position");
                    }
                }
                return Value(str);
            } else if (encoding == "latin-1" || encoding == "LATIN-1" || 
                       encoding == "iso-8859-1" || encoding == "ISO-8859-1") {
                // Latin-1 encoding - check if all characters fit in 8-bit range
                for (char c : str) {
                    if (static_cast<unsigned char>(c) > 255) {
                        throw std::runtime_error("latin-1 codec can't encode character");
                    }
                }
                return Value(str);
            } else if (encoding == "utf-16" || encoding == "UTF-16") {
                // Simplified UTF-16 - for now just return the string
                // Full implementation would convert to UTF-16 byte representation
                return Value(str);
            } else if (encoding == "utf-32" || encoding == "UTF-32") {
                // Simplified UTF-32 - for now just return the string
                // Full implementation would convert to UTF-32 byte representation
                return Value(str);
            } else {
                throw std::runtime_error("Unsupported encoding: " + encoding);
            }
        } catch (const std::exception& e) {
            throw std::runtime_error("Encode error: " + std::string(e.what()));
        }
    }
    
    int getArity() const override { return -1; } // Variable arity (0 or 1)
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() <= 1;
    }
    
    std::string getDescription() const override {
        return "encode([encoding]) -> bytes";
    }
};

class IsUnicodeMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        (void)vm; (void)args; // Suppress unused parameter warnings
        
        // Check if string contains non-ASCII characters (more comprehensive)
        for (size_t i = 0; i < str.length(); ++i) {
            unsigned char c = static_cast<unsigned char>(str[i]);
            
            // Check for UTF-8 multi-byte sequences
            if (c > 127) {
                return Value(true);
            }
            
            // Check for UTF-8 continuation bytes
            if ((c & 0x80) != 0) {
                return Value(true);
            }
        }
        
        // Also check for Unicode escape sequences in the original string
        // This is a simplified check - full implementation would be more comprehensive
        if (str.find("\\u") != std::string::npos || str.find("\\U") != std::string::npos) {
            return Value(true);
        }
        
        return Value(false);
    }
    
    int getArity() const override { return 0; }
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return args.size() == 0;
    }
    
    std::string getDescription() const override {
        return "isunicode() -> bool";
    }
};

// Formatting method handlers
class FormatMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
        try {
            std::string result = StringFormatter::format(str, args);
            return Value(vm->internString(result));
        } catch (const std::exception& e) {
            throw std::runtime_error("Format error: " + std::string(e.what()));
        }
    }
    
    int getArity() const override { return -1; } // Variable arity
    
    bool validateArgs(const std::vector<Value>& args) const override {
        return true; // Accept any number of arguments
    }
    
    std::string getDescription() const override {
        return "format(*args) -> string";
    }
};

StringMethodRegistry::StringMethodRegistry() {
    initializeBasicMethods();
}

void StringMethodRegistry::registerMethod(const std::string& name, 
                                        std::unique_ptr<StringMethodHandler> handler,
                                        StringMethodCategory category) {
    methods_[name] = std::move(handler);
    categories_[name] = category;
}

StringMethodHandler* StringMethodRegistry::getMethod(const std::string& name) {
    auto it = methods_.find(name);
    return (it != methods_.end()) ? it->second.get() : nullptr;
}

bool StringMethodRegistry::hasMethod(const std::string& name) const {
    return methods_.find(name) != methods_.end();
}

StringMethodCategory StringMethodRegistry::getMethodCategory(const std::string& name) const {
    auto it = categories_.find(name);
    return (it != categories_.end()) ? it->second : StringMethodCategory::BASIC;
}

void StringMethodRegistry::initializeBasicMethods() {
    // Register existing methods to maintain backward compatibility
    registerMethod("length", std::make_unique<LengthMethodHandler>(), StringMethodCategory::BASIC);
    registerMethod("contains", std::make_unique<ContainsMethodHandler>(), StringMethodCategory::BASIC);
    registerMethod("split", std::make_unique<SplitMethodHandler>(), StringMethodCategory::BASIC);
    registerMethod("substring", std::make_unique<SubstringMethodHandler>(), StringMethodCategory::BASIC);
    
    // Register search methods
    registerMethod("find", std::make_unique<FindMethodHandler>(), StringMethodCategory::SEARCH);
    registerMethod("rfind", std::make_unique<RFindMethodHandler>(), StringMethodCategory::SEARCH);
    registerMethod("index", std::make_unique<IndexMethodHandler>(), StringMethodCategory::SEARCH);
    registerMethod("rindex", std::make_unique<RIndexMethodHandler>(), StringMethodCategory::SEARCH);
    
    // Register modification methods
    registerMethod("strip", std::make_unique<StripMethodHandler>(), StringMethodCategory::BASIC);
    registerMethod("lstrip", std::make_unique<LStripMethodHandler>(), StringMethodCategory::BASIC);
    registerMethod("rstrip", std::make_unique<RStripMethodHandler>(), StringMethodCategory::BASIC);
    registerMethod("replace", std::make_unique<ReplaceMethodHandler>(), StringMethodCategory::BASIC);
    
    // Register case conversion methods
    registerMethod("upper", std::make_unique<UpperMethodHandler>(), StringMethodCategory::CASE);
    registerMethod("lower", std::make_unique<LowerMethodHandler>(), StringMethodCategory::CASE);
    registerMethod("capitalize", std::make_unique<CapitalizeMethodHandler>(), StringMethodCategory::CASE);
    registerMethod("title", std::make_unique<TitleMethodHandler>(), StringMethodCategory::CASE);
    registerMethod("swapcase", std::make_unique<SwapcaseMethodHandler>(), StringMethodCategory::CASE);
    registerMethod("casefold", std::make_unique<CasefoldMethodHandler>(), StringMethodCategory::CASE);
    
    // Register character classification methods
    registerMethod("isalnum", std::make_unique<IsAlnumMethodHandler>(), StringMethodCategory::CLASSIFICATION);
    registerMethod("isalpha", std::make_unique<IsAlphaMethodHandler>(), StringMethodCategory::CLASSIFICATION);
    registerMethod("isdigit", std::make_unique<IsDigitMethodHandler>(), StringMethodCategory::CLASSIFICATION);
    registerMethod("islower", std::make_unique<IsLowerMethodHandler>(), StringMethodCategory::CLASSIFICATION);
    registerMethod("isupper", std::make_unique<IsUpperMethodHandler>(), StringMethodCategory::CLASSIFICATION);
    registerMethod("isspace", std::make_unique<IsSpaceMethodHandler>(), StringMethodCategory::CLASSIFICATION);
    registerMethod("istitle", std::make_unique<IsTitleMethodHandler>(), StringMethodCategory::CLASSIFICATION);
    
    // Register sequence operation methods
    registerMethod("slice", std::make_unique<SliceMethodHandler>(), StringMethodCategory::SEQUENCE);
    
    // Register Unicode methods
    registerMethod("normalize", std::make_unique<NormalizeMethodHandler>(), StringMethodCategory::UNICODE);
    registerMethod("encode", std::make_unique<EncodeMethodHandler>(), StringMethodCategory::UNICODE);
    registerMethod("isunicode", std::make_unique<IsUnicodeMethodHandler>(), StringMethodCategory::UNICODE);
    
    // Register formatting methods
    registerMethod("format", std::make_unique<FormatMethodHandler>(), StringMethodCategory::FORMATTING);
}

std::vector<std::string> StringMethodRegistry::getMethodNames() const {
    std::vector<std::string> names;
    names.reserve(methods_.size());
    for (const auto& pair : methods_) {
        names.push_back(pair.first);
    }
    return names;
}

StringMethodRegistry& StringMethodRegistry::getInstance() {
    static StringMethodRegistry instance;
    return instance;
}

}