#include "native.h"
#include "vm.h"
#include <algorithm>
#include <random>
#include <functional>

namespace neutron {

// Helper function to get array from arguments
Array* getArrayFromArgs(const std::vector<Value>& arguments, int index = 0) {
    if (static_cast<int>(arguments.size()) <= index || arguments[index].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument at position " + std::to_string(index + 1));
    }
    return std::get<Array*>(arguments[index].as);
}

// Helper function to check if two values are equal
bool valuesEqual(const Value& a, const Value& b) {
    if (a.type != b.type) return false;
    
    switch (a.type) {
        case ValueType::NIL:
            return true;
        case ValueType::BOOLEAN:
            return std::get<bool>(a.as) == std::get<bool>(b.as);
        case ValueType::NUMBER:
            return std::get<double>(a.as) == std::get<double>(b.as);
        case ValueType::STRING:
            return std::get<std::string>(a.as) == std::get<std::string>(b.as);
        default:
            return false; // For complex types, we'll do shallow comparison
    }
}

Value native_arrays_create(std::vector<Value> arguments) {
    (void)arguments; // Suppress unused parameter warning
    // Create new empty array
    return Value(new Array());
}

Value native_arrays_length(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for length function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    return Value(static_cast<double>(arr->size()));
}

Value native_arrays_push(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected array and value arguments for push function");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("First argument must be an array");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    arr->push(arguments[1]);
    return Value(); // Return nil to indicate success
}

Value native_arrays_pop(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for pop function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    if (arr->size() == 0) {
        throw std::runtime_error("Cannot pop from empty array");
    }
    
    return arr->pop();
}

Value native_arrays_at(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::ARRAY || 
        arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected array and index arguments for at function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    int index = static_cast<int>(std::get<double>(arguments[1].as));
    
    if (index < 0 || index >= static_cast<int>(arr->size())) {
        std::string range = arr->size() == 0 ? "[]" : "[0, " + std::to_string(arr->size()-1) + "]";
        throw std::runtime_error("Array index out of bounds: index " + std::to_string(index) + 
                                " is not within " + range);
    }
    
    return arr->at(index);
}

Value native_arrays_set(std::vector<Value> arguments) {
    if (arguments.size() != 3 || arguments[0].type != ValueType::ARRAY || 
        arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected array, index, and value arguments for set function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    int index = static_cast<int>(std::get<double>(arguments[1].as));
    
    if (index < 0 || index >= static_cast<int>(arr->size())) {
        std::string range = arr->size() == 0 ? "[]" : "[0, " + std::to_string(arr->size()-1) + "]";
        throw std::runtime_error("Array index out of bounds: index " + std::to_string(index) + 
                                " is not within " + range);
    }
    
    arr->set(index, arguments[2]);
    return arguments[2]; // Return the set value
}

Value native_arrays_slice(std::vector<Value> arguments) {
    if (arguments.size() < 2 || arguments[0].type != ValueType::ARRAY || 
        arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected array, start index, and optional end index for slice function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    int start = static_cast<int>(std::get<double>(arguments[1].as));
    int end = (arguments.size() > 2 && arguments[2].type == ValueType::NUMBER) ? 
              static_cast<int>(std::get<double>(arguments[2].as)) : static_cast<int>(arr->size());
    
    if (start < 0) start = 0;
    if (end > static_cast<int>(arr->size())) end = arr->size();
    if (start > end) start = end;
    
    std::vector<Value> sliced;
    for (int i = start; i < end; i++) {
        sliced.push_back(arr->at(i));
    }
    
    return Value(new Array(sliced));
}

Value native_arrays_join(std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for join function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    std::string separator = (arguments.size() > 1) ? arguments[1].toString() : ",";
    
    std::string result;
    for (size_t i = 0; i < arr->size(); i++) {
        if (i > 0) result += separator;
        result += arr->at(i).toString();
    }
    
    return Value(result);
}

Value native_arrays_reverse(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for reverse function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    std::reverse(arr->elements.begin(), arr->elements.end());
    return arguments[0]; // Return the same array object for chaining
}

Value native_arrays_sort(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for sort function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    
    // Sort with mixed type handling: numbers first, then strings
    std::sort(arr->elements.begin(), arr->elements.end(), [](const Value& a, const Value& b) {
        if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) {
            return std::get<double>(a.as) < std::get<double>(b.as);
        } else if (a.type == ValueType::STRING && b.type == ValueType::STRING) {
            return std::get<std::string>(a.as) < std::get<std::string>(b.as);
        } else if (a.type == ValueType::NUMBER) {
            return true; // Numbers come before strings
        } else if (b.type == ValueType::NUMBER) {
            return false; // Strings come after numbers
        }
        return a.toString() < b.toString(); // Fallback comparison
    });
    
    return arguments[0]; // Return the same array object for chaining
}

Value native_arrays_index_of(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected array and value arguments for index_of function");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("First argument must be an array");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    Value target = arguments[1];
    
    for (size_t i = 0; i < arr->size(); i++) {
        if (valuesEqual(arr->at(i), target)) {
            return Value(static_cast<double>(i));
        }
    }
    
    return Value(-1.0); // Return -1 if not found
}

Value native_arrays_contains(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected array and value arguments for contains function");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("First argument must be an array");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    Value target = arguments[1];
    
    for (size_t i = 0; i < arr->size(); i++) {
        if (valuesEqual(arr->at(i), target)) {
            return Value(true);
        }
    }
    
    return Value(false);
}

Value native_arrays_remove(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected array and value arguments for remove function");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("First argument must be an array");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    Value target = arguments[1];
    
    for (auto it = arr->elements.begin(); it != arr->elements.end(); ++it) {
        if (valuesEqual(*it, target)) {
            arr->elements.erase(it);
            return Value(true);
        }
    }
    
    return Value(false);
}

Value native_arrays_remove_at(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::ARRAY || 
        arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected array and index arguments for remove_at function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    int index = static_cast<int>(std::get<double>(arguments[1].as));
    
    if (index < 0 || index >= static_cast<int>(arr->size())) {
        std::string range = arr->size() == 0 ? "[]" : "[0, " + std::to_string(arr->size()-1) + "]";
        throw std::runtime_error("Array index out of bounds: index " + std::to_string(index) + 
                                " is not within " + range);
    }
    
    Value removed = arr->at(index);
    arr->elements.erase(arr->elements.begin() + index);
    return removed;
}

Value native_arrays_clear(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for clear function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    arr->elements.clear();
    return arguments[0]; // Return the same array for chaining
}

Value native_arrays_clone(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for clone function");
    }
    
    Array* original = std::get<Array*>(arguments[0].as);
    return Value(new Array(original->elements)); // Shallow copy
}

Value native_arrays_to_string(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for to_string function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    std::string result = "[";
    
    for (size_t i = 0; i < arr->size(); i++) {
        if (i > 0) result += ", ";
        result += arr->at(i).toString();
    }
    
    result += "]";
    return Value(result);
}

Value native_arrays_flat(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for flat function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    std::vector<Value> result;
    
    for (size_t i = 0; i < arr->size(); i++) {
        Value current = arr->at(i);
        if (current.type == ValueType::ARRAY) {
            Array* nested = std::get<Array*>(current.as);
            result.insert(result.end(), nested->elements.begin(), nested->elements.end());
        } else {
            result.push_back(current);
        }
    }
    
    return Value(new Array(result));
}

Value native_arrays_fill(std::vector<Value> arguments) {
    if (arguments.size() < 2 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array and value arguments for fill function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    Value value = arguments[1];
    int start = (arguments.size() > 2) ? static_cast<int>(std::get<double>(arguments[2].as)) : 0;
    int end = (arguments.size() > 3) ? static_cast<int>(std::get<double>(arguments[3].as)) : static_cast<int>(arr->size());
    
    if (start < 0) start = 0;
    if (end > static_cast<int>(arr->size())) end = arr->size();
    
    for (int i = start; i < end; i++) {
        arr->elements[i] = value;
    }
    
    return arguments[0];
}

Value native_arrays_range(std::vector<Value> arguments) {
    if (arguments.size() < 1) {
        throw std::runtime_error("Expected start and optional end arguments for range function");
    }
    
    int start = static_cast<int>(std::get<double>(arguments[0].as));
    int end = (arguments.size() > 1) ? static_cast<int>(std::get<double>(arguments[1].as)) : start + 10; // Default to 10 elements if no end specified
    int step = (arguments.size() > 2) ? static_cast<int>(std::get<double>(arguments[2].as)) : 1;
    
    if (step == 0) step = 1; // Avoid infinite loop
    
    std::vector<Value> elements;
    if (step > 0) {
        for (int i = start; i < end; i += step) {
            elements.push_back(Value(static_cast<double>(i)));
        }
    } else {
        for (int i = start; i > end; i += step) {
            elements.push_back(Value(static_cast<double>(i)));
        }
    }
    
    return Value(new Array(elements));
}

Value native_arrays_shuffle(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for shuffle function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(arr->elements.begin(), arr->elements.end(), gen);
    
    return arguments[0]; // Return the same array for chaining
}

Value native_array_reverse(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for reverse function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    std::reverse(arr->elements.begin(), arr->elements.end());
    return arguments[0]; // Return the same array object for chaining
}

Value native_array_sort(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for sort function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    
    // Sort with mixed type handling: numbers first, then strings
    std::sort(arr->elements.begin(), arr->elements.end(), [](const Value& a, const Value& b) {
        if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) {
            return std::get<double>(a.as) < std::get<double>(b.as);
        } else if (a.type == ValueType::STRING && b.type == ValueType::STRING) {
            return std::get<std::string>(a.as) < std::get<std::string>(b.as);
        } else if (a.type == ValueType::NUMBER) {
            return true; // Numbers come before strings
        } else if (b.type == ValueType::NUMBER) {
            return false; // Strings come after numbers
        }
        return a.toString() < b.toString(); // Fallback comparison
    });
    
    return arguments[0]; // Return the same array object for chaining
}

Value native_array_index_of(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected array and value arguments for index_of function");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("First argument must be an array");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    Value target = arguments[1];
    
    for (size_t i = 0; i < arr->size(); i++) {
        if (valuesEqual(arr->at(i), target)) {
            return Value(static_cast<double>(i));
        }
    }
    
    return Value(-1.0); // Return -1 if not found
}

Value native_array_contains(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected array and value arguments for contains function");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("First argument must be an array");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    Value target = arguments[1];
    
    for (size_t i = 0; i < arr->size(); i++) {
        if (valuesEqual(arr->at(i), target)) {
            return Value(true);
        }
    }
    
    return Value(false);
}

Value native_array_remove(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected array and value arguments for remove function");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("First argument must be an array");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    Value target = arguments[1];
    
    for (auto it = arr->elements.begin(); it != arr->elements.end(); ++it) {
        if (valuesEqual(*it, target)) {
            arr->elements.erase(it);
            return Value(true);
        }
    }
    
    return Value(false);
}

Value native_array_remove_at(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::ARRAY || 
        arguments[1].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected array and index arguments for remove_at function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    int index = static_cast<int>(std::get<double>(arguments[1].as));
    
    if (index < 0 || index >= static_cast<int>(arr->size())) {
        std::string range = arr->size() == 0 ? "[]" : "[0, " + std::to_string(arr->size()-1) + "]";
        throw std::runtime_error("Array index out of bounds: index " + std::to_string(index) + 
                                " is not within " + range);
    }
    
    Value removed = arr->at(index);
    arr->elements.erase(arr->elements.begin() + index);
    return removed;
}

Value native_array_clear(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for clear function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    arr->elements.clear();
    return arguments[0]; // Return the same array for chaining
}

Value native_array_clone(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for clone function");
    }
    
