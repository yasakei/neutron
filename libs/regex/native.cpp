#include "native.h"
#include "vm.h"
#include "types/obj_string.h"
#include <string>
#include <regex>
#include <vector>

namespace neutron {

// Test if a string matches a regex pattern
Value regex_test(VM& /*vm*/, std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for regex.test(text, pattern).");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("First argument for regex.test() must be a string.");
    }
    if (arguments[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Second argument for regex.test() must be a regex pattern string.");
    }
    
    std::string text = arguments[0].as.obj_string->chars;
    std::string pattern = arguments[1].as.obj_string->chars;
    
    try {
        std::regex re(pattern);
        bool matches = std::regex_match(text, re);
        return Value(matches);
    } catch (const std::regex_error& e) {
        throw std::runtime_error("Invalid regex pattern: " + std::string(e.what()));
    }
}

// Search for a pattern in text (returns true if found)
Value regex_search(VM& /*vm*/, std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for regex.search(text, pattern).");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("First argument for regex.search() must be a string.");
    }
    if (arguments[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Second argument for regex.search() must be a regex pattern string.");
    }
    
    std::string text = arguments[0].as.obj_string->chars;
    std::string pattern = arguments[1].as.obj_string->chars;
    
    try {
        std::regex re(pattern);
        bool found = std::regex_search(text, re);
        return Value(found);
    } catch (const std::regex_error& e) {
        throw std::runtime_error("Invalid regex pattern: " + std::string(e.what()));
    }
}

// Find first match and return match object with groups
Value regex_find(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for regex.find(text, pattern).");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("First argument for regex.find() must be a string.");
    }
    if (arguments[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Second argument for regex.find() must be a regex pattern string.");
    }
    
    std::string text = arguments[0].as.obj_string->chars;
    std::string pattern = arguments[1].as.obj_string->chars;
    
    try {
        std::regex re(pattern);
        std::smatch match;
        
        if (std::regex_search(text, match, re)) {
            auto matchObj = vm.allocate<JsonObject>();
            matchObj->properties[vm.internString("matched")] = Value(vm.internString(match.str()));
            matchObj->properties[vm.internString("position")] = Value(static_cast<double>(match.position()));
            matchObj->properties[vm.internString("length")] = Value(static_cast<double>(match.length()));
            
            // Add capture groups
            auto groupsArray = vm.allocate<Array>();
            for (size_t i = 0; i < match.size(); i++) {
                groupsArray->elements.push_back(Value(vm.internString(match[i].str())));
            }
            matchObj->properties[vm.internString("groups")] = Value(groupsArray);
            
            return Value(matchObj);
        }
        
        return Value(); // nil if no match
    } catch (const std::regex_error& e) {
        throw std::runtime_error("Invalid regex pattern: " + std::string(e.what()));
    }
}

// Find all matches in text
Value regex_findAll(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for regex.findAll(text, pattern).");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("First argument for regex.findAll() must be a string.");
    }
    if (arguments[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Second argument for regex.findAll() must be a regex pattern string.");
    }
    
    std::string text = arguments[0].as.obj_string->chars;
    std::string pattern = arguments[1].as.obj_string->chars;
    
    try {
        std::regex re(pattern);
        auto matchesArray = vm.allocate<Array>();
        
        auto words_begin = std::sregex_iterator(text.begin(), text.end(), re);
        auto words_end = std::sregex_iterator();
        
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            auto matchObj = vm.allocate<JsonObject>();
            matchObj->properties[vm.internString("matched")] = Value(vm.internString(match.str()));
            matchObj->properties[vm.internString("position")] = Value(static_cast<double>(match.position()));
            matchObj->properties[vm.internString("length")] = Value(static_cast<double>(match.length()));
            
            // Add capture groups
            auto groupsArray = vm.allocate<Array>();
            for (size_t j = 0; j < match.size(); j++) {
                groupsArray->elements.push_back(Value(vm.internString(match[j].str())));
            }
            matchObj->properties[vm.internString("groups")] = Value(groupsArray);
            
            matchesArray->elements.push_back(Value(matchObj));
        }
        
        return Value(matchesArray);
    } catch (const std::regex_error& e) {
        throw std::runtime_error("Invalid regex pattern: " + std::string(e.what()));
    }
}

