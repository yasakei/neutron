#pragma once

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
