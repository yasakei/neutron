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

/*
 * Code Documentation: Virtual Machine (vm.h)
 * ==========================================
 * 
 * This header defines the heart of Neutron - the Virtual Machine that executes
 * bytecode. Think of it as the engine room where all the magic (and occasionally
 * the mayhem) happens.
 * 
 * What This File Includes:
 * ------------------------
 * - CallFrame structure: Tracks execution context for function calls
 * - VM class: The main virtual machine implementation
 * - Memory management: Garbage collection, object allocation, instance pooling
 * - JIT compilation support: Multi-tier JIT with inline caching and OSR
 * - Exception handling: Stack unwinding and exception frame management
 * - Module loading: Dynamic module support and search path management
 * - Component system: Plugin architecture for extensibility
 * 
 * How It Works:
 * -------------
 * The VM uses a stack-based execution model. Bytecode instructions manipulate
 * values on the operand stack, while call frames track function invocation
 * context. The garbage collector uses a mark-and-sweep algorithm with periodic
 * collection triggered by heap growth thresholds.
 * 
 * Adding Features:
 * ----------------
 * - New opcodes: Add to bytecode.h, then implement in vm.cpp's run loop
 * - New value types: Extend ValueType enum and Value union
 * - Native functions: Use define_native() and follow the native function pattern
 * - JIT optimizations: Extend jitManager with new trace compilation strategies
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT modify the stack directly during execution (use push/pop)
 * - Do NOT bypass the garbage collector when allocating objects
 * - Do NOT call VM methods from multiple threads without proper locking
 * - Do NOT modify exceptionFrames without understanding stack unwinding
 * - Do NOT disable JIT mid-execution without resetting jitManager state
 * 
 * Performance Notes:
 * ------------------
 * - INSTANCE_POOL_MAX controls object pooling trade-offs (memory vs. allocation speed)
 * - JIT_LOOP_CACHE_SIZE affects inline cache hit rates
 * - nextGC threshold impacts GC frequency and pause times
 */

#ifndef NEUTRON_VM_H
#define NEUTRON_VM_H

// Platform-specific performance hints
// MSVC gets special treatment because it thinks it's too cool for standard attributes
#ifdef _MSC_VER
#define NEUTRON_FORCEINLINE __forceinline
#define NEUTRON_NOINLINE __declspec(noinline)
#define NEUTRON_LIKELY(x) (x)
#define NEUTRON_UNLIKELY(x) (x)
#else
#define NEUTRON_FORCEINLINE inline __attribute__((always_inline))
#define NEUTRON_NOINLINE __attribute__((noinline))
#define NEUTRON_LIKELY(x) __builtin_expect(!!(x), 1)
#define NEUTRON_UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

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
#include "jit/jit_manager.h"

#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <string>
#include <stack>
#include <utility>
#include <set>
#include <mutex>
#ifdef _WIN32
#include <process.h>  // Required for std::thread on Windows (provides _beginthreadex)
#endif
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

/**
 * @brief CallFrame - The VM's way of remembering where it was before that function call interrupted things.
 * 
 * Each call frame represents a single function invocation on the call stack.
 * It tracks the instruction pointer, stack slot offset, and source location
 * for error reporting. When exceptions occur, frames are unwound to find
 * appropriate handlers.
 */
struct CallFrame {
    Function* function;        ///< The function being executed
    uint8_t* ip;               ///< Instruction pointer - points to current bytecode
    size_t slot_offset;        ///< Stack slot offset where this frame's locals begin
    const std::string* fileName;  ///< Pointer to source file name (avoids string copy per call)
    int currentLine;           ///< Current line number for error reporting (because stack traces are useful)
    bool isBoundMethod;        ///< True if this is a method call (receiver at slot 0)
    bool isInitializer;        ///< True if this is a class initializer call (special handling for 'this')

    CallFrame() : function(nullptr), ip(nullptr), slot_offset(0), fileName(nullptr), currentLine(-1), isBoundMethod(false), isInitializer(false) {}

