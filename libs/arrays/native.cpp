#include "native.h"
#include "vm.h"
#include "types/obj_string.h"
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
        case ValueType::OBJ_STRING:
            return std::get<ObjString*>(a.as)->chars == std::get<ObjString*>(b.as)->chars;
        default:
            return false; // For complex types, we'll do shallow comparison
    }
}

Value native_arrays_create(VM& vm, std::vector<Value> arguments) {
    (void)arguments; 
    return Value(vm.allocate<Array>());
}

Value native_arrays_length(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for length function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    return Value(static_cast<double>(arr->size()));
}

Value native_arrays_push(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected array and value arguments for push function");
    }
    
    if (arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("First argument must be an array");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    arr->push(arguments[1]);
    return Value(); // Return nil
}

Value native_arrays_pop(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for pop function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    if (arr->size() == 0) {
        throw std::runtime_error("Cannot pop from empty array");
    }
    
    return arr->pop();
}

Value native_arrays_at(VM& vm, std::vector<Value> arguments) {
    (void)vm;
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

Value native_arrays_set(VM& vm, std::vector<Value> arguments) {
    (void)vm;
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
    return arguments[2]; 
}

Value native_arrays_slice(VM& vm, std::vector<Value> arguments) {
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
    
    return Value(vm.allocate<Array>(sliced));
}

Value native_arrays_join(VM& vm, std::vector<Value> arguments) {
    (void)vm;
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
    
    return Value(vm.allocate<ObjString>(result));
}

Value native_arrays_reverse(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for reverse function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    std::reverse(arr->elements.begin(), arr->elements.end());
    return arguments[0]; 
}

Value native_arrays_sort(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for sort function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    
    std::sort(arr->elements.begin(), arr->elements.end(), [](const Value& a, const Value& b) {
        if (a.type == ValueType::NUMBER && b.type == ValueType::NUMBER) {
            return std::get<double>(a.as) < std::get<double>(b.as);
        } else if (a.type == ValueType::OBJ_STRING && b.type == ValueType::OBJ_STRING) {
            return std::get<ObjString*>(a.as)->chars < std::get<ObjString*>(b.as)->chars;
        } else if (a.type == ValueType::NUMBER) {
            return true; 
        } else if (b.type == ValueType::NUMBER) {
            return false; 
        }
        return a.toString() < b.toString(); 
    });
    
    return arguments[0]; 
}

Value native_arrays_index_of(VM& vm, std::vector<Value> arguments) {
    (void)vm;
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
    
    return Value(-1.0); 
}

Value native_arrays_contains(VM& vm, std::vector<Value> arguments) {
    (void)vm;
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

Value native_arrays_remove(VM& vm, std::vector<Value> arguments) {
    (void)vm;
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

Value native_arrays_remove_at(VM& vm, std::vector<Value> arguments) {
    (void)vm;
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

Value native_arrays_clear(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for clear function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    arr->elements.clear();
    return arguments[0]; 
}

Value native_arrays_clone(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for clone function");
    }
    
    Array* original = std::get<Array*>(arguments[0].as);
    return Value(vm.allocate<Array>(original->elements)); 
}

Value native_arrays_to_string(VM& vm, std::vector<Value> arguments) {
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
    return Value(vm.allocate<ObjString>(result));
}

Value native_arrays_flat(VM& vm, std::vector<Value> arguments) {
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
    
    return Value(vm.allocate<Array>(result));
}

Value native_arrays_fill(VM& vm, std::vector<Value> arguments) {
    (void)vm;
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

Value native_arrays_range(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() < 1) {
        throw std::runtime_error("Expected start and optional end arguments for range function");
    }
    
    int start = static_cast<int>(std::get<double>(arguments[0].as));
    int end = (arguments.size() > 1) ? static_cast<int>(std::get<double>(arguments[1].as)) : start + 10; 
    int step = (arguments.size() > 2) ? static_cast<int>(std::get<double>(arguments[2].as)) : 1;
    
    if (step == 0) step = 1; 
    
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
    
    return Value(vm.allocate<Array>(elements));
}

Value native_arrays_shuffle(VM& vm, std::vector<Value> arguments) {
    (void)vm;
    if (arguments.size() != 1 || arguments[0].type != ValueType::ARRAY) {
        throw std::runtime_error("Expected array argument for shuffle function");
    }
    
    Array* arr = std::get<Array*>(arguments[0].as);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(arr->elements.begin(), arr->elements.end(), gen);
    
    return arguments[0]; 
}

// Functional Programming Methods - Placeholders for now
Value native_arrays_map(VM& vm, std::vector<Value> arguments) {
    (void)vm; (void)arguments;
    throw std::runtime_error("Map function requires VM context for function calling - not fully implemented in this version");
}

Value native_arrays_filter(VM& vm, std::vector<Value> arguments) {
    (void)vm; (void)arguments;
    throw std::runtime_error("Filter function requires VM context for function calling - not fully implemented in this version");
}

Value native_arrays_find(VM& vm, std::vector<Value> arguments) {
    (void)vm; (void)arguments;
    throw std::runtime_error("Find function requires VM context for function calling - not fully implemented in this version");
}

Value native_arrays_reduce(VM& vm, std::vector<Value> arguments) {
    (void)vm; (void)arguments;
    throw std::runtime_error("Reduce function requires VM context for function calling - not fully implemented in this version");
}

Value native_arrays_every(VM& vm, std::vector<Value> arguments) {
    (void)vm; (void)arguments;
    throw std::runtime_error("Every function requires VM context for function calling - not fully implemented in this version");
}

Value native_arrays_some(VM& vm, std::vector<Value> arguments) {
    (void)vm; (void)arguments;
    throw std::runtime_error("Some function requires VM context for function calling - not fully implemented in this version");
}

Value native_arrays_flat_map(VM& vm, std::vector<Value> arguments) {
    (void)vm; (void)arguments;
    throw std::runtime_error("Flat_map function requires VM context for function calling - not fully implemented in this version");
}

void register_arrays_functions(VM& vm, std::shared_ptr<Environment> env) {
    env->define("new", Value(vm.allocate<NativeFn>(native_arrays_create, 0, true)));
    env->define("length", Value(vm.allocate<NativeFn>(native_arrays_length, 1, true)));
    env->define("push", Value(vm.allocate<NativeFn>(native_arrays_push, 2, true)));
    env->define("pop", Value(vm.allocate<NativeFn>(native_arrays_pop, 1, true)));
    env->define("at", Value(vm.allocate<NativeFn>(native_arrays_at, 2, true)));
    env->define("set", Value(vm.allocate<NativeFn>(native_arrays_set, 3, true)));
    env->define("slice", Value(vm.allocate<NativeFn>(native_arrays_slice, -1, true)));
    env->define("join", Value(vm.allocate<NativeFn>(native_arrays_join, -1, true)));
    env->define("reverse", Value(vm.allocate<NativeFn>(native_arrays_reverse, 1, true)));
    env->define("sort", Value(vm.allocate<NativeFn>(native_arrays_sort, 1, true)));
    env->define("index_of", Value(vm.allocate<NativeFn>(native_arrays_index_of, 2, true)));
    env->define("contains", Value(vm.allocate<NativeFn>(native_arrays_contains, 2, true)));
    env->define("remove", Value(vm.allocate<NativeFn>(native_arrays_remove, 2, true)));
    env->define("remove_at", Value(vm.allocate<NativeFn>(native_arrays_remove_at, 2, true)));
    env->define("clear", Value(vm.allocate<NativeFn>(native_arrays_clear, 1, true)));
    env->define("clone", Value(vm.allocate<NativeFn>(native_arrays_clone, 1, true)));
    env->define("to_string", Value(vm.allocate<NativeFn>(native_arrays_to_string, 1, true)));
    env->define("flat", Value(vm.allocate<NativeFn>(native_arrays_flat, 1, true)));
    env->define("fill", Value(vm.allocate<NativeFn>(native_arrays_fill, -1, true)));
    env->define("range", Value(vm.allocate<NativeFn>(native_arrays_range, -1, true)));
    env->define("shuffle", Value(vm.allocate<NativeFn>(native_arrays_shuffle, 1, true)));
    
    // Functional methods
    env->define("map", Value(vm.allocate<NativeFn>(native_arrays_map, 2, true)));
    env->define("filter", Value(vm.allocate<NativeFn>(native_arrays_filter, 2, true)));
    env->define("find", Value(vm.allocate<NativeFn>(native_arrays_find, 2, true)));
    env->define("reduce", Value(vm.allocate<NativeFn>(native_arrays_reduce, -1, true)));
    env->define("every", Value(vm.allocate<NativeFn>(native_arrays_every, 2, true)));
    env->define("some", Value(vm.allocate<NativeFn>(native_arrays_some, 2, true)));
    env->define("flat_map", Value(vm.allocate<NativeFn>(native_arrays_flat_map, 2, true)));
}

} // namespace neutron

extern "C" void neutron_init_arrays_module(neutron::VM* vm) {
    auto arrays_env = std::make_shared<neutron::Environment>();
    neutron::register_arrays_functions(*vm, arrays_env);
    auto arrays_module = vm->allocate<neutron::Module>("arrays", arrays_env);
    vm->define_module("arrays", arrays_module);
}
