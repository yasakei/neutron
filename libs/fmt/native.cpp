/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
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
#include "types/obj_string.h"
#include <bitset>
#include <sstream>

namespace neutron {

// Helper function to get type name for error messages
std::string getTypeName(const Value& val) {
    switch (val.type) {
        case ValueType::NIL:       return "nil";
        case ValueType::BOOLEAN:   return "bool";
        case ValueType::NUMBER:    return "number";
        case ValueType::OBJ_STRING:    return "string";
        case ValueType::CALLABLE:  return "function";
        case ValueType::OBJECT:    return "object";
        case ValueType::ARRAY:     return "array";
        case ValueType::MODULE:    return "module";
        default:                   return "unknown";
    }
}

Value native_fmt_to_int(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.to_int().");
    }
    
    Value val = arguments[0];
    
    switch (val.type) {
        case ValueType::NUMBER:
            // Already a number, just return as integer (truncate decimal part)
            return Value(static_cast<double>(static_cast<int>(val.as.number)));
        
        case ValueType::OBJ_STRING: {
            // Convert string to number
            std::string str = val.asString()->chars;
            
            // Handle empty string
            if (str.empty()) {
                return Value(0.0);
            }
            
            try {
                double result = std::stod(str);
                return Value(static_cast<double>(static_cast<int>(result)));
            } catch (const std::invalid_argument& ia) {
                throw std::runtime_error("Cannot convert string '" + str + "' to integer: invalid format.");
            } catch (const std::out_of_range& oor) {
                throw std::runtime_error("Cannot convert string '" + str + "' to integer: out of range.");
            }
        }
        
        case ValueType::BOOLEAN:
            // Convert bool to int (true = 1, false = 0)
            return Value(val.as.boolean ? 1.0 : 0.0);
            
        case ValueType::NIL:
            // nil converts to 0
            return Value(0.0);
        
        case ValueType::ARRAY:
        case ValueType::OBJECT:
        case ValueType::CALLABLE:
        case ValueType::MODULE:
        default:
            // Unsupported types throw error
            throw std::runtime_error("Cannot convert " + getTypeName(val) + " to integer.");
    }
}

Value native_fmt_to_str(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.to_str().");
    }
    
    Value val = arguments[0];
    
    switch (val.type) {
        case ValueType::NUMBER: {
            // Convert number to string
            char buffer[32];
            double num = val.as.number;
            snprintf(buffer, sizeof(buffer), "%.15g", num);
            return Value(vm.internString(std::string(buffer)));
        }
        
        case ValueType::OBJ_STRING:
            // Already a string, just return it
            return val;
            
        case ValueType::BOOLEAN:
            // Convert bool to string
            return Value(vm.internString(val.as.boolean ? "true" : "false"));
            
        case ValueType::NIL:
            // nil converts to "nil" string
            return Value(vm.internString("nil"));
            
        case ValueType::ARRAY:
        case ValueType::OBJECT:
        case ValueType::CALLABLE:
        case ValueType::MODULE:
        default:
            // For complex types, we can provide a string representation
            // but for now let's throw error as specified
            throw std::runtime_error("Cannot convert " + getTypeName(val) + " to string.");
    }
}

Value native_fmt_to_bin(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.to_bin().");
    }
    
    Value val = arguments[0];
    
    switch (val.type) {
        case ValueType::NUMBER: {
            // Convert number to binary string
            int num = static_cast<int>(val.as.number);
            // Remove leading zeros from the binary representation
            std::string bin = std::bitset<32>(num).to_string();
            // Find first '1' and return substring from there, or return "0" if all zeros
            size_t firstOne = bin.find('1');
            if (firstOne == std::string::npos) {
                return Value(vm.internString("0"));
            }
            return Value(vm.internString(bin.substr(firstOne)));
        }
        
        case ValueType::OBJ_STRING: {
            // If it's a string, first convert to number, then to binary
            std::string str = val.asString()->chars;
            
            if (str.empty()) {
                return Value(vm.allocate<ObjString>("0"));
            }
            
            try {
                double num = std::stod(str);
                int intNum = static_cast<int>(num);
                std::string bin = std::bitset<32>(intNum).to_string();
                size_t firstOne = bin.find('1');
                if (firstOne == std::string::npos) {
                    return Value(vm.allocate<ObjString>("0"));
                }
                return Value(vm.allocate<ObjString>(bin.substr(firstOne)));
            } catch (const std::invalid_argument& ia) {
                throw std::runtime_error("Cannot convert string '" + str + "' to binary: invalid format.");
            } catch (const std::out_of_range& oor) {
                throw std::runtime_error("Cannot convert string '" + str + "' to binary: out of range.");
            }
        }
        
        case ValueType::BOOLEAN:
            // Convert boolean to binary (true = 1, false = 0)
            return Value(vm.allocate<ObjString>(val.as.boolean ? "1" : "0"));
            
        case ValueType::NIL:
            // nil converts to "0" binary
            return Value(vm.allocate<ObjString>("0"));
        
        case ValueType::ARRAY:
        case ValueType::OBJECT:
        case ValueType::CALLABLE:
        case ValueType::MODULE:
        default:
            // Unsupported types throw error
            throw std::runtime_error("Cannot convert " + getTypeName(val) + " to binary.");
    }
}