    /**
     * @brief Get the filename safely - returns empty string if no file is set.
     * @return Reference to the filename string, or an empty string if unavailable.
     */
    const std::string& getFileName() const {
        static const std::string empty;
        return fileName ? *fileName : empty;
    }
};

/**
 * @brief Return - A wrapper for return values, because C++ exceptions are too mainstream.
 * @param value The value being returned from a function.
 */
class Return {
public:
    Value value;
    Return(Value value);
};

/**
 * @brief VM - The Virtual Machine: Where bytecode comes to live (and sometimes die).
 * 
 * The VM class is the central execution engine for Neutron. It interprets bytecode,
 * manages memory via garbage collection, handles exceptions, and coordinates JIT
 * compilation. If Neutron were a car, this would be the engine, transmission, and
 * driver all rolled into one.
 * 
 * Key Responsibilities:
 * - Bytecode interpretation via stack-based execution
 * - Memory management with mark-and-sweep garbage collection
 * - Function calls, method dispatch, and closure support
 * - Exception handling with try-catch-finally semantics
 * - Module loading and dynamic library management
 * - JIT compilation coordination (when the hardware cooperates)
 * - Component/plugin system for extensibility
 * 
 * Thread Safety:
 * The VM is NOT thread-safe by default. Use lock()/unlock() for multi-threaded
 * scenarios, or better yet, don't share VM instances across threads.
 */
class VM {
public:
    VM();
    ~VM();
    
    /**
     * @brief Begin interpreting a function.
     * @param function The function to execute.
     */
    void interpret(Function* function);
    
    /**
     * @brief Push a value onto the operand stack.
     * @param value The value to push.
     */
    void push(const Value& value);
    
    /**
     * @brief Pop a value from the operand stack.
     * @return The popped value.
     */
    Value pop();
    
    /**
     * @brief Define a native function in the global scope.
     * @param name The name to register.
     * @param function The native callable to register.
     */
    void define_native(const std::string& name, Callable* function);
    
    /**
     * @brief Define a module in the global scope.
     * @param name The module name.
     * @param module The module instance.
     */
    void define_module(const std::string& name, Module* module);
    
    /**
     * @brief Define a global variable.
     * @param name The variable name.
     * @param value The initial value.
     */
    void define(const std::string& name, const Value& value);
    
    /**
     * @brief Load and execute a module by name.
     * @param name The module name to load.
     */
    void load_module(const std::string& name);
    
    /**
     * @brief Load and execute a file.
     * @param filepath Path to the Neutron source file.
     */
    void load_file(const std::string& filepath);
    
    /**
     * @brief Load a file as a module (for import statements).
     * @param filepath Path to the Neutron source file.
     * @return The loaded module, or nullptr on failure.
     */
    Module* load_file_as_module(const std::string& filepath);
    
    /**
     * @brief Call a callable value with arguments.
     * @param callee The function/method to call.
     * @param arguments Arguments to pass.
     * @return The return value (or NIL if none).
     */
    Value call(const Value& callee, const std::vector<Value>& arguments);
    
    /**
     * @brief Execute Neutron source code directly from a string.
     * @param source The source code to execute.
     * @return The result of execution (typically NIL).
     */
    Value execute_string(const std::string& source);
    
    /**
     * @brief Add a directory to the module search path.
     * @param path Directory path to search for modules.
     */
    void add_module_search_path(const std::string& path);

    // Component management - because why not let users plug things in?
    /**
     * @brief Register a component (plugin) with the VM.
     * @param component The component to register.
     */
    void registerComponent(std::shared_ptr<ComponentInterface> component);
    
    /**
     * @brief Get all registered components.
     * @return Vector of component shared pointers.
     */
    std::vector<std::shared_ptr<ComponentInterface>> getComponents() const;
    
    /**
     * @brief Get a component by name.
     * @param name The component name to find.
     * @return The component, or nullptr if not found.
     */
    std::shared_ptr<ComponentInterface> getComponent(const std::string& name) const;