    Array* original = std::get<Array*>(arguments[0].as);
    return Value(new Array(original->elements)); // Shallow copy
}

Value native_array_to_string(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for to_string function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    std::string result = "[";
    
    for (size_t i = 0; i < arr->size(); i++) {
        if (i > 0) result += ", ";
        result += arr->at(i).toString();
    }
    
    result += "]";
    return Value(result);
}

// Functional Programming Methods
Value native_array_map(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::ARRAY ||
        arguments[1].type != ValueType::CALLABLE) {
        throw std::runtime_error("Expected array and function arguments for map function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    Value func = arguments[1];
    
    std::vector<Value> result;
    for (size_t i = 0; i < arr->size(); i++) {
        // Call the function with the current element
        std::vector<Value> args = {arr->at(i), Value(static_cast<double>(i)), arguments[0]};
        // We'll need to call the function with the VM context, but for now return dummy
        // In a full implementation, we'd need VM context to call the function
        throw std::runtime_error("Map function requires VM context for function calling - not fully implemented in this version");
    }
    
    return Value(new Array(result));
}

Value native_array_filter(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::ARRAY ||
        arguments[1].type != ValueType::CALLABLE) {
        throw std::runtime_error("Expected array and function arguments for filter function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    Value func = arguments[1];
    
    std::vector<Value> result;
    for (size_t i = 0; i < arr->size(); i++) {
        // Call the function with the current element - simplified for now
        throw std::runtime_error("Filter function requires VM context for function calling - not fully implemented in this version");
    }
    
    return Value(new Array(result));
}

Value native_array_find(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::ARRAY ||
        arguments[1].type != ValueType::CALLABLE) {
        throw std::runtime_error("Expected array and function arguments for find function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    Value func = arguments[1];
    
    for (size_t i = 0; i < arr->size(); i++) {
        // Call the function with the current element - simplified for now
        throw std::runtime_error("Find function requires VM context for function calling - not fully implemented in this version");
    }
    
    return Value(); // Return nil if not found
}

Value native_array_reduce(std::vector<Value> arguments) {
    if (arguments.size() < 2 || arguments[0].type != ValueType::ARRAY ||
        arguments[1].type != ValueType::CALLABLE) {
        throw std::runtime_error("Expected array, function, and optional initial value for reduce function");
    }
    
    throw std::runtime_error("Reduce function requires VM context for function calling - not fully implemented in this version");
}

Value native_array_every(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::ARRAY ||
        arguments[1].type != ValueType::CALLABLE) {
        throw std::runtime_error("Expected array and function arguments for every function");
    }
    
    throw std::runtime_error("Every function requires VM context for function calling - not fully implemented in this version");
}

Value native_array_some(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::ARRAY ||
        arguments[1].type != ValueType::CALLABLE) {
        throw std::runtime_error("Expected array and function arguments for some function");
    }
    
    throw std::runtime_error("Some function requires VM context for function calling - not fully implemented in this version");
}

Value native_array_flat(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for flat function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    std::vector<Value> result;
    
    for (size_t i = 0; i < arr->size(); i++) {
        Value current = arr->at(i);
        if (current.type == ValueType::ARRAY) {
            Array* nested = std::get<Array*>(current.as);
            result.insert(result.end(), nested->elements.begin(), nested->elements.end());
        } else {
            result.push_back(current);
        }
    }
    
    return Value(new Array(result));
}

Value native_array_flat_map(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::ARRAY ||
        arguments[1].type != ValueType::CALLABLE) {
        throw std::runtime_error("Expected array and function arguments for flat_map function");
    }
    
    throw std::runtime_error("Flat_map function requires VM context for function calling - not fully implemented in this version");
}