Value native_fmt_type(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.type().");
    }
    
    Value val = arguments[0];
    std::string typeName;
    
    switch (val.type) {
        case ValueType::NIL:
            typeName = "nil";
            break;
        case ValueType::BOOLEAN:
            typeName = "bool";
            break;
        case ValueType::NUMBER:
            typeName = "number";
            break;
        case ValueType::OBJ_STRING:
            typeName = "string";
            break;
        case ValueType::CALLABLE:
            typeName = "function";
            break;
        case ValueType::OBJECT:
            typeName = "object";
            break;
        case ValueType::ARRAY:
            typeName = "array";
            break;
        case ValueType::MODULE:
            typeName = "module";
            break;
        default:
            typeName = "unknown";
            break;
    }
    
    return Value(vm.allocate<ObjString>(typeName));
}

Value native_fmt_to_float(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.to_float().");
    }
    
    Value val = arguments[0];
    
    switch (val.type) {
        case ValueType::NUMBER:
            // Already a number, return as is (could be int or float)
            return val;
        
        case ValueType::OBJ_STRING: {
            // Convert string to float
            std::string str = val.asString()->chars;
            
            // Handle empty string
            if (str.empty()) {
                return Value(0.0);
            }
            
            try {
                double result = std::stod(str);
                return Value(result);
            } catch (const std::invalid_argument& ia) {
                throw std::runtime_error("Cannot convert string '" + str + "' to float: invalid format.");
            } catch (const std::out_of_range& oor) {
                throw std::runtime_error("Cannot convert string '" + str + "' to float: out of range.");
            }
        }
        
        case ValueType::BOOLEAN:
            // Convert bool to float (true = 1.0, false = 0.0)
            return Value(val.as.boolean ? 1.0 : 0.0);
            
        case ValueType::NIL:
            // nil converts to 0.0
            return Value(0.0);
        
        case ValueType::ARRAY:
        case ValueType::OBJECT:
        case ValueType::CALLABLE:
        case ValueType::MODULE:
        default:
            // Unsupported types throw error
            throw std::runtime_error("Cannot convert " + getTypeName(val) + " to float.");
    }
}

Value native_fmt_is_int(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.is_int().");
    }
    Value val = arguments[0];
    if (val.type != ValueType::NUMBER) {
        return Value(false);
    }
    double d = val.as.number;
    return Value(d == static_cast<double>(static_cast<long long>(d)));
}

Value native_fmt_is_float(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.is_float().");
    }
    Value val = arguments[0];
    return Value(val.type == ValueType::NUMBER);
}

Value native_fmt_is_string(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.is_string().");
    }
    Value val = arguments[0];
    return Value(val.type == ValueType::OBJ_STRING);
}

Value native_fmt_is_bool(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.is_bool().");
    }
    Value val = arguments[0];
    return Value(val.type == ValueType::BOOLEAN);
}

Value native_fmt_is_nil(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.is_nil().");
    }
    Value val = arguments[0];
    return Value(val.type == ValueType::NIL);
}

Value native_fmt_is_array(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.is_array().");
    }
    Value val = arguments[0];
    return Value(val.type == ValueType::ARRAY);
}

Value native_fmt_is_object(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.is_object().");
    }
    Value val = arguments[0];
    return Value(val.type == ValueType::OBJECT);
}

Value native_fmt_is_callable(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.is_callable().");
    }
    Value val = arguments[0];
    return Value(val.type == ValueType::CALLABLE);
}

Value native_fmt_to_hex(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.to_hex().");
    }
    Value val = arguments[0];
    if (val.type != ValueType::NUMBER) {
        throw std::runtime_error("fmt.to_hex() expects a number.");
    }
    long long n = static_cast<long long>(val.as.number);
    if (n < 0) {
        throw std::runtime_error("fmt.to_hex() expects a non-negative number.");
    }
    std::stringstream ss;
    ss << std::hex << n;
    return Value(vm.allocate<neutron::ObjString>(ss.str()));
}

