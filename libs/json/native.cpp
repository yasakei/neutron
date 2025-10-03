#include "native.h"
#include "vm.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

using namespace neutron;

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
        pretty = std::get<bool>(arguments[1].as);
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
    
    return parseJsonString(std::get<std::string>(arguments[0].as).c_str());
}

// Helper function to convert Value to JSON string
std::string valueToJsonString(const Value& value, bool pretty, int indent) {
    std::string indentStr = pretty ? std::string(indent * 2, ' ') : "";
    std::string nextIndentStr = pretty ? std::string((indent + 1) * 2, ' ') : "";
    
    switch (value.type) {
        case ValueType::NIL:
            return "null";
        case ValueType::BOOLEAN:
            return std::get<bool>(value.as) ? "true" : "false";
        case ValueType::NUMBER: {
            std::ostringstream oss;
            oss << std::setprecision(15) << std::get<double>(value.as);
            return oss.str();
        }
        case ValueType::STRING: {
            std::string str = std::get<std::string>(value.as);
            // Escape special characters
            std::string escaped;
            for (char c : str) {
                switch (c) {
                    case '"':  escaped += "\\\""; break;
                    case '\\': escaped += "\\\\"; break;
                    case '\b': escaped += "\\b";  break;
                    case '\f': escaped += "\\f";  break;
                    case '\n': escaped += "\\n";  break;
                    case '\r': escaped += "\\r";  break;
                    case '\t': escaped += "\\t";  break;
                    default:
                        if (c >= 0 && c < 32) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", c);
                            escaped += buf;
                        } else {
                            escaped += c;
                        }
                }
            }
            return "\"" + escaped + "\"";
        }
        case ValueType::OBJECT: {
            JsonObject* obj = dynamic_cast<JsonObject*>(std::get<Object*>(value.as));
            if (obj) {
                std::ostringstream oss;
                oss << "{";
                bool first = true;
                for (const auto& pair : obj->properties) {
                    if (!first) {
                        oss << (pretty ? ",\n" + nextIndentStr : ",");
                    } else if (pretty) {
                        oss << "\n" + nextIndentStr;
                    }
                    oss << "\"" << pair.first << "\":" << (pretty ? " " : "");
                    oss << valueToJsonString(pair.second, pretty, indent + 1);
                    first = false;
                }
                if (pretty && !first) {
                    oss << "\n" + indentStr;
                }
                oss << "}";
                return oss.str();
            }
            return "null";
        }
        case ValueType::ARRAY: {
            JsonArray* arr = dynamic_cast<JsonArray*>(std::get<Object*>(value.as));
            if (arr) {
                std::ostringstream oss;
                oss << "[";
                bool first = true;
                for (const auto& elem : arr->elements) {
                    if (!first) {
                        oss << (pretty ? ",\n" + nextIndentStr : ",");
                    } else if (pretty) {
                        oss << "\n" + nextIndentStr;
                    }
                    oss << valueToJsonString(elem, pretty, indent + 1);
                    first = false;
                }
                if (pretty && !first) {
                    oss << "\n" + indentStr;
                }
                oss << "]";
                return oss.str();
            }
            return "null";
        }
        case ValueType::CLASS:
        case ValueType::INSTANCE:
        case ValueType::CALLABLE:
            return "null";
        case ValueType::MODULE:
            return "null";
    }
    return "null";
}