// Utility Methods
Value native_array_fill(std::vector<Value> arguments) {
    if (arguments.size() < 2 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array and value arguments for fill function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    Value value = arguments[1];
    int start = (arguments.size() > 2) ? static_cast<int>(std::get<double>(arguments[2].as)) : 0;
    int end = (arguments.size() > 3) ? static_cast<int>(std::get<double>(arguments[3].as)) : static_cast<int>(arr->size());
    
    if (start < 0) start = 0;
    if (end > static_cast<int>(arr->size())) end = arr->size();
    
    for (int i = start; i < end; i++) {
        arr->elements[i] = value;
    }
    
    return arguments[0];
}

Value native_array_range(std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected start and optional end arguments for range function");
    }
    
    int start = static_cast<int>(std::get<double>(arguments[0].as));
    int end = (arguments.size() > 1) ? static_cast<int>(std::get<double>(arguments[1].as)) : start;
    int step = (arguments.size() > 2) ? static_cast<int>(std::get<double>(arguments[2].as)) : 1;
    
    if (step == 0) step = 1; // Avoid infinite loop
    
    std::vector<Value> elements;
    if (step > 0) {
        for (int i = start; i < end; i += step) {
            elements.push_back(Value(static_cast<double>(i)));
        }
    } else {
        for (int i = start; i > end; i += step) {
            elements.push_back(Value(static_cast<double>(i)));
        }
    }
    
    return Value(new Array(elements));
}

