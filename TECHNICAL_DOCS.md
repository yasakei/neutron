# Neutron Technical Documentation

## Core Fixes and Improvements

### 1. Stack Management in Control Flow (Critical Fix)

**Problem**: The `OP_JUMP_IF_FALSE` bytecode instruction was not properly managing the evaluation stack, causing function calls within control flow constructs (if/while/for) to return incorrect values.

**Root Cause**: The VM's `OP_JUMP_IF_FALSE` implementation was checking the truthiness of `stack.back()` without popping the condition value from the stack. This left the condition value on the stack, which would later be misinterpreted as a function return value.

**Original Code**:
```cpp
case (uint8_t)OpCode::OP_JUMP_IF_FALSE: {
    uint16_t offset = READ_SHORT();
    if (!isTruthy(stack.back())) {
        frame->ip += offset;
    }
    break;
}
```

**Fixed Code**:
```cpp
case (uint8_t)OpCode::OP_JUMP_IF_FALSE: {
    uint16_t offset = READ_SHORT();
    Value condition = pop();  // Pop the condition value
    if (!isTruthy(condition)) {
        frame->ip += offset;
    }
    break;
}
```

**Impact**: This fix resolves function call return value issues in:
- Functions called inside if statements
- Functions called inside while loops  
- Functions called inside for loops
- Method calls and any expressions within control flow constructs

### 2. Module Loading with Recursive Function Support

**Problem**: Functions defined in the same Neutron module could not call each other recursively because the module loading process cleared all global functions during execution, preventing functions from finding each other.

**Root Cause**: In the `load_module` function, the code did:
```cpp
// Save current globals
auto saved_globals = globals;

// Clear all globals - PROBLEMATIC
globals.clear();

// Execute module code (functions can't find each other)
Compiler compiler(*this);
Function* module_function = compiler.compile(statements);
if (module_function) {
    interpret(module_function);
    delete module_function;
}

// Copy results back
for (const auto& pair : globals) {
    module_env->define(pair.first, pair.second);
}

// Restore globals
globals = saved_globals;
```

**Fixed Approach**: Preserve essential built-in functions during module execution:
```cpp
// Save current globals
auto saved_globals = globals;

// Preserve essential functions while allowing module functions to be defined
std::unordered_map<std::string, Value> module_execution_globals = saved_globals;

// Execute module with essential functions available
globals = module_execution_globals;

// Execute module code (functions can now find each other)
Compiler compiler(*this);
Function* module_function = compiler.compile(statements);
if (module_function) {
    interpret(module_function);
    delete module_function;
}

// Copy results to module environment
for (const auto& pair : globals) {
    module_env->define(pair.first, pair.second);
}

// Restore original globals
globals = saved_globals;
```

**Impact**: This enables:
- Recursive function calls within the same module
- Functions in same module calling each other during execution
- Proper module loading with internal function dependencies

### 3. Dual Access Pattern for Built-in Modules

**Problem**: Built-in modules needed to support both global access (`str(42)`) and module access (`convert.str(42)`).

**Solution**: Modified VM constructor to register modules in both ways:
- Store built-in functions in global environment for backward compatibility
- Create separate module objects for module-style access
- Register both in the globals map

**Impact**: Users can choose between:
- `str(42)` for global function access
- `convert.str(42)` for module-style access

### 4. Enhanced Module Search Paths

**Problem**: User modules in different directories could not be found consistently.

**Solution**: Added comprehensive search paths:
- Current directory (".")
- Built-in library directory ("lib/")
- Box modules directory ("box/")

**Impact**: Better module resolution for:
- Local modules in same directory
- Standard library modules
- External box modules

## Testing Verification

All development tests have been verified to work:
- ✅ `feature_test.nt` - Core language features
- ✅ `module_test.nt` - Module system functionality
- ✅ `class_test.nt` - Object-oriented features  
- ✅ `binary_conversion_feature_test.nt` - Comprehensive feature test

## Architecture Impact

These changes maintain full backward compatibility while improving:
- Execution reliability in control flow
- Module loading robustness
- Function call accuracy
- Memory management safety