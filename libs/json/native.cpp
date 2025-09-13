#include "libs/json/native.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace neutron {

// Forward declaration for helper functions
std::string valueToJsonString(const Value& value, bool pretty = false, int indent = 0);
Value parseJsonString(const std::string& json);

// JSON stringify function
Value json_stringify(std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::runtime_error("Expected 1 or 2 arguments for json.stringify().");
    }
    
    bool pretty = false;
    if (arguments.size() == 2) {
        if (arguments[1].type != ValueType::BOOLEAN) {
            throw std::runtime_error("Second argument for json.stringify() must be a boolean.");
        }
        pretty = arguments[1].as.boolean;
    }
    
    std::string result = valueToJsonString(arguments[0], pretty);
    return Value(result);
}

// JSON parse function
Value json_parse(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for json.parse().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Argument for json.parse() must be a string.");
    }
    
    return parseJsonString(arguments[0].as.string->c_str());
}

// Helper function to convert Value to JSON string
std::string valueToJsonString(const Value& value, bool pretty, int indent) {
    std::string indentStr = pretty ? std::string(indent * 2, ' ') : "";
    std::string nextIndentStr = pretty ? std::string((indent + 1) * 2, ' ') : "";
    
    switch (value.type) {
        case ValueType::NIL:
            return "null";
            
        case ValueType::BOOLEAN:
            return value.as.boolean ? "true" : "false";
            
        case ValueType::NUMBER:
            {
                std::ostringstream oss;
                oss << std::setprecision(15) << value.as.number;
                return oss.str();
            }
            
        case ValueType::STRING:
            {
                std::string str = *value.as.string;
                // Escape special characters in JSON strings
                std::ostringstream oss;
                oss << "\"";
                for (char c : str) {
                    switch (c) {
                        case '"':  oss << "\\\""; break;
                        case '\\': oss << "\\\\"; break;
                        case '\b': oss << "\\b";  break;
                        case '\f': oss << "\\f";  break;
                        case '\n': oss << "\\n";  break;
                        case '\r': oss << "\\r";  break;
                        case '\t': oss << "\\t";  break;
                        default:
                            if (c >= 0 && c < 32) {
                                // Control characters need to be escaped as \u00xx
                                oss << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
                            } else {
                                oss << c;
                            }
                    }
                }
                oss << "\"";
                return oss.str();
            }
            
        case ValueType::OBJECT:
            {
                JsonObject* obj = dynamic_cast<JsonObject*>(value.as.object);
                if (obj) {
                    std::ostringstream oss;
                    oss << "{";
                    if (pretty) oss << "\n";
                    
                    bool first = true;
                    for (const auto& pair : obj->properties) {
                        if (!first) {
                            oss << ",";
                            if (pretty) oss << "\n";
                            else oss << " ";
                        }
                        
                        if (pretty) oss << nextIndentStr;
                        oss << valueToJsonString(Value(pair.first), false) << ": " << valueToJsonString(pair.second, pretty, indent + 1);
                        first = false;
                    }
                    
                    if (pretty && !obj->properties.empty()) oss << "\n" << indentStr;
                    oss << "}";
                    return oss.str();
                }
                
                JsonArray* arr = dynamic_cast<JsonArray*>(value.as.object);
                if (arr) {
                    std::ostringstream oss;
                    oss << "[";
                    if (pretty) oss << "\n";
                    
                    bool first = true;
                    for (const auto& element : arr->elements) {
                        if (!first) {
                            oss << ",";
                            if (pretty) oss << "\n";
                            else oss << " ";
                        }
                        
                        if (pretty) oss << nextIndentStr;
                        oss << valueToJsonString(element, pretty, indent + 1);
                        first = false;
                    }
                    
                    if (pretty && !arr->elements.empty()) oss << "\n" << indentStr;
                    oss << "]";
                    return oss.str();
                }
                
                return "\"<object>\"";
            }
            
        default:
            return "\"<unknown>\"";
    }
}

