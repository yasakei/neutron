#ifndef NEUTRON_VM_H
#define NEUTRON_VM_H

// Include all necessary type headers
#include "types/value.h"
#include "types/object.h"
#include "types/array.h"
#include "types/json_object.h"
#include "types/json_array.h"
#include "types/callable.h"
#include "types/function.h"
#include "types/native_fn.h"
#include "types/bound_method.h"
#include "types/class.h"
#include "types/instance.h"
#include "modules/module.h"
#include "runtime/environment.h"
#include "compiler/bytecode.h"

#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <string>

// Forward declarations only for types defined elsewhere
namespace neutron {
    class Stmt;
    class FunctionStmt;
    
    // Forward declarations for module registration functions
    void register_sys_functions(std::shared_ptr<Environment> env);
    void register_json_functions(std::shared_ptr<Environment> env);
    void register_convert_functions(std::shared_ptr<Environment> env);
    void register_time_functions(std::shared_ptr<Environment> env);
    void register_math_functions(std::shared_ptr<Environment> env);
}

namespace neutron {

struct CallFrame {
    Function* function;
    uint8_t* ip;
    size_t slot_offset;
    std::string fileName;  // Source file name for error reporting
    int currentLine;       // Current line number for error reporting
    
    CallFrame() : function(nullptr), ip(nullptr), slot_offset(0), fileName(""), currentLine(-1) {}
};

class Return {
public:
    Value value;
    Return(Value value);
};

class VM {
public:
    VM();
    void interpret(Function* function);
    void push(const Value& value);
    Value pop();
    void define_native(const std::string& name, Callable* function);
    void define_module(const std::string& name, Module* module);
    void define(const std::string& name, const Value& value);
    void load_module(const std::string& name);
    void load_file(const std::string& filepath);
    Value call(const Value& callee, const std::vector<Value>& arguments);
    Value execute_string(const std::string& source);
    void add_module_search_path(const std::string& path);
    
    // Memory management functions
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        T* obj = new T(std::forward<Args>(args)...);
        heap.push_back(obj);
        return obj;
    }

    // Public data members (for access from other components)
    std::vector<CallFrame> frames;
    Chunk* chunk;
    uint8_t* ip;
    std::vector<Value> stack;
    std::unordered_map<std::string, Value> globals;
    std::unordered_map<std::string, TokenType> globalTypes;  // Track type annotations for global variables
    std::vector<std::string> module_search_paths;
    std::vector<Object*> heap;
    size_t nextGC;
    std::vector<std::string> commandLineArgs;  // Store command line arguments
    std::string currentFileName;  // Current source file being executed
    std::vector<std::string> sourceLines;  // Source code lines for error reporting

private:
    bool call(Function* function, int argCount);
    bool callValue(Value callee, int argCount);
    bool callArrayMethod(class BoundArrayMethod* method, int argCount);
    void run(size_t minFrameDepth = 0);  // Run until frames.size() <= minFrameDepth (0 = run completely)
    void interpret_module(const std::vector<std::unique_ptr<Stmt>>& statements, std::shared_ptr<Environment> module_env);
    
    // Garbage collection methods
    void markRoots();
    void markValue(const Value& value);
    void collectGarbage();
    void sweep();
};

} // namespace neutron

#endif // NEUTRON_VM_H
