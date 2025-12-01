/*
 * Neutron Programming Language
 * Copyright (c) 2025 yasakei
 * 
 * This software is distributed under the Neutron Public License 1.0.
 * For full license text, see LICENSE file in the root directory.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "native.h"
#include "vm.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <fstream>

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

// Forward declaration
Value parseJsonValue(const std::string& json, size_t& pos);

void skipWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.length() && isspace(json[pos])) {
        pos++;
    }
}

Value parseJsonNull(const std::string& json, size_t& pos) {
    if (json.substr(pos, 4) == "null") {
        pos += 4;
        return Value();
    }
    throw std::runtime_error("Expected 'null'");
}

Value parseJsonBoolean(const std::string& json, size_t& pos) {
    if (json.substr(pos, 4) == "true") {
        pos += 4;
        return Value(true);
    }
    if (json.substr(pos, 5) == "false") {
        pos += 5;
        return Value(false);
    }
    throw std::runtime_error("Expected boolean");
}

Value parseJsonNumber(const std::string& json, size_t& pos) {
    size_t start = pos;
    if (json[pos] == '-') pos++;
    while (pos < json.length() && (isdigit(json[pos]) || json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E' || json[pos] == '+' || json[pos] == '-')) {
        pos++;
    }
    std::string numStr = json.substr(start, pos - start);
    try {
        return Value(std::stod(numStr));
    } catch (...) {
        throw std::runtime_error("Invalid number format");
    }
}

std::string parseJsonStringInternal(const std::string& json, size_t& pos) {
    if (json[pos] != '"') throw std::runtime_error("Expected string");
    pos++; // skip opening quote
    std::string str;
    while (pos < json.length()) {
        char c = json[pos];
        if (c == '"') {
            pos++; // skip closing quote
            return str;
        }
        if (c == '\\') {
            pos++;
            if (pos >= json.length()) throw std::runtime_error("Unexpected end of string");
            char escaped = json[pos];
            switch (escaped) {
                case '"': str += '"'; break;
                case '\\': str += '\\'; break;
                case '/': str += '/'; break;
                case 'b': str += '\b'; break;
                case 'f': str += '\f'; break;
                case 'n': str += '\n'; break;
                case 'r': str += '\r'; break;
                case 't': str += '\t'; break;
                case 'u': {
                    // Simplified unicode handling (just skip 4 chars)
                    if (pos + 4 >= json.length()) throw std::runtime_error("Invalid unicode");
                    pos += 4; 
                    str += '?'; 
                    break;
                }
                default: str += escaped; break;
            }
        } else {
            str += c;
        }
        pos++;
    }
    throw std::runtime_error("Unterminated string");
}

Value parseJsonArray(const std::string& json, size_t& pos) {
    pos++; // skip [
    auto arr = new Array();
    skipWhitespace(json, pos);
    if (pos < json.length() && json[pos] == ']') {
        pos++;
        return Value(arr);
    }
    
    while (pos < json.length()) {
        arr->push(parseJsonValue(json, pos));
        skipWhitespace(json, pos);
        if (pos < json.length() && json[pos] == ']') {
            pos++;
            return Value(arr);
        }
        if (pos < json.length() && json[pos] == ',') {
            pos++;
            skipWhitespace(json, pos);
        } else {
            throw std::runtime_error("Expected ',' or ']' in array");
        }
    }
    throw std::runtime_error("Unterminated array");
}

Value parseJsonObject(const std::string& json, size_t& pos) {
    pos++; // skip {
    auto obj = new JsonObject();
    skipWhitespace(json, pos);
    if (pos < json.length() && json[pos] == '}') {
        pos++;
        return Value(obj);
    }
    
    while (pos < json.length()) {
        if (json[pos] != '"') throw std::runtime_error("Expected string key");
        std::string key = parseJsonStringInternal(json, pos);
        skipWhitespace(json, pos);
        if (pos >= json.length() || json[pos] != ':') throw std::runtime_error("Expected ':'");
        pos++; // skip :
        skipWhitespace(json, pos);
        
        obj->properties[key] = parseJsonValue(json, pos);
        
        skipWhitespace(json, pos);
        if (pos < json.length() && json[pos] == '}') {
            pos++;
            return Value(obj);
        }
        if (pos < json.length() && json[pos] == ',') {
            pos++;
            skipWhitespace(json, pos);
        } else {
            throw std::runtime_error("Expected ',' or '}' in object");
        }
    }
    throw std::runtime_error("Unterminated object");
}

Value parseJsonValue(const std::string& json, size_t& pos) {
    skipWhitespace(json, pos);
    if (pos >= json.length()) throw std::runtime_error("Unexpected end of JSON");
    
    char c = json[pos];
    if (c == '{') return parseJsonObject(json, pos);
    if (c == '[') return parseJsonArray(json, pos);
    if (c == '"') return Value(parseJsonStringInternal(json, pos));
    if (c == 't' || c == 'f') return parseJsonBoolean(json, pos);
    if (c == 'n') return parseJsonNull(json, pos);
    if (isdigit(c) || c == '-') return parseJsonNumber(json, pos);
    
    throw std::runtime_error(std::string("Unexpected character: ") + c);
}

// Helper function to parse JSON string
Value parseJsonString(const std::string& json) {
    size_t pos = 0;
    return parseJsonValue(json, pos);
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

// JSON set - set a property on an object
Value json_set(std::vector<Value> arguments) {
    if (arguments.size() != 3) {
        throw std::runtime_error("Expected 3 arguments for json.set() (object, key, value).");
    }
    
    if (arguments[0].type != ValueType::OBJECT) {
        throw std::runtime_error("First argument for json.set() must be an object.");
    }
    
    if (arguments[1].type != ValueType::STRING) {
        throw std::runtime_error("Second argument for json.set() must be a string key.");
    }
    
    JsonObject* obj = dynamic_cast<JsonObject*>(std::get<Object*>(arguments[0].as));
    if (!obj) {
        throw std::runtime_error("Invalid object type.");
    }
    
    std::string key = std::get<std::string>(arguments[1].as);
    obj->properties[key] = arguments[2];
    
    return arguments[0]; // Return the modified object
}

// JSON has - check if object has a key
Value json_has(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for json.has() (object, key).");
    }
    
    if (arguments[0].type != ValueType::OBJECT) {
        throw std::runtime_error("First argument for json.has() must be an object.");
    }
    
    if (arguments[1].type != ValueType::STRING) {
        throw std::runtime_error("Second argument for json.has() must be a string key.");
    }
    
    JsonObject* obj = dynamic_cast<JsonObject*>(std::get<Object*>(arguments[0].as));
    if (!obj) {
        throw std::runtime_error("Invalid object type.");
    }
    
    std::string key = std::get<std::string>(arguments[1].as);
    return Value(obj->properties.find(key) != obj->properties.end());
}

// JSON keys - get all keys from an object
Value json_keys(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for json.keys().");
    }
    
    if (arguments[0].type != ValueType::OBJECT) {
        throw std::runtime_error("Argument for json.keys() must be an object.");
    }
    
    JsonObject* obj = dynamic_cast<JsonObject*>(std::get<Object*>(arguments[0].as));
    if (!obj) {
        throw std::runtime_error("Invalid object type.");
    }
    
    auto keysArray = new Array();
    for (const auto& pair : obj->properties) {
        keysArray->elements.push_back(Value(pair.first));
    }
    
    return Value(keysArray);
}

// JSON values - get all values from an object
Value json_values(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for json.values().");
    }
    
    if (arguments[0].type != ValueType::OBJECT) {
        throw std::runtime_error("Argument for json.values() must be an object.");
    }
    
    JsonObject* obj = dynamic_cast<JsonObject*>(std::get<Object*>(arguments[0].as));
    if (!obj) {
        throw std::runtime_error("Invalid object type.");
    }
    
    auto valuesArray = new Array();
    for (const auto& pair : obj->properties) {
        valuesArray->elements.push_back(pair.second);
    }
    
    return Value(valuesArray);
}

// JSON merge - merge two objects
Value json_merge(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for json.merge() (obj1, obj2).");
    }
    
    if (arguments[0].type != ValueType::OBJECT || arguments[1].type != ValueType::OBJECT) {
        throw std::runtime_error("Both arguments for json.merge() must be objects.");
    }
    
    JsonObject* obj1 = dynamic_cast<JsonObject*>(std::get<Object*>(arguments[0].as));
    JsonObject* obj2 = dynamic_cast<JsonObject*>(std::get<Object*>(arguments[1].as));
    
    if (!obj1 || !obj2) {
        throw std::runtime_error("Invalid object type.");
    }
    
    auto merged = new JsonObject();
    
    // Copy obj1 properties
    for (const auto& pair : obj1->properties) {
        merged->properties[pair.first] = pair.second;
    }
    
    // Copy obj2 properties (overwriting duplicates)
    for (const auto& pair : obj2->properties) {
        merged->properties[pair.first] = pair.second;
    }
    
    return Value(merged);
}

// JSON delete - remove a property from an object
Value json_delete(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for json.delete() (object, key).");
    }
    
    if (arguments[0].type != ValueType::OBJECT) {
        throw std::runtime_error("First argument for json.delete() must be an object.");
    }
    
    if (arguments[1].type != ValueType::STRING) {
        throw std::runtime_error("Second argument for json.delete() must be a string key.");
    }
    
    JsonObject* obj = dynamic_cast<JsonObject*>(std::get<Object*>(arguments[0].as));
    if (!obj) {
        throw std::runtime_error("Invalid object type.");
    }
    
    std::string key = std::get<std::string>(arguments[1].as);
    obj->properties.erase(key);
    
    return arguments[0]; // Return the modified object
}

// JSON clone - deep copy an object
Value json_clone(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for json.clone().");
    }
    
    // Convert to JSON string and parse it back (simple deep copy)
    std::string jsonStr = valueToJsonString(arguments[0], false);
    return parseJsonString(jsonStr);
}

// JSON read from file
Value json_readFile(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for json.readFile() (filepath).");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Argument for json.readFile() must be a string filepath.");
    }
    
    std::string filepath = std::get<std::string>(arguments[0].as);
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    std::string jsonContent = buffer.str();
    return parseJsonString(jsonContent);
}

// JSON write to file
Value json_writeFile(std::vector<Value> arguments) {
    if (arguments.size() < 2 || arguments.size() > 3) {
        throw std::runtime_error("Expected 2-3 arguments for json.writeFile() (filepath, data, pretty?).");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("First argument for json.writeFile() must be a string filepath.");
    }
    
    bool pretty = false;
    if (arguments.size() == 3 && arguments[2].type == ValueType::BOOLEAN) {
        pretty = std::get<bool>(arguments[2].as);
    }
    
    std::string filepath = std::get<std::string>(arguments[0].as);
    std::string jsonContent = valueToJsonString(arguments[1], pretty);
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filepath);
    }
    
    file << jsonContent;
    file.close();
    
    return Value(true);
}

namespace neutron {
    void register_json_functions(std::shared_ptr<Environment> env) {
        // Core functions
        env->define("stringify", Value(new NativeFn(json_stringify, -1)));
        env->define("parse", Value(new NativeFn(json_parse, 1)));
        
        // File operations
        env->define("readFile", Value(new NativeFn(json_readFile, 1)));
        env->define("writeFile", Value(new NativeFn(json_writeFile, -1)));
        
        // Object manipulation
        env->define("get", Value(new NativeFn(json_get, 2)));
        env->define("set", Value(new NativeFn(json_set, 3)));
        env->define("has", Value(new NativeFn(json_has, 2)));
        env->define("delete", Value(new NativeFn(json_delete, 2)));
        
        // Object introspection
        env->define("keys", Value(new NativeFn(json_keys, 1)));
        env->define("values", Value(new NativeFn(json_values, 1)));
        
        // Object utilities
        env->define("merge", Value(new NativeFn(json_merge, 2)));
        env->define("clone", Value(new NativeFn(json_clone, 1)));
    }
}

extern "C" void neutron_init_json_module(VM* vm) {
    auto json_env = std::make_shared<neutron::Environment>();
    neutron::register_json_functions(json_env);
    auto json_module = new neutron::Module("json", json_env);
    vm->define_module("json", json_module);
}