Value native_array_shuffle(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for shuffle function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(arr->elements.begin(), arr->elements.end(), gen);
    
    return arguments[0]; // Return the same array for chaining
}

void register_arrays_functions(std::shared_ptr<Environment> env) {
    env->define("new", Value(new NativeFn(native_arrays_create, 0)));           // arrays.new()
    env->define("length", Value(new NativeFn(native_arrays_length, 1)));     // arrays.length(arr)
    env->define("push", Value(new NativeFn(native_arrays_push, 2)));         // arrays.push(arr, value)
    env->define("pop", Value(new NativeFn(native_arrays_pop, 1)));           // arrays.pop(arr)
    env->define("at", Value(new NativeFn(native_arrays_at, 2)));             // arrays.at(arr, index)
    env->define("set", Value(new NativeFn(native_arrays_set, 3)));           // arrays.set(arr, index, value)
    env->define("slice", Value(new NativeFn(native_arrays_slice, -1)));       // arrays.slice(arr, start, [end])
    env->define("join", Value(new NativeFn(native_arrays_join, -1)));         // arrays.join(arr, [separator])
    env->define("reverse", Value(new NativeFn(native_arrays_reverse, 1)));   // arrays.reverse(arr)
    env->define("sort", Value(new NativeFn(native_arrays_sort, 1)));         // arrays.sort(arr)
    env->define("index_of", Value(new NativeFn(native_arrays_index_of, 2))); // arrays.index_of(arr, value)
    env->define("contains", Value(new NativeFn(native_arrays_contains, 2)));  // arrays.contains(arr, value)
    env->define("remove", Value(new NativeFn(native_arrays_remove, 2)));     // arrays.remove(arr, value)
    env->define("remove_at", Value(new NativeFn(native_arrays_remove_at, 2))); // arrays.remove_at(arr, index)
    env->define("clear", Value(new NativeFn(native_arrays_clear, 1)));       // arrays.clear(arr)
    env->define("clone", Value(new NativeFn(native_arrays_clone, 1)));       // arrays.clone(arr)
    env->define("to_string", Value(new NativeFn(native_arrays_to_string, 1))); // arrays.to_string(arr)
    env->define("flat", Value(new NativeFn(native_arrays_flat, 1)));         // arrays.flat(arr)
    env->define("fill", Value(new NativeFn(native_arrays_fill, -1)));         // arrays.fill(arr, value, [start, end])
    env->define("range", Value(new NativeFn(native_arrays_range, -1)));       // arrays.range(start, [end, step])
    env->define("shuffle", Value(new NativeFn(native_arrays_shuffle, 1)));   // arrays.shuffle(arr)
    
    // Methods that require function calling capability
    // Note: map, filter, find, reduce, every, some, flat_map would need full VM context
}

} // namespace neutron

extern "C" void neutron_init_arrays_module(neutron::VM* vm) {
    auto arrays_env = std::make_shared<neutron::Environment>();
    neutron::register_arrays_functions(arrays_env);
    auto arrays_module = new neutron::Module("arrays", arrays_env);
    vm->define_module("arrays", arrays_module);
}