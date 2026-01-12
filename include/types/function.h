#pragma once

// Windows macro undefs - must be before any standard library includes
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    // Undefine Windows macros that conflict with C++ code
    // NOTE: Do NOT undefine FAR and NEAR as they are needed by some Windows headers
    #undef TRUE
    #undef FALSE
    #undef DELETE
    #undef ERROR
    #undef IN
    #undef OUT
    #undef OPTIONAL
    #undef interface
    #undef small
    #undef max
    #undef min
#endif

#include "types/callable.h"
#include "compiler/bytecode.h"
#include "runtime/environment.h"
#include "token.h"  // Add this for TokenType
#include <string>
#include <memory>
#include <optional>  // Add this for std::optional

namespace neutron {

class FunctionStmt;

class Function : public Callable {
    friend class VM;
    friend class CheckpointManager;
public:
    Function(const FunctionStmt* declaration, std::shared_ptr<Environment> closure);
    
    // Constructor for deserialization
    Function(std::string name, int arity) : name(name), arity_val(arity), declaration(nullptr), closure(nullptr) {
        chunk = new Chunk();
    }

    ~Function();
    int arity() override;
    Value call(VM& vm, std::vector<Value> arguments) override;
    std::string toString() const override;

    std::string name;
    int arity_val;
    Chunk* chunk;
    const FunctionStmt* declaration;  // Made public for error reporting
    std::vector<std::optional<TokenType>> paramTypes;  // Parameter type annotations
    std::optional<TokenType> returnType;  // Return type annotation
    
private:
    std::shared_ptr<Environment> closure;
};

}