Value native_fmt_to_oct(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected one argument for fmt.to_oct().");
    }
    Value val = arguments[0];
    if (val.type != ValueType::NUMBER) {
        throw std::runtime_error("fmt.to_oct() expects a number.");
    }
    long long n = static_cast<long long>(val.as.number);
    if (n < 0) {
        throw std::runtime_error("fmt.to_oct() expects a non-negative number.");
    }
    std::stringstream ss;
    ss << std::oct << n;
    return Value(vm.allocate<neutron::ObjString>(ss.str()));
}

Value native_fmt_pad_left(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() < 2 || arguments.size() > 3) {
        throw std::runtime_error("fmt.pad_left() expects 2 or 3 arguments.");
    }
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("fmt.pad_left() expects string as first argument.");
    }
    if (arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("fmt.pad_left() expects number as second argument.");
    }
    std::string str = arguments[0].asString()->chars;
    int width = static_cast<int>(arguments[1].as.number);
    std::string pad = " ";
    if (arguments.size() == 3 && arguments[2].type == ValueType::OBJ_STRING) {
        pad = arguments[2].asString()->chars;
    }
    if (static_cast<int>(str.length()) >= width) {
        return Value(vm.allocate<neutron::ObjString>(str));
    }
    int padCount = width - static_cast<int>(str.length());
    std::string result;
    for (int i = 0; i < padCount; i++) {
        result += pad;
    }
    result += str;
    return Value(vm.allocate<neutron::ObjString>(result));
}

Value native_fmt_pad_right(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() < 2 || arguments.size() > 3) {
        throw std::runtime_error("fmt.pad_right() expects 2 or 3 arguments.");
    }
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("fmt.pad_right() expects string as first argument.");
    }
    if (arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("fmt.pad_right() expects number as second argument.");
    }
    std::string str = arguments[0].asString()->chars;
    int width = static_cast<int>(arguments[1].as.number);
    std::string pad = " ";
    if (arguments.size() == 3 && arguments[2].type == ValueType::OBJ_STRING) {
        pad = arguments[2].asString()->chars;
    }
    if (static_cast<int>(str.length()) >= width) {
        return Value(vm.allocate<neutron::ObjString>(str));
    }
    int padCount = width - static_cast<int>(str.length());
    std::string result = str;
    for (int i = 0; i < padCount; i++) {
        result += pad;
    }
    return Value(vm.allocate<neutron::ObjString>(result));
}

void register_fmt_functions(VM& vm, std::shared_ptr<Environment> env) {
    env->define("to_int", Value(vm.allocate<NativeFn>(native_fmt_to_int, 1, true)));    // Dynamic int conversion
    env->define("to_str", Value(vm.allocate<NativeFn>(native_fmt_to_str, 1, true)));    // Dynamic string conversion
    env->define("to_bin", Value(vm.allocate<NativeFn>(native_fmt_to_bin, 1, true)));    // Dynamic binary conversion
    env->define("to_float", Value(vm.allocate<NativeFn>(native_fmt_to_float, 1, true))); // Dynamic float conversion
    env->define("type", Value(vm.allocate<NativeFn>(native_fmt_type, 1, true)));        // Type detection function
    env->define("is_int", Value(vm.allocate<NativeFn>(native_fmt_is_int, 1, true)));
    env->define("is_float", Value(vm.allocate<NativeFn>(native_fmt_is_float, 1, true)));
    env->define("is_string", Value(vm.allocate<NativeFn>(native_fmt_is_string, 1, true)));
    env->define("is_bool", Value(vm.allocate<NativeFn>(native_fmt_is_bool, 1, true)));
    env->define("is_nil", Value(vm.allocate<NativeFn>(native_fmt_is_nil, 1, true)));
    env->define("is_array", Value(vm.allocate<NativeFn>(native_fmt_is_array, 1, true)));
    env->define("is_object", Value(vm.allocate<NativeFn>(native_fmt_is_object, 1, true)));
    env->define("is_callable", Value(vm.allocate<NativeFn>(native_fmt_is_callable, 1, true)));
    env->define("to_hex", Value(vm.allocate<NativeFn>(native_fmt_to_hex, 1, true)));
    env->define("to_oct", Value(vm.allocate<NativeFn>(native_fmt_to_oct, 1, true)));
    env->define("pad_left", Value(vm.allocate<NativeFn>(native_fmt_pad_left, -1, true)));
    env->define("pad_right", Value(vm.allocate<NativeFn>(native_fmt_pad_right, -1, true)));
}

} // namespace neutron

extern "C" void neutron_init_fmt_module(neutron::VM* vm) {
    auto fmt_env = std::make_shared<neutron::Environment>();
    neutron::register_fmt_functions(*vm, fmt_env);
    auto fmt_module = vm->allocate<neutron::Module>("fmt", fmt_env);
    vm->define_module("fmt", fmt_module);
}
