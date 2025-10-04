#include "compiler/compiler.h"
#include "compiler/parser.h" // For the Parser class
#include "compiler/scanner.h" // For the Scanner class
#include "compiler/bytecode.h"
#include "runtime/debug.h"
#include <iostream>

namespace neutron {

Compiler::Compiler(VM& vm) : enclosing(nullptr), function(nullptr), vm(vm), chunk(nullptr), scopeDepth(0) {
    function = new Function(nullptr, std::make_shared<Environment>());
    function->name = "<script>";
    function->chunk = new Chunk();
    chunk = function->chunk;
}

Compiler::Compiler(Compiler* enclosing) : enclosing(enclosing), function(nullptr), vm(enclosing->vm), chunk(nullptr), scopeDepth(0) {
    function = new Function(nullptr, std::make_shared<Environment>());
    function->chunk = new Chunk();
    chunk = function->chunk;
}

Function* Compiler::compile(const std::vector<std::unique_ptr<Stmt>>& statements) {
    for (const auto& statement : statements) {
        compileStatement(statement.get());
    }
    emitReturn();

#ifdef DEBUG_PRINT_CODE
    disassembleChunk(this->chunk, "code");
#endif

    return function;
}

void Compiler::beginScope() {
    scopeDepth++;
}

void Compiler::endScope() {
    scopeDepth--;
    while (locals.size() > 0 && locals.back().depth > scopeDepth) {
        emitByte((uint8_t)OpCode::OP_POP);
        locals.pop_back();
    }
}

void Compiler::compileStatement(const Stmt* stmt) {
    stmt->accept(this);
}

void Compiler::compileExpression(const Expr* expr) {
    expr->accept(this);
}

void Compiler::emitByte(uint8_t byte) {
    chunk->write(byte, 0); // Placeholder for line number
}

void Compiler::emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

void Compiler::emitReturn() {
    emitByte((uint8_t)OpCode::OP_RETURN);
}

uint8_t Compiler::makeConstant(const Value& value) {
    int constant = chunk->addConstant(value);
    if (constant > UINT8_MAX) {
        // Handle error: too many constants
        return 0;
    }
    return (uint8_t)constant;
}

void Compiler::emitConstant(const Value& value) {
    emitBytes((uint8_t)OpCode::OP_CONSTANT, makeConstant(value));
}

int Compiler::emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return chunk->code.size() - 2;
}

void Compiler::patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = chunk->code.size() - offset - 2;

    if (jump > UINT16_MAX) {
        // error("Too much code to jump over.");
    }

    chunk->code[offset] = (jump >> 8) & 0xff;
    chunk->code[offset + 1] = jump & 0xff;
}