    /**
     * @brief Allocate an object on the heap with automatic GC tracking.
     * 
     * This is the primary allocation method for all GC-managed objects.
     * Objects are automatically tracked and collected when no longer reachable.
     * 
     * @tparam T The object type to allocate.
     * @tparam Args Constructor argument types.
     * @param args Constructor arguments.
     * @return Pointer to the newly allocated object.
     */
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        T* obj = new T(std::forward<Args>(args)...);
        heap.push_back(obj);

        // Only check GC periodically to reduce overhead
        // Like checking your oil - only necessary once in a while
        if (heap.size() >= nextGC) {
            tempRoots.push_back(obj);  // Protect during GC
            collectGarbage();
            if (!tempRoots.empty() && tempRoots.back() == obj) {
                tempRoots.pop_back();
            }
        }

        return obj;
    }

    /**
     * @brief Fast Instance allocation with free-list pool.
     * 
     * Instances (class objects) are allocated from a pool for performance.
     * This avoids frequent allocations/deallocations for commonly used types.
     * 
     * @param klass The class type for the instance.
     * @return Pointer to the allocated instance.
     */
    Instance* allocateInstance(Class* klass);
    
    /**
     * @brief Return an instance to the pool for reuse.
     * @param inst The instance to free.
     */
    void freeInstance(Instance* inst);
    
    /// Maximum instances to keep in the pool (memory vs. performance trade-off)
    static constexpr size_t INSTANCE_POOL_MAX = 1024;
    std::vector<Instance*> instancePool;  ///< Instance free-list pool

    // JIT compilation
    // The JIT manager: because interpreting the same loop 10,000 times is masochistic
    jit::MultiTierJITManager jitManager;
    
    /**
     * @brief JIT enabled flag - controlled at runtime based on platform support.
     * 
     * JIT compilation is only available on x86_64 and ARM64 platforms.
     * On other architectures, this will be false and code runs in interpreter mode.
     */
#if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__) || defined(__aarch64__) || defined(__arm64__)
    bool jitEnabled = true;
#else
    bool jitEnabled = false;
