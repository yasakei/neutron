#include "types/string_method_handler.h"
#include "types/string_method_registry.h"
#include "types/value.h"
#include "core/vm.h"
#include <stdexcept>
#include <algorithm>

namespace neutron {

/**
 * Find method handler - returns index of first occurrence or -1
 */
class FindMethodHandler : public StringMethodHandler {
public:
    Value execute(VM* vm, const std::string& str, const std::vector<Value>& args) override {
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
        
        if (start >= str.length()) {
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
        
        if (start >= str.length()) {
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
 * Register all search methods with the registry
 */
void registerSearchMethods() {
    StringMethodRegistry& registry = StringMethodRegistry::getInstance();
    
    registry.registerMethod("find", std::make_unique<FindMethodHandler>(), StringMethodCategory::SEARCH);
    registry.registerMethod("rfind", std::make_unique<RFindMethodHandler>(), StringMethodCategory::SEARCH);
    registry.registerMethod("index", std::make_unique<IndexMethodHandler>(), StringMethodCategory::SEARCH);
    registry.registerMethod("rindex", std::make_unique<RIndexMethodHandler>(), StringMethodCategory::SEARCH);
}

}