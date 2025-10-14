#pragma once

#include "types/callable.h"
#include "compiler/bytecode.h"
#include "runtime/environment.h"
#include <string>
#include <memory>

namespace neutron {

class FunctionStmt;

class Function : public Callable {
public:
    Function(const FunctionStmt* declaration, std::shared_ptr<Environment> closure);
    int arity() override;
    Value call(VM& vm, std::vector<Value> arguments) override;
    std::string toString() override;

    std::string name;
    int arity_val;
    Chunk* chunk;
    const FunctionStmt* declaration;  // Made public for error reporting
    
private:
    std::shared_ptr<Environment> closure;
};

}