// Replace matches with a replacement string
Value regex_replace(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 3) {
        throw std::runtime_error("Expected 3 arguments for regex.replace(text, pattern, replacement).");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("First argument for regex.replace() must be a string.");
    }
    if (arguments[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Second argument for regex.replace() must be a regex pattern string.");
    }
    if (arguments[2].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Third argument for regex.replace() must be a replacement string.");
    }
    
    std::string text = arguments[0].as.obj_string->chars;
    std::string pattern = arguments[1].as.obj_string->chars;
    std::string replacement = arguments[2].as.obj_string->chars;
    
    try {
        std::regex re(pattern);
        std::string result = std::regex_replace(text, re, replacement);
        return Value(vm.internString(result));
    } catch (const std::regex_error& e) {
        throw std::runtime_error("Invalid regex pattern: " + std::string(e.what()));
    }
}

// Split string by regex pattern
Value regex_split(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for regex.split(text, pattern).");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("First argument for regex.split() must be a string.");
    }
    if (arguments[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Second argument for regex.split() must be a regex pattern string.");
    }
    
    std::string text = arguments[0].as.obj_string->chars;
    std::string pattern = arguments[1].as.obj_string->chars;
    
    try {
        std::regex re(pattern);
        auto partsArray = vm.allocate<Array>();
        
        std::sregex_token_iterator iter(text.begin(), text.end(), re, -1);
        std::sregex_token_iterator end;
        
        for (; iter != end; ++iter) {
            partsArray->elements.push_back(Value(vm.internString(iter->str())));
        }
        
        return Value(partsArray);
    } catch (const std::regex_error& e) {
        throw std::runtime_error("Invalid regex pattern: " + std::string(e.what()));
    }
}

// Test if pattern is valid
Value regex_isValid(VM& /*vm*/, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for regex.isValid(pattern).");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for regex.isValid() must be a regex pattern string.");
    }
    
    std::string pattern = arguments[0].as.obj_string->chars;
    
    try {
        std::regex re(pattern);
        return Value(true);
    } catch (const std::regex_error&) {
        return Value(false);
    }
}

// Escape special regex characters
Value regex_escape(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for regex.escape(text).");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for regex.escape() must be a string.");
    }
    
    std::string text = arguments[0].as.obj_string->chars;
    std::string escaped;
    
    // Characters that need escaping in regex
    const std::string specialChars = "\\^$.|?*+()[]{}";
    
    for (char ch : text) {
        if (specialChars.find(ch) != std::string::npos) {
            escaped += '\\';
        }
        escaped += ch;
    }
    
    return Value(vm.internString(escaped));
}

void register_regex_functions(VM& vm, std::shared_ptr<Environment> env) {
    env->define("test", Value(vm.allocate<NativeFn>(regex_test, 2, true)));
    env->define("search", Value(vm.allocate<NativeFn>(regex_search, 2, true)));
    env->define("find", Value(vm.allocate<NativeFn>(regex_find, 2, true)));
    env->define("findAll", Value(vm.allocate<NativeFn>(regex_findAll, 2, true)));
    env->define("replace", Value(vm.allocate<NativeFn>(regex_replace, 3, true)));
    env->define("split", Value(vm.allocate<NativeFn>(regex_split, 2, true)));
    env->define("isValid", Value(vm.allocate<NativeFn>(regex_isValid, 1, true)));
    env->define("escape", Value(vm.allocate<NativeFn>(regex_escape, 1, true)));
}

} // namespace neutron

extern "C" void neutron_init_regex_module(neutron::VM* vm) {
    auto regex_env = std::make_shared<neutron::Environment>();
    neutron::register_regex_functions(*vm, regex_env);
    auto regex_module = vm->allocate<neutron::Module>("regex", regex_env);
    vm->define_module("regex", regex_module);
}