// Simple JSON parser (basic implementation)
Value parseJsonString(const std::string& json) {
    // This is a simplified parser that handles basic JSON structures
    // For a production implementation, you would want a more robust parser
    
    std::string trimmed = json;
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](int ch) {
        return !std::isspace(ch);
    }));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), trimmed.end());
    
    if (trimmed.empty()) {
        throw std::runtime_error("Invalid JSON: empty string");
    }
    
    // Handle null
    if (trimmed == "null") {
        return Value(nullptr);
    }
    
    // Handle boolean
    if (trimmed == "true") {
        return Value(true);
    }
    if (trimmed == "false") {
        return Value(false);
    }
    
    // Handle numbers
    if (std::isdigit(trimmed[0]) || trimmed[0] == '-') {
        try {
            double num = std::stod(trimmed);
            return Value(num);
        } catch (...) {
            throw std::runtime_error("Invalid JSON number: " + trimmed);
        }
    }
    
    // Handle strings
    if (trimmed[0] == '"') {
        if (trimmed.back() != '"') {
            throw std::runtime_error("Invalid JSON string: " + trimmed);
        }
        // Remove quotes and unescape
        std::string str = trimmed.substr(1, trimmed.length() - 2);
        return Value(str);
    }
    
    // Handle objects
    if (trimmed[0] == '{') {
        if (trimmed.back() != '}') {
            throw std::runtime_error("Invalid JSON object: " + trimmed);
        }
        
        auto obj = new JsonObject();
        std::string content = trimmed.substr(1, trimmed.length() - 2);
        
        size_t pos = 0;
        while (pos < content.length()) {
            size_t key_start = content.find('"', pos);
            if (key_start == std::string::npos) break;
            
            size_t key_end = content.find('"', key_start + 1);
            if (key_end == std::string::npos) break;
            
            std::string key = content.substr(key_start + 1, key_end - key_start - 1);
            
            size_t colon = content.find(':', key_end);
            if (colon == std::string::npos) break;
            
            size_t value_start = content.find_first_not_of(" \t\n\r", colon + 1);
            if (value_start == std::string::npos) break;
            
            size_t value_end;
            if (content[value_start] == '"') {
                value_end = content.find('"', value_start + 1);
                if (value_end != std::string::npos) {
                    std::string value_str = content.substr(value_start + 1, value_end - value_start - 1);
                    obj->properties[key] = Value(value_str);
                    pos = value_end + 1;
                } else {
                    break; 
                }
            } else {
                value_end = content.find_first_of(",}", value_start);
                if (value_end == std::string::npos) {
                    value_end = content.length();
                }
                std::string value_str = content.substr(value_start, value_end - value_start);
                // For simplicity, assuming the value is a string, but it could be a number, bool, etc.
                // A more robust parser would recursively call parseJsonString here.
                obj->properties[key] = Value(value_str);
                pos = value_end;
            }
            
            size_t comma = content.find(',', pos);
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
        
        return Value(obj);
    }
    
    // Handle arrays
    if (trimmed[0] == '[') {
        if (trimmed.back() != ']') {
            throw std::runtime_error("Invalid JSON array: " + trimmed);
        }
        
        auto arr = new JsonArray();
        // For now, return empty array - a full parser would parse the contents
        return Value(arr);
    }
    
    throw std::runtime_error("Invalid JSON: " + trimmed);
}

// JSON get function (to access properties of JSON objects)
Value json_get(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for json.get().");
    }
    
    if (arguments[0].type != ValueType::OBJECT) {
        throw std::runtime_error("First argument for json.get() must be an object.");
    }
    
    if (arguments[1].type != ValueType::STRING) {
        throw std::runtime_error("Second argument for json.get() must be a string key.");
    }
    
    JsonObject* obj = dynamic_cast<JsonObject*>(arguments[0].as.object);
    if (!obj) {
        throw std::runtime_error("First argument for json.get() must be a JSON object.");
    }
    
    std::string key = *arguments[1].as.string;
    auto it = obj->properties.find(key);
    if (it != obj->properties.end()) {
        return it->second;
    }
    
    return Value(nullptr);
}

// Register JSON functions in the environment
void register_json_functions(std::shared_ptr<Environment> env) {
    // Create a JSON module
    auto jsonEnv = std::make_shared<Environment>();
    jsonEnv->define("stringify", Value(new NativeFn(json_stringify, -1))); // -1 for variable arguments
    jsonEnv->define("parse", Value(new NativeFn(json_parse, 1)));
    jsonEnv->define("get", Value(new NativeFn(json_get, 2)));
    
    auto jsonModule = new Module("json", jsonEnv, std::vector<std::unique_ptr<Stmt>>());
    env->define("json", Value(jsonModule));
}

} // namespace neutron