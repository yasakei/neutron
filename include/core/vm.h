/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
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
#include "utils/component_interface.h"
#include "runtime/environment.h"
#include "compiler/bytecode.h"

#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <string>
#include <stack>
#include <utility>
#include <set>
#include <mutex>
#include <thread>

// Forward declarations only for types defined elsewhere
namespace neutron {
    class Stmt;
    class FunctionStmt;
    
    // Forward declarations for module registration functions
    void register_sys_functions(VM& vm, std::shared_ptr<Environment> env);
    void register_json_functions(VM& vm, std::shared_ptr<Environment> env);
    void register_convert_functions(VM& vm, std::shared_ptr<Environment> env);
    void register_time_functions(VM& vm, std::shared_ptr<Environment> env);
    void register_math_functions(VM& vm, std::shared_ptr<Environment> env);
    void register_arrays_functions(VM& vm, std::shared_ptr<Environment> env);
}

namespace neutron {

struct CallFrame {
    Function* function;
    uint8_t* ip;
    size_t slot_offset;
    std::string fileName;  // Source file name for error reporting
    int currentLine;       // Current line number for error reporting
    bool isBoundMethod;    // True if this is a method call (receiver at slot 0)
    bool isInitializer;    // True if this is a class initializer call
    
    CallFrame() : function(nullptr), ip(nullptr), slot_offset(0), fileName(""), currentLine(-1), isBoundMethod(false), isInitializer(false) {}
};

class Return {
public:
    Value value;
    Return(Value value);
};

class VM {
public:
    VM();
    ~VM();
    void interpret(Function* function);
    void push(const Value& value);
    Value pop();
    void define_native(const std::string& name, Callable* function);
    void define_module(const std::string& name, Module* module);
    void define(const std::string& name, const Value& value);
    void load_module(const std::string& name);
    void load_file(const std::string& filepath);
    Module* load_file_as_module(const std::string& filepath);
    Value call(const Value& callee, const std::vector<Value>& arguments);
    Value execute_string(const std::string& source);
    void add_module_search_path(const std::string& path);
    
    // Component management
    void registerComponent(std::shared_ptr<ComponentInterface> component);
    std::vector<std::shared_ptr<ComponentInterface>> getComponents() const;
    std::shared_ptr<ComponentInterface> getComponent(const std::string& name) const;
    
    // Memory management functions
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        T* obj = new T(std::forward<Args>(args)...);
        heap.push_back(obj);
        
        // Only check GC periodically to reduce overhead
        if (heap.size() >= nextGC) {
            tempRoots.push_back(obj);  // Protect during GC
            collectGarbage();
            if (!tempRoots.empty() && tempRoots.back() == obj) {
                tempRoots.pop_back();
            }
        }
        
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
    std::unordered_map<std::string, bool> loadedModuleCache;  // Cache for already loaded modules
    std::vector<Object*> heap;
    size_t nextGC;
    std::vector<std::string> commandLineArgs;  // Store command line arguments
    std::string currentFileName;  // Current source file being executed
    std::vector<std::string> sourceLines;  // Source code lines for error reporting
    std::vector<std::shared_ptr<ComponentInterface>> loadedComponents;  // Loaded components
    
    // String interning
    std::unordered_map<std::string, ObjString*> internedStrings;
    ObjString* internString(const std::string& str);
    
    // Fast string allocation (no interning - for data strings like concatenation results)
    ObjString* makeString(const std::string& str);
    ObjString* makeString(std::string&& str);

    // Temporary roots for garbage collection protection during allocation
    std::vector<Object*> tempRoots;

    // Embedded files support (for standalone executables)
    std::unordered_map<std::string, std::string> embeddedFiles;
    void addEmbeddedFile(const std::string& path, const std::string& content);
    
    // Exception handling support
    struct ExceptionFrame {
        int tryStart;         // Start of try block
        int tryEnd;           // End of try block
        int catchStart;       // Start of catch block (or -1 if no catch)
        int finallyStart;     // Start of finally block (or -1 if no finally)
        size_t frameBase;     // Stack frame base when exception frame was created
        std::string fileName;
        int line;
        
        ExceptionFrame() : tryStart(0), tryEnd(0), catchStart(-1), finallyStart(-1), frameBase(0), fileName(""), line(-1) {}

        ExceptionFrame(int tryStart, int tryEnd, int catchStart, int finallyStart, size_t frameBase, const std::string& fileName, int line)
            : tryStart(tryStart), tryEnd(tryEnd), catchStart(catchStart), finallyStart(finallyStart), 
              frameBase(frameBase), fileName(fileName), line(line) {}
    };
    
    std::vector<ExceptionFrame> exceptionFrames;  // Stack of exception frames
    bool hasException;  // Flag to indicate if an exception is currently being handled
    Value pendingException;  // The exception to be re-thrown after finally block
    
    // Static (immutable) variables - tracked in VM so they persist across REPL statements
    std::set<std::string> staticVariables;
    
    // Track declared global variables to prevent redeclaration in REPL
    std::set<std::string> declaredGlobals;
    
    // Flag to track if we're running a .ntsc (safe) file
    bool isSafeFile;

private:
    bool call(Function* function, int argCount);
    bool callValue(Value callee, int argCount);
    bool callArrayMethod(class BoundArrayMethod* method, int argCount);
    bool callStringMethod(class BoundStringMethod* method, int argCount);
    
    void run(size_t minFrameDepth = 0);  // Run until frames.size() <= minFrameDepth (0 = run completely)
    
public:
    // Expose callValue and run for native modules
    bool callValuePublic(Value callee, int argCount) {
        return callValue(callee, argCount);
    }
    
    void runPublic(size_t minFrameDepth = 0) {
        run(minFrameDepth);
    }

    // Thread safety
    std::mutex vm_mutex;
    std::thread::id owner_thread;
    int lock_count = 0;

    void lock();
    void unlock();
    int unlock_fully();
    void relock(int count);

private:
    void interpret_module(const std::vector<std::unique_ptr<Stmt>>& statements, std::shared_ptr<Environment> module_env);
    
    // Exception handling methods
    bool handleException(const Value& exception);
    
    // Garbage collection methods
    std::vector<Object*> grayStack;
    void markRoots();
    void markValue(const Value& value);
    void markObject(Object* obj);
    void traceReferences();
    void blackenObject(Object* obj);
    void collectGarbage();
    void sweep();
};

} // namespace neutron

#endif // NEUTRON_VM_H