#endif
    uint32_t jitLoopCounter = 0;  ///< Tracks loop iterations for JIT promotion decisions

    /**
     * @brief Inline cache for JIT-compiled loop traces.
     * 
     * Maps loop program counters to trace function pointers for O(1) dispatch.
     * This is essentially a "hot loop" cache - when we see the same loop again,
     * we jump straight to the compiled version instead of interpreting.
     * 
     * Size is kept small (32 entries) to balance hit rate against cache locality.
     */
    struct JITLoopCacheEntry {
        uint64_t loop_pc = 0;       ///< Program counter of the loop header
        uint64_t method_id = 0;     ///< ID of the method containing the loop
        uint64_t trace_id = 0;      ///< ID of the compiled trace
    };
    static constexpr size_t JIT_LOOP_CACHE_SIZE = 32;  ///< Cache size - small enough for L1, big enough to be useful
    JITLoopCacheEntry jitLoopCache[JIT_LOOP_CACHE_SIZE] = {};

    /**
     * @brief OSR (On-Stack Replacement) support.
     * 
     * OSR allows transitioning from interpreted to JIT-compiled code mid-execution.
     * This is crucial for long-running loops that are already executing when JIT
     * compilation completes.
     * 
     * Think of it as switching from a bicycle to a motorcycle without stopping.
     */
    struct OSREntry {
        uint64_t trace_id;           ///< JIT trace ID
        uint64_t bytecode_pc;        ///< Bytecode offset where OSR can occur
        uint32_t frame_size;         ///< Stack frame size at OSR point
        uint32_t locals_count;       ///< Number of live locals at OSR point
        std::vector<uint32_t> local_slots;  ///< Local variable slots to restore
    };
    std::unordered_map<uint64_t, std::vector<OSREntry>> osrEntries;  ///< method_id -> OSR entries

    /**
     * @brief Deoptimization support - when the JIT's assumptions prove wrong.
     * 
     * Guards track type specializations. When a guard fails (e.g., a variable
     * changes type), we deoptimize back to interpreted code and recompile with
     * more general assumptions.
     * 
     * It's like realizing your shortcut only works on Tuesdays.
     */
    struct JITGuard {
        uint64_t trace_id;
        uint32_t slot_index;
        uint8_t expected_type;  ///< ValueType enum - the type we optimized for
        bool is_active;
    };
    std::vector<JITGuard> jitGuards;  ///< Active guards for deoptimization
    bool hasPendingDeoptimization = false;  ///< Flag indicating deopt is pending
    uint64_t deoptTraceId = 0;  ///< Trace ID to deoptimize

    // JIT statistics - for when you need to prove the JIT is actually doing something
    uint64_t jitTracesCompiled = 0;       ///< Number of traces compiled
    uint64_t jitTracesExecuted = 0;       ///< Number of trace executions
    uint64_t jitGuardFailures = 0;        ///< Number of guard failures (deopts)
    uint64_t osrTransitions = 0;          ///< Number of OSR transitions

    // Public data members (for access from other components)
    // Yes, these are public. Encapsulation is important, but so is performance.
    std::vector<CallFrame> frames;           ///< Call stack - tracks active function calls
    Chunk* chunk;                             ///< Current bytecode chunk being executed
    uint8_t* ip;                              ///< Current instruction pointer
    std::vector<Value> stack;                 ///< Operand stack - where values live during execution
    std::unordered_map<std::string, Value> globals;  ///< Global variables
    std::unordered_map<std::string, TokenType> globalTypes;  ///< Type annotations for globals (for type checking)
    std::vector<std::string> module_search_paths;  ///< Directories to search for modules
    std::unordered_map<std::string, bool> loadedModuleCache;  ///< Cache for already loaded modules (avoid reloading)
    std::vector<Object*> heap;                ///< GC heap - all allocated objects
    size_t nextGC;                            ///< Heap size threshold that triggers next GC
    std::vector<std::string> commandLineArgs; ///< Command line arguments passed to the script
    std::string currentFileName;              ///< Current source file being executed
    std::vector<std::string> sourceLines;     ///< Source code lines for error reporting (for stack traces)
    std::vector<std::shared_ptr<ComponentInterface>> loadedComponents;  ///< Loaded components

    // String interning - because allocating the same "hello" 1000 times is wasteful
    std::unordered_map<std::string, ObjString*> internedStrings;
    
    /**
     * @brief Intern a string - returns a canonical copy for identical strings.
     * @param str The string to intern.
     * @return Pointer to the interned string object.
     */
    ObjString* internString(const std::string& str);

    /**
     * @brief Make a new string without interning (for data strings).
     * 
     * Use this when you need a string that won't be compared by identity,
     * such as concatenation results or user input.
     * 
     * @param str The string content.
     * @return Pointer to the new string object.
     */
    ObjString* makeString(const std::string& str);
    
    /**
     * @brief Make a new string without interning (move version).
     * @param str The string content (moved).
     * @return Pointer to the new string object.
     */
    ObjString* makeString(std::string&& str);

    // Temporary roots for garbage collection protection during allocation
    // These prevent objects from being collected while they're still in use
    std::vector<Object*> tempRoots;

    // Embedded files support (for standalone executables)
    // Allows bundling source files directly into the executable
    std::unordered_map<std::string, std::string> embeddedFiles;
    
    /**
     * @brief Add an embedded file to the VM.
     * @param path The file path (key).
     * @param content The file content.
     */
    void addEmbeddedFile(const std::string& path, const std::string& content);

    // JIT control methods
    void setJITEnabled(bool enabled);
    bool isJITEnabled() const { return jitEnabled; }
    void setJITMonitoring(bool enabled);

    // OSR and deoptimization support
    void registerOSREntry(uint64_t method_id, uint64_t trace_id, uint64_t bytecode_pc);
    void triggerDeoptimization(uint64_t trace_id);
    bool performDeoptimization(CallFrame* frame);

    // JIT statistics accessors
    uint64_t getJITTracesCompiled() const { return jitTracesCompiled; }
    uint64_t getJITTracesExecuted() const { return jitTracesExecuted; }
    uint64_t getJITGuardFailures() const { return jitGuardFailures; }
    void printJITStatistics() const;
    
    /**
     * @brief Exception handling support - because errors happen (especially in production).
     * 
     * ExceptionFrame tracks try-catch-finally blocks during execution.
     * When an exception occurs, frames are searched to find handlers.
     * The stack is unwound to the handler's frame, and execution resumes.
     */
    struct ExceptionFrame {
        int tryStart;         ///< Start offset of try block in bytecode
        int tryEnd;           ///< End offset of try block in bytecode
        int catchStart;       ///< Start offset of catch block (-1 if no catch)
        int finallyStart;     ///< Start offset of finally block (-1 if no finally)
        size_t frameBase;     ///< Stack frame base when exception frame was created
        std::string fileName; ///< Source file name for debugging
        int line;             ///< Source line number for debugging

        ExceptionFrame() : tryStart(0), tryEnd(0), catchStart(-1), finallyStart(-1), frameBase(0), fileName(""), line(-1) {}

        ExceptionFrame(int tryStart, int tryEnd, int catchStart, int finallyStart, size_t frameBase, const std::string& fileName, int line)
            : tryStart(tryStart), tryEnd(tryEnd), catchStart(catchStart), finallyStart(finallyStart),
              frameBase(frameBase), fileName(fileName), line(line) {}
    };

    std::vector<ExceptionFrame> exceptionFrames;  ///< Stack of exception frames (LIFO - last in, first handled)
    bool hasException;  ///< Flag indicating an exception is currently being handled
    Value pendingException;  ///< The exception to be re-thrown after finally block executes

    // Static (immutable) variables - tracked in VM so they persist across REPL statements
    // Once set, these cannot be changed (like const, but enforced)
    std::set<std::string> staticVariables;

    // Track declared global variables to prevent redeclaration in REPL
    // Because accidentally redefining 'pi = 3.14' in the REPL would be embarrassing
    std::set<std::string> declaredGlobals;

    // Flag to track if we're running a .ntsc (safe) file
    // Safe files have restricted permissions (no file I/O, no system calls, etc.)
    bool isSafeFile;

