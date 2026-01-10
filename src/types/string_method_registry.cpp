#include "types/string_method_registry.h"
#include "types/string_search_methods.h"
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
        std::string substr = args[0].toString();
        
        size_t start = 0;
        size_t end = str.length();
        
        // Handle optional start parameter
        if (args.size() >= 2) {
            if (args[1].type != ValueType::NUMBER) {
                throw std::runtime_error("find() start parameter must be a number");
            }
            int startInt = static_cast<int>(args[1].as.number);
            if (startInt < 0) startInt = 0;
            start = static_cast<size_t>(startInt);
        }
        
        // Handle optional end parameter
        if (args.size() >= 3) {
            if (args[2].type != ValueType::NUMBER) {
                throw std::runtime_error("find() end parameter must be a number");
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
        
        if (start >= str.length()) {
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
        
        if (start >= str.length()) {
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
        
        std::string oldStr = args[0].toString();
        std::string newStr = args[1].toString();
        int count = -1; // Replace all by default
        
        // Handle optional count parameter
        if (args.size() >= 3) {
            if (args[2].type != ValueType::NUMBER) {
                throw std::runtime_error("replace() count parameter must be a number");
            }
            count = static_cast<int>(args[2].as.number);
            if (count < 0) count = -1; // Negative means replace all
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
        
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), 
                      [](unsigned char c) { return std::toupper(c); });
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
    static bool initialized = false;
    
    if (!initialized) {
        instance.initializeBasicMethods();
        initialized = true;
    }
    
    return instance;
}

}