void Compiler::emitLoop(int loopStart) {
    emitByte((uint8_t)OpCode::OP_LOOP);

    int offset = chunk->code.size() - loopStart + 2;
    if (offset > UINT16_MAX) {
        // error("Loop body too large.");
    }

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

int Compiler::resolveLocal(const Token& name) {
    for (int i = locals.size() - 1; i >= 0; i--) {
        Local& local = locals[i];
        if (name.lexeme == local.name.lexeme) {
            if (local.depth == -1) {
                // error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

void Compiler::visitLiteralExpr(const LiteralExpr* expr) {
    switch (expr->valueType) {
        case LiteralValueType::NIL:
            emitByte((uint8_t)OpCode::OP_NIL);
            break;
        case LiteralValueType::BOOLEAN:
            emitByte(*static_cast<bool*>(expr->value.get()) ? (uint8_t)OpCode::OP_TRUE : (uint8_t)OpCode::OP_FALSE);
            break;
        case LiteralValueType::NUMBER:
            emitConstant(Value(*static_cast<double*>(expr->value.get())));
            break;
        case LiteralValueType::STRING:
            emitConstant(Value(*static_cast<std::string*>(expr->value.get())));
            break;
    }
}

void Compiler::visitGroupingExpr(const GroupingExpr* expr) {
    compileExpression(expr->expression.get());
}

void Compiler::visitUnaryExpr(const UnaryExpr* expr) {
    compileExpression(expr->right.get());

    // Emit the operator instruction.
    switch (expr->op.type) {
        case TokenType::MINUS: emitByte((uint8_t)OpCode::OP_NEGATE); break;
        case TokenType::BANG: emitByte((uint8_t)OpCode::OP_NOT); break;
        default:
            return; // Unreachable.
    }
}

void Compiler::visitBinaryExpr(const BinaryExpr* expr) {
    compileExpression(expr->left.get());
    compileExpression(expr->right.get());

    switch (expr->op.type) {
        case TokenType::PLUS:          emitByte((uint8_t)OpCode::OP_ADD); break;
        case TokenType::MINUS:         emitByte((uint8_t)OpCode::OP_SUBTRACT); break;
        case TokenType::STAR:          emitByte((uint8_t)OpCode::OP_MULTIPLY); break;
        case TokenType::SLASH:         emitByte((uint8_t)OpCode::OP_DIVIDE); break;
        case TokenType::PERCENT:       emitByte((uint8_t)OpCode::OP_MODULO); break;
        case TokenType::EQUAL_EQUAL:   emitByte((uint8_t)OpCode::OP_EQUAL); break;
        case TokenType::GREATER:       emitByte((uint8_t)OpCode::OP_GREATER); break;
        case TokenType::GREATER_EQUAL: emitBytes((uint8_t)OpCode::OP_LESS, (uint8_t)OpCode::OP_NOT); break;
        case TokenType::LESS:          emitByte((uint8_t)OpCode::OP_LESS); break;
        case TokenType::LESS_EQUAL:    emitBytes((uint8_t)OpCode::OP_GREATER, (uint8_t)OpCode::OP_NOT); break;
        default:
            return; // Unreachable.
    }
}

void Compiler::visitVariableExpr(const VariableExpr* expr) {
    int arg = resolveLocal(expr->name);
    if (arg != -1) {
        emitBytes((uint8_t)OpCode::OP_GET_LOCAL, arg);
    } else {
        emitBytes((uint8_t)OpCode::OP_GET_GLOBAL, makeConstant(Value(expr->name.lexeme)));
    }
}

void Compiler::visitAssignExpr(const AssignExpr* expr) {
    compileExpression(expr->value.get());
    int arg = resolveLocal(expr->name);
    if (arg != -1) {
        emitBytes((uint8_t)OpCode::OP_SET_LOCAL, arg);
    } else {
        emitBytes((uint8_t)OpCode::OP_SET_GLOBAL, makeConstant(Value(expr->name.lexeme)));
    }
}

void Compiler::visitExpressionStmt(const ExpressionStmt* stmt) {
    compileExpression(stmt->expression.get());
    emitByte((uint8_t)OpCode::OP_POP);
}

void Compiler::visitSayStmt(const SayStmt* stmt) {
    compileExpression(stmt->expression.get());
    emitByte((uint8_t)OpCode::OP_SAY);
}

void Compiler::visitVarStmt(const VarStmt* stmt) {
    if (scopeDepth > 0) {
        // Local variable
        for (int i = locals.size() - 1; i >= 0; i--) {
            Local& local = locals[i];
            if (local.depth != -1 && local.depth < scopeDepth) {
                break;
            }

            if (stmt->name.lexeme == local.name.lexeme) {
                // error("Already a variable with this name in this scope.");
            }
        }


        locals.push_back({stmt->name, scopeDepth});

        if (stmt->initializer) {
            compileExpression(stmt->initializer.get());
        } else {
            emitByte((uint8_t)OpCode::OP_NIL);
        }
        // No need to emit a define instruction for locals
        return;
    }

    // Global variable
    if (stmt->initializer) {
        compileExpression(stmt->initializer.get());
    } else {
        emitByte((uint8_t)OpCode::OP_NIL);
    }
    emitBytes((uint8_t)OpCode::OP_DEFINE_GLOBAL, makeConstant(Value(stmt->name.lexeme)));
}

void Compiler::visitBlockStmt(const BlockStmt* stmt) {
    beginScope();
    for (const auto& statement : stmt->statements) {
        compileStatement(statement.get());
    }
    endScope();
}

void Compiler::visitIfStmt(const IfStmt* stmt) {
    compileExpression(stmt->condition.get());

    // Emit a jump instruction with a placeholder offset.
    int thenJump = emitJump((uint8_t)OpCode::OP_JUMP_IF_FALSE);

    compileStatement(stmt->thenBranch.get());

    int elseJump = emitJump((uint8_t)OpCode::OP_JUMP);

    patchJump(thenJump);

    if (stmt->elseBranch) {
        compileStatement(stmt->elseBranch.get());
    }

    patchJump(elseJump);
}

void Compiler::visitWhileStmt(const WhileStmt* stmt) {
    int loopStart = chunk->code.size();
    
    // Check if this is a for-loop (body is a BlockStmt with 2 statements, last being ExpressionStmt)
    bool isForLoop = false;
    const BlockStmt* blockBody = dynamic_cast<const BlockStmt*>(stmt->body.get());
    if (blockBody && blockBody->statements.size() == 2) {
        if (dynamic_cast<const ExpressionStmt*>(blockBody->statements[1].get())) {
            isForLoop = true;
        }
    }
    
    // Push loop info for break/continue tracking
    loopStarts.push_back(loopStart);
    breakJumps.push_back(std::vector<int>());
    continueJumps.push_back(std::vector<int>());
    continueTargets.push_back(-1);  // Will be set later for for-loops

    compileExpression(stmt->condition.get());

    int exitJump = emitJump((uint8_t)OpCode::OP_JUMP_IF_FALSE);

    if (isForLoop && blockBody) {
        // For for-loops, compile body and increment separately
        // so we can set continue target before increment
        beginScope();
        
        // Compile the main body (first statement)
        compileStatement(blockBody->statements[0].get());
        
        // Set continue target here (before increment)
        continueTargets.back() = chunk->code.size();
        
        // Compile the increment (second statement)
        compileStatement(blockBody->statements[1].get());
        
        endScope();
    } else {
        // Regular while loop - continue target is loop start
        continueTargets.back() = loopStart;
        compileStatement(stmt->body.get());
    }
    
    // Patch all continue jumps
    for (int jump : continueJumps.back()) {
        int target = continueTargets.back();
        int offset = target - jump - 2;
        if (offset < 0) {
            // Backward jump (to loop start for while loops)
            offset = -offset;
            chunk->code[jump - 1] = (uint8_t)OpCode::OP_LOOP;
            chunk->code[jump] = (offset >> 8) & 0xff;
            chunk->code[jump + 1] = offset & 0xff;
        } else {
            // Forward jump (to increment for for loops)
            chunk->code[jump] = (offset >> 8) & 0xff;
            chunk->code[jump + 1] = offset & 0xff;
        }
    }

    emitLoop(loopStart);

    patchJump(exitJump);
    
    // Patch all break jumps to jump to end of loop
    for (int breakJump : breakJumps.back()) {
        patchJump(breakJump);
    }
    
    // Pop loop info
    loopStarts.pop_back();
    breakJumps.pop_back();
    continueJumps.pop_back();
    continueTargets.pop_back();
}

void Compiler::visitClassStmt(const ClassStmt* stmt) {
    // Create a new class object
    auto klass = new Class(stmt->name.lexeme);

    // Compile the methods
    for (const auto& method : stmt->body) {
        if (auto funcStmt = dynamic_cast<const FunctionStmt*>(method.get())) {
            Compiler compiler(this);
            compiler.function->name = funcStmt->name.lexeme;
            compiler.function->arity_val = funcStmt->params.size();

            compiler.beginScope();
            for (const auto& param : funcStmt->params) {
                compiler.locals.push_back({param, compiler.scopeDepth});
            }

            for (const auto& statement : funcStmt->body) {
                compiler.compileStatement(statement.get());
            }

            compiler.emitReturn();

            klass->methods[funcStmt->name.lexeme] = compiler.function;
        } else if (dynamic_cast<const VarStmt*>(method.get())) {
            // For now, we only support function declarations in classes
            // Properties will be handled by the VM at runtime
        }
    }

    // Define the class as a global variable
    emitBytes((uint8_t)OpCode::OP_CONSTANT, makeConstant(Value(klass)));
    emitBytes((uint8_t)OpCode::OP_DEFINE_GLOBAL, makeConstant(Value(stmt->name.lexeme)));
}

void Compiler::visitUseStmt(const UseStmt* stmt) {
    if (stmt->isFilePath) {
        // Import a .nt file
        vm.load_file(stmt->library.lexeme);
    } else {
        // Import a module
        vm.load_module(stmt->library.lexeme);
    }
}

void Compiler::visitFunctionStmt(const FunctionStmt* stmt) {
    // Create a new compiler for this function
    Compiler compiler(this);
    
    // Set the function's name and arity
    compiler.function->name = stmt->name.lexeme;
    compiler.function->arity_val = stmt->params.size();

    // Add parameters as local variables
    compiler.beginScope();
    for (const auto& param : stmt->params) {
        compiler.locals.push_back({param, compiler.scopeDepth});
    }

    // Compile the function body
    for (const auto& statement : stmt->body) {
        compiler.compileStatement(statement.get());
    }

    // Emit return for the function
    compiler.emitReturn();

#ifdef DEBUG_PRINT_CODE
    disassembleChunk(compiler.function->chunk, compiler.function->name.c_str());
#endif

    // Add the compiled function as a constant in the current (enclosing) compiler
    uint8_t constant = makeConstant(Value(compiler.function));
    
    // Define the function as a global variable in the current scope
    emitBytes((uint8_t)OpCode::OP_CONSTANT, constant);
    emitBytes((uint8_t)OpCode::OP_DEFINE_GLOBAL, makeConstant(Value(stmt->name.lexeme)));
}

void Compiler::visitReturnStmt(const ReturnStmt* stmt) {
    if (stmt->value) {
        compileExpression(stmt->value.get());
    } else {
        emitByte((uint8_t)OpCode::OP_NIL);
    }
    emitByte((uint8_t)OpCode::OP_RETURN);
}

void Compiler::visitMemberExpr(const MemberExpr* expr) {
    // Compile the object expression
    compileExpression(expr->object.get());
    
    // Emit a GET_PROPERTY instruction with the property name
    emitBytes((uint8_t)OpCode::OP_GET_PROPERTY, makeConstant(Value(expr->property.lexeme)));
}

void Compiler::visitCallExpr(const CallExpr* expr) {
    // Compile the callee
    compileExpression(expr->callee.get());
    
    // Compile the arguments
    for (const auto& argument : expr->arguments) {
        compileExpression(argument.get());
    }
    
    // Emit the call instruction with the number of arguments
    emitBytes((uint8_t)OpCode::OP_CALL, expr->arguments.size());
}

void Compiler::visitArrayExpr(const ArrayExpr* expr) {
    // Compile all array elements
    for (const auto& element : expr->elements) {
        compileExpression(element.get());
    }
    
    // Emit the array instruction with the number of elements
    emitBytes((uint8_t)OpCode::OP_ARRAY, expr->elements.size());
}

void Compiler::visitIndexGetExpr(const IndexGetExpr* expr) {
    // Compile the array and index
    compileExpression(expr->array.get());
    compileExpression(expr->index.get());
    
    // Emit the index get instruction
    emitByte((uint8_t)OpCode::OP_INDEX_GET);
}

void Compiler::visitIndexSetExpr(const IndexSetExpr* expr) {
    // Compile all three expressions: array, index, and value
    compileExpression(expr->array.get());
    compileExpression(expr->index.get());
    compileExpression(expr->value.get());
    
    // Emit the index set instruction
    emitByte((uint8_t)OpCode::OP_INDEX_SET);
}

void Compiler::visitMemberSetExpr(const MemberSetExpr* expr) {
    // Compile the object and value
    compileExpression(expr->object.get());
    compileExpression(expr->value.get());
    
    // Emit the property name as a constant
    emitBytes((uint8_t)OpCode::OP_SET_PROPERTY, makeConstant(Value(expr->property.lexeme)));
}

void Compiler::visitObjectExpr(const ObjectExpr* expr) {
    // Create a new JsonObject
    auto obj = new JsonObject();
    
    // For each property, check if it's a string literal and store the actual value
    for (const auto& property : expr->properties) {
        // Check if the value is a string literal
        const LiteralExpr* literalExpr = dynamic_cast<const LiteralExpr*>(property.second.get());
        if (literalExpr && literalExpr->valueType == LiteralValueType::STRING) {
            // Store the actual string value
            obj->properties[property.first] = Value(*static_cast<std::string*>(literalExpr->value.get()));
        } else if (literalExpr && literalExpr->valueType == LiteralValueType::NUMBER) {
            // Store the actual number value
            obj->properties[property.first] = Value(*static_cast<double*>(literalExpr->value.get()));
        } else if (literalExpr && literalExpr->valueType == LiteralValueType::BOOLEAN) {
            // Store the actual boolean value
            obj->properties[property.first] = Value(*static_cast<bool*>(literalExpr->value.get()));
        } else {
            // For other expressions, store a placeholder value
            obj->properties[property.first] = Value(property.first + " value");
        }
    }
    
    // Push the object onto the stack
    emitConstant(Value(obj));
}

void Compiler::visitThisExpr(const ThisExpr* expr) {
    // In class methods, 'this' refers to the current instance
    // We'll emit an OP_THIS instruction to load the current instance
    emitByte((uint8_t)OpCode::OP_THIS);
}

void Compiler::visitBreakStmt(const BreakStmt* stmt) {
    if (breakJumps.empty()) {
        throw std::runtime_error("Cannot use 'break' outside of a loop.");
    }
    
    // Emit a jump instruction and record it for later patching
    int jump = emitJump((uint8_t)OpCode::OP_JUMP);
    breakJumps.back().push_back(jump);
}

void Compiler::visitContinueStmt(const ContinueStmt* stmt) {
    if (continueJumps.empty()) {
        throw std::runtime_error("Cannot use 'continue' outside of a loop.");
    }
    
    // Emit a forward jump to the continue target (end of loop body, before loop back)
    int jump = emitJump((uint8_t)OpCode::OP_JUMP);
    continueJumps.back().push_back(jump);
}

} // namespace neutron