private:
    // Internal call methods - not for public consumption (like the kitchen in a restaurant)
    bool call(Function* function, int argCount);
    bool callValue(Value callee, int argCount);
    bool callArrayMethod(class BoundArrayMethod* method, int argCount);
    bool callStringMethod(class BoundStringMethod* method, int argCount);

    /**
     * @brief Run the VM until frames are exhausted or reduced to minFrameDepth.
     * @param minFrameDepth Stop when frame count reaches this level (0 = run completely).
     */
    void run(size_t minFrameDepth = 0);

public:
    // Exposed methods for native modules - use with caution (here be dragons)
    bool callValuePublic(Value callee, int argCount) {
        return callValue(callee, argCount);
    }

    void runPublic(size_t minFrameDepth = 0) {
        run(minFrameDepth);
    }

    // Thread safety - because sometimes you really do need multiple threads
    std::mutex vm_mutex;
    std::thread::id owner_thread;
    int lock_count = 0;

    void lock();
    void unlock();
    int unlock_fully();
    void relock(int count);

private:
    // Module interpretation - internal use only
    void interpret_module(const std::vector<std::unique_ptr<Stmt>>& statements, std::shared_ptr<Environment> module_env);

    // Exception handling internals
    bool handleException(const Value& exception);

    // Garbage collection internals - the unsung hero of memory management
    std::vector<Object*> grayStack;  ///< Gray set for tri-color marking
    void markRoots();                ///< Mark all root objects
    void markValue(const Value& value);  ///< Mark a value's referenced objects
    void markObject(Object* obj);    ///< Mark a single object
    void traceReferences();          ///< Trace all references from marked objects
    void blackenObject(Object* obj); ///< Mark an object as fully processed
    void collectGarbage();           ///< Main GC entry point
    void sweep();                    ///< Sweep unmarked objects
};

} // namespace neutron

#endif // NEUTRON_VM_H