// Helper function to parse JSON string
Value parseJsonString(const std::string& json) {
    // This is a simplified JSON parser - in a real implementation,
    // you would want to use a proper JSON parsing library
    size_t pos = 0;
    
    // Skip whitespace
    while (pos < json.length() && isspace(json[pos])) {
        pos++;
    }
    
    if (pos >= json.length()) {
        throw std::runtime_error("Unexpected end of JSON input");
    }
    
    char c = json[pos];
    
    // Parse null
    if (json.substr(pos, 4) == "null") {
        return Value();
    }
    
    // Parse boolean
    if (json.substr(pos, 4) == "true") {
        return Value(true);
    }
    if (json.substr(pos, 5) == "false") {
        return Value(false);
    }
    
    // Parse number
    if (isdigit(c) || c == '-') {
        size_t start = pos;
        if (c == '-') pos++;
        while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E' || json[pos] == '+' || json[pos] == '-')) {
            pos++;
        }
        std::string numStr = json.substr(start, pos - start);
        try {
            return Value(std::stod(numStr));
        } catch (const std::exception&) {
            throw std::runtime_error("Invalid number format");
        }
    }
    
    // Parse string
    if (c == '"') {
        pos++;
        std::string str;
        while (pos < json.length() && json[pos] != '"') {
            if (json[pos] == '\\') {
                pos++;
                if (pos >= json.length()) {
                    throw std::runtime_error("Unexpected end of string");
                }
                switch (json[pos]) {
                    case '"':  str += '"';  break;
                    case '\\': str += '\\'; break;
                    case '/':  str += '/';  break;
                    case 'b':  str += '\b'; break;
                    case 'f':  str += '\f'; break;
                    case 'n':  str += '\n'; break;
                    case 'r':  str += '\r'; break;
                    case 't':  str += '\t'; break;
                    case 'u': {
                        // Unicode escape sequence - simplified handling
                        if (pos + 4 >= json.length()) {
                            throw std::runtime_error("Invalid unicode escape sequence");
                        }
                        pos += 4;
                        str += '?'; // Placeholder for actual unicode handling
                        break;
                    }
                    default:
                        str += json[pos];
                }
            } else {
                str += json[pos];
            }
            pos++;
        }
        if (pos >= json.length()) {
            throw std::runtime_error("Unterminated string");
        }
        pos++; // Skip closing quote
        return Value(str);
    }
    
    // Parse array
    if (c == '[') {
        pos++;
        auto jsonArray = new JsonArray();
        // Skip whitespace
        while (pos < json.length() && isspace(json[pos])) {
            pos++;
        }
        // Parse elements
        while (pos < json.length() && json[pos] != ']') {
            jsonArray->elements.push_back(parseJsonString(json.substr(pos)));
            // Skip to end of value
            // This is a simplified implementation - in practice you'd need a proper parser
            // For now, we'll just skip to the next comma or closing bracket
            while (pos < json.length() && json[pos] != ',' && json[pos] != ']') {
                pos++;
            }
            if (json[pos] == ',') {
                pos++;
                // Skip whitespace
                while (pos < json.length() && isspace(json[pos])) {
                    pos++;
                }
            }
        }
        if (pos >= json.length()) {
            throw std::runtime_error("Unterminated array");
        }
        pos++; // Skip closing bracket
        return Value(jsonArray);
    }
    
    // Parse object
    if (c == '{') {
        pos++;
        auto jsonObject = new JsonObject();
        // Skip whitespace
        while (pos < json.length() && isspace(json[pos])) {
            pos++;
        }
        // Parse key-value pairs
        while (pos < json.length() && json[pos] != '}') {
            // Parse key
            if (json[pos] != '"') {
                throw std::runtime_error("Expected string key");
            }
            Value keyVal = parseJsonString(json.substr(pos));
            std::string key = std::get<std::string>(keyVal.as);
            pos += 2; // Skip opening quote and some content (simplified)
            // Skip to colon
            while (pos < json.length() && json[pos] != ':') {
                pos++;
            }
            if (pos >= json.length()) {
                throw std::runtime_error("Expected colon");
            }
            pos++; // Skip colon
            // Skip whitespace
            while (pos < json.length() && isspace(json[pos])) {
                pos++;
            }
            // Parse value
            jsonObject->properties[key] = parseJsonString(json.substr(pos));
            // Skip to end of value (simplified)
            while (pos < json.length() && json[pos] != ',' && json[pos] != '}') {
                pos++;
            }
            if (json[pos] == ',') {
                pos++;
                // Skip whitespace
                while (pos < json.length() && isspace(json[pos])) {
                    pos++;
                }
            }
        }
        if (pos >= json.length()) {
            throw std::runtime_error("Unterminated object");
        }
        pos++; // Skip closing brace
        return Value(jsonObject);
    }
    
    throw std::runtime_error("Invalid JSON");
}

Value json_get(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for json.get().");
    }
    
    if (arguments[0].type != ValueType::OBJECT) {
        throw std::runtime_error("First argument for json.get() must be an object.");
    }
    
    if (arguments[1].type != ValueType::STRING) {
        throw std::runtime_error("Second argument for json.get() must be a string.");
    }
    
    JsonObject* obj = dynamic_cast<JsonObject*>(std::get<Object*>(arguments[0].as));
    if (!obj) {
        throw std::runtime_error("Invalid object type.");
    }
    
    std::string key = std::get<std::string>(arguments[1].as);
    auto it = obj->properties.find(key);
    if (it != obj->properties.end()) {
        return it->second;
    }
    
    return Value(); // Return null if key not found
}

namespace neutron {
    void register_json_functions(std::shared_ptr<Environment> env) {
        env->define("stringify", Value(new NativeFn(json_stringify, -1)));
        env->define("parse", Value(new NativeFn(json_parse, 1)));
        env->define("get", Value(new NativeFn(json_get, 2)));
    }
}

extern "C" void neutron_init_json_module(VM* vm) {
    auto json_env = std::make_shared<neutron::Environment>();
    neutron::register_json_functions(json_env);
    auto json_module = new neutron::Module("json", json_env);
    vm->define_module("json", json_module);
}
