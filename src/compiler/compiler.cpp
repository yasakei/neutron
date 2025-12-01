#include "compiler/compiler.h"
#include "compiler/parser.h" // For the Parser class
#include "compiler/scanner.h" // For the Scanner class
#include "compiler/bytecode.h"
#include "runtime/debug.h"
#include "runtime/error_handler.h"
#include <iostream>

namespace neutron {

Compiler::Compiler(VM& vm) : enclosing(nullptr), function(nullptr), vm(vm), chunk(nullptr), scopeDepth(0), currentLine(1) {
    function = new Function(nullptr, std::make_shared<Environment>());
    function->name = "<script>";
    function->chunk = new Chunk();
    chunk = function->chunk;
}

Compiler::Compiler(Compiler* enclosing) : enclosing(enclosing), function(nullptr), vm(enclosing->vm), chunk(nullptr), scopeDepth(0), currentLine(1) {
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
    chunk->write(byte, currentLine);
}

void Compiler::emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

void Compiler::emitReturn() {
    emitByte((uint8_t)OpCode::OP_NIL);
    emitByte((uint8_t)OpCode::OP_RETURN);
}

uint8_t Compiler::makeConstant(const Value& value) {
    int constant = chunk->addConstant(value);
    if (constant > UINT8_MAX) {
        // Handle error: too many constants
        throw std::runtime_error("Error: Too many constants in one chunk. Maximum of 256 constants allowed.");
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
        throw std::runtime_error("Error: Too much code to jump over. Maximum jump distance is 65535 bytes.");
    }

    chunk->code[offset] = (jump >> 8) & 0xff;
    chunk->code[offset + 1] = jump & 0xff;
}

void Compiler::emitLoop(int loopStart) {
    emitByte((uint8_t)OpCode::OP_LOOP);

    int offset = chunk->code.size() - loopStart + 2;
    if (offset > UINT16_MAX) {
        throw std::runtime_error("Error: Loop body too large. Maximum loop size is 65535 bytes.");
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
    // Update current line from operator token
    currentLine = expr->op.line;
    
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
    // Update current line from operator token
    currentLine = expr->op.line;
    
    // Handle logical operators separately (short-circuit evaluation)
    if (expr->op.type == TokenType::AND) {
        // Short-circuit AND: evaluate left, if falsy return left, else return right
        compileExpression(expr->left.get());
        emitByte((uint8_t)OpCode::OP_DUP);  // Duplicate left to keep original as potential result
        int jumpToRight = emitJump((uint8_t)OpCode::OP_JUMP_IF_FALSE); // Jump if duplicate is falsy (original is falsy)
        
        // If we reach here, original left was truthy (duplicate was truthy, so no jump)
        // Stack has original left value since OP_JUMP_IF_FALSE popped the duplicate
        emitByte((uint8_t)OpCode::OP_POP);  // Pop the original left value
        compileExpression(expr->right.get());  // Evaluate right as result
        
        int skipFalsy = emitJump((uint8_t)OpCode::OP_JUMP);  // Skip the falsy case
        
        // When original left was falsy, we jump here
        patchJump(jumpToRight);
        // Stack has original falsy left value (since OP_JUMP_IF_FALSE popped the duplicate)
        // That's our result when left is falsy - no additional operation needed
        
        patchJump(skipFalsy);  // End of both paths
        return;
    }
    
    if (expr->op.type == TokenType::OR) {
        // Short-circuit OR: evaluate left, if truthy return left, else return right
        compileExpression(expr->left.get());
        emitByte((uint8_t)OpCode::OP_DUP);  // Duplicate to preserve original value
        emitByte((uint8_t)OpCode::OP_NOT);  // NOT the duplicate
        int jumpToRight = emitJump((uint8_t)OpCode::OP_JUMP_IF_FALSE); // Jump if (NOT left) is falsy, i.e., if original left is truthy
        
        // If we reach here, original left was falsy (so we continue execution)
        // Stack has original falsy left value
        emitByte((uint8_t)OpCode::OP_POP);  // Pop the original falsy left 
        compileExpression(expr->right.get()); // Evaluate right as result
        
        int skipRight = emitJump((uint8_t)OpCode::OP_JUMP); // Skip to end
        
        // When the jump happens (original left was truthy)
        patchJump(jumpToRight);
        // Stack has original truthy left value (since OP_JUMP_IF_FALSE popped the NOT'd duplicate)
        // No additional operation needed, original truthy left is already result
        
        patchJump(skipRight); // End of both paths
        return;
    }
    
    // For non-logical operators, compile normally
    compileExpression(expr->left.get());
    compileExpression(expr->right.get());

    switch (expr->op.type) {
        case TokenType::PLUS:          emitByte((uint8_t)OpCode::OP_ADD); break;
        case TokenType::MINUS:         emitByte((uint8_t)OpCode::OP_SUBTRACT); break;
        case TokenType::STAR:          emitByte((uint8_t)OpCode::OP_MULTIPLY); break;
        case TokenType::SLASH:         emitByte((uint8_t)OpCode::OP_DIVIDE); break;
        case TokenType::PERCENT:       emitByte((uint8_t)OpCode::OP_MODULO); break;
        case TokenType::EQUAL_EQUAL:   emitByte((uint8_t)OpCode::OP_EQUAL); break;
        case TokenType::BANG_EQUAL:    emitByte((uint8_t)OpCode::OP_NOT_EQUAL); break;
        case TokenType::GREATER:       emitByte((uint8_t)OpCode::OP_GREATER); break;
        case TokenType::GREATER_EQUAL: emitBytes((uint8_t)OpCode::OP_LESS, (uint8_t)OpCode::OP_NOT); break;
        case TokenType::LESS:          emitByte((uint8_t)OpCode::OP_LESS); break;
        case TokenType::LESS_EQUAL:    emitBytes((uint8_t)OpCode::OP_GREATER, (uint8_t)OpCode::OP_NOT); break;
        default:
            return; // Should not reach for logical operators due to early return
    }
}

void Compiler::visitVariableExpr(const VariableExpr* expr) {
    // Update current line from variable name token
    currentLine = expr->name.line;
    
    int arg = resolveLocal(expr->name);
    if (arg != -1) {
        emitBytes((uint8_t)OpCode::OP_GET_LOCAL, arg);
    } else {
        emitBytes((uint8_t)OpCode::OP_GET_GLOBAL, makeConstant(Value(expr->name.lexeme)));
    }
}

void Compiler::visitAssignExpr(const AssignExpr* expr) {
    // Check if trying to assign to a static variable (tracked in VM for REPL persistence)
    if (vm.staticVariables.count(expr->name.lexeme)) {
        std::string errorMsg = "Cannot assign to static variable '" + expr->name.lexeme + "'. Static variables are immutable.";
        ErrorHandler::reportRuntimeError(errorMsg, vm.currentFileName, expr->name.line);
        exit(1);
    }
    
    // Check if the variable has a type annotation
    int arg = resolveLocal(expr->name);
    if (arg != -1) {
        // It's a local variable, check if it has a type annotation
        if (locals[arg].typeAnnotation.has_value()) {
            Token typeAnnotation = locals[arg].typeAnnotation.value();
            // Emit the value first
            compileExpression(expr->value.get());
            // Then emit the type-safe assignment instruction with the expected type
            emitBytes((uint8_t)OpCode::OP_SET_LOCAL_TYPED, arg);
            emitByte((uint8_t)typeAnnotation.type);
        } else {
            // No type annotation, use regular assignment
            compileExpression(expr->value.get());
            emitBytes((uint8_t)OpCode::OP_SET_LOCAL, arg);
        }
    } else {
        // For global variables, we need to check if they have a type annotation
        // Since we don't store type annotations for globals in the compiler,
        // we need a different mechanism. For now, store type information in a special way.
        // Look for a convention where we might store type info alongside the variable
        
        // For global variables, use the type-safe assignment which will check against stored type at runtime
        compileExpression(expr->value.get());
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
    // Update current line from variable name token
    currentLine = stmt->name.line;
    
    if (scopeDepth > 0) {
        // Local variable
        for (int i = locals.size() - 1; i >= 0; i--) {
            Local& local = locals[i];
            if (local.depth != -1 && local.depth < scopeDepth) {
                break;
            }

            if (stmt->name.lexeme == local.name.lexeme) {
                std::string errorMsg = "Variable '" + stmt->name.lexeme + "' is already declared in this scope.";
                ErrorHandler::reportRuntimeError(errorMsg, vm.currentFileName, stmt->name.line);
                exit(1);
            }
        }

        // Perform compile-time type checking if type annotation exists
        if (stmt->typeAnnotation.has_value() && stmt->initializer) {
            ValueType exprType = getExpressionType(stmt->initializer.get());
            // Debug: Print what we're checking
            std::string expectedType = tokenTypeToString(stmt->typeAnnotation.value().type);
            std::string actualType = valueTypeToString(exprType);
            // For debugging, check if we have a type mismatch case
            if (exprType != ValueType::NIL) {
                bool isValid = validateType(stmt->typeAnnotation, exprType);
                if (!isValid) {
                    // Type mismatch - throw an error to enforce type safety
                    throw std::runtime_error("Type mismatch on line " + std::to_string(stmt->name.line) + 
                                             ": Cannot assign value of type '" + actualType + 
                                             "' to variable of type '" + expectedType + "'");
                }
            }
        }

        locals.push_back(Local{stmt->name, scopeDepth, stmt->typeAnnotation});
        
        // Track static variables in VM (persistence across REPL statements)
        if (stmt->isStatic) {
            vm.staticVariables.insert(stmt->name.lexeme);
        }

        if (stmt->initializer) {
            compileExpression(stmt->initializer.get());
        } else {
            emitByte((uint8_t)OpCode::OP_NIL);
        }
        // No need to emit a define instruction for locals
        return;
    }

    // Global variable
    // Check if already declared
    if (declaredGlobals.count(stmt->name.lexeme)) {
        std::string errorMsg = "Variable '" + stmt->name.lexeme + "' is already declared.";
        ErrorHandler::reportRuntimeError(errorMsg, vm.currentFileName, stmt->name.line);
        exit(1);
    }
    declaredGlobals.insert(stmt->name.lexeme);
    
    // Track static variables in VM (persistence across REPL statements)
    if (stmt->isStatic) {
        vm.staticVariables.insert(stmt->name.lexeme);
    }
    
    if (stmt->initializer) {
        compileExpression(stmt->initializer.get());
    } else {
        emitByte((uint8_t)OpCode::OP_NIL);
    }
    
    // If there's a type annotation, use the typed define instruction
    if (stmt->typeAnnotation.has_value()) {
        emitBytes((uint8_t)OpCode::OP_DEFINE_TYPED_GLOBAL, makeConstant(Value(stmt->name.lexeme)));
        emitByte((uint8_t)stmt->typeAnnotation.value().type);
    } else {
        // Use regular define for variables without type annotations
        emitBytes((uint8_t)OpCode::OP_DEFINE_GLOBAL, makeConstant(Value(stmt->name.lexeme)));
    }
}

void Compiler::visitBlockStmt(const BlockStmt* stmt) {
    if (stmt->createsScope) beginScope();
    for (const auto& statement : stmt->statements) {
        compileStatement(statement.get());
    }
    if (stmt->createsScope) endScope();
}

void Compiler::visitIfStmt(const IfStmt* stmt) {
    // First, compile the main if condition and then branch
    compileExpression(stmt->condition.get());
    
    // Emit a jump instruction with a placeholder offset for the main if branch.
    int thenJump = emitJump((uint8_t)OpCode::OP_JUMP_IF_FALSE);
    
    // Compile the main then branch
    compileStatement(stmt->thenBranch.get());
    
    // Jump to end (skip all elif and else branches)
    std::vector<int> endJumps;
    endJumps.push_back(emitJump((uint8_t)OpCode::OP_JUMP));
    
    // Patch the main if condition jump
    patchJump(thenJump);
    
    // Compile elif branches
    for (const auto& elifBranch : stmt->elifBranches) {
        // Compile elif condition
        compileExpression(elifBranch.first.get());
        
        // Emit a jump instruction with a placeholder offset for this elif branch
        int elifJump = emitJump((uint8_t)OpCode::OP_JUMP_IF_FALSE);
        
        // Compile elif body
        compileStatement(elifBranch.second.get());
        
        // Jump to end (skip remaining elif and else branches)
        endJumps.push_back(emitJump((uint8_t)OpCode::OP_JUMP));
        
        // Patch the elif condition jump
        patchJump(elifJump);
    }
    
    // Compile else branch if it exists
    if (stmt->elseBranch) {
        compileStatement(stmt->elseBranch.get());
    }
    
    // Patch all end jumps to point to the end of the entire if-elif-else statement
    for (int jump : endJumps) {
        patchJump(jump);
    }
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
            
            // Reserve slot 0 for 'this' in class methods
            Token thisToken(TokenType::THIS, "this", 0);
            compiler.locals.push_back(Local{thisToken, compiler.scopeDepth, std::nullopt});
            
            // Add parameters starting at slot 1
            for (const auto& param : funcStmt->params) {
                compiler.locals.push_back(Local{param, compiler.scopeDepth, std::nullopt});
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
    if (stmt->importedSymbols.empty()) {
        // Standard import (import everything)
        if (stmt->isFilePath) {
            // Import a .nt file
            vm.load_file(stmt->library.lexeme);
        } else {
            // Import a module
            vm.load_module(stmt->library.lexeme);
        }
    } else {
        // Selective import
        Module* module = nullptr;
        
        if (stmt->isFilePath) {
            // Load file as module (isolated)
            module = vm.load_file_as_module(stmt->library.lexeme);
        } else {
            // Load module
            vm.load_module(stmt->library.lexeme);
            // Retrieve the module object from globals
            // load_module defines the module in globals with its name
            auto it = vm.globals.find(stmt->library.lexeme);
            if (it != vm.globals.end() && it->second.isModule()) {
                module = it->second.asModule();
            }
        }
        
        if (module) {
            // Import requested symbols
            for (const auto& token : stmt->importedSymbols) {
                std::string name = token.lexeme;
                Value val = module->get(name);
                if (val.type != ValueType::NIL) {
                    // Define in current global scope
                    emitBytes((uint8_t)OpCode::OP_CONSTANT, makeConstant(val));
                    emitBytes((uint8_t)OpCode::OP_DEFINE_GLOBAL, makeConstant(Value(name)));
                } else {
                     emitBytes((uint8_t)OpCode::OP_CONSTANT, makeConstant(val));
                     emitBytes((uint8_t)OpCode::OP_DEFINE_GLOBAL, makeConstant(Value(name)));
                }
            }
            
            // CRITICAL FIX: If this was a module import (not file), load_module() defined the module
            // in globals. We must remove it to ensure selective import doesn't leak the module itself.
            if (!stmt->isFilePath) {
                // We can't easily remove from globals at compile time because globals are runtime state.
                // But we can emit code to undefine it!
                // Wait, VM::load_module runs at COMPILE TIME for the compiler to see it?
                // No, vm.load_module is called here in the compiler, which runs during compilation.
                // So the module IS in vm.globals right now.
                
                // We should remove it from vm.globals so subsequent compilation doesn't see it.
                vm.globals.erase(stmt->library.lexeme);
                vm.loadedModuleCache.erase(stmt->library.lexeme); // Also remove from cache so it can be re-imported if needed
                
                // However, load_module ALSO emits code or defines it in the runtime environment?
                // Let's check vm.load_module again. It calls neutron_init_X_module(this).
                // Those functions usually do vm->define_module("name", module).
                // vm->define_module does globals["name"] = module.
                
                // So removing it from vm.globals here affects the COMPILER's view of the VM.
                // But does it affect the RUNTIME?
                // The compiler is running on the SAME VM instance that will execute the code (in REPL)
                // or a VM that is being prepared.
                
                // If we are compiling a script, we are populating the VM's globals.
                // So erasing it here is exactly what we want!
            }
        }
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
        compiler.locals.push_back(Local{param, compiler.scopeDepth, std::nullopt});
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
    (void)expr; // Unused parameter
    // In class methods, 'this' refers to the current instance
    // We'll emit an OP_THIS instruction to load the current instance
    emitByte((uint8_t)OpCode::OP_THIS);
}

void Compiler::visitFunctionExpr(const FunctionExpr* expr) {
    // Create a new compiler for the lambda function
    Compiler compiler(this);
    compiler.function = new Function(nullptr, std::make_shared<Environment>());
    compiler.function->chunk = new Chunk();
    compiler.chunk = compiler.function->chunk;
    compiler.function->arity_val = expr->params.size();
    
    // Add parameters as local variables
    compiler.beginScope();
    for (const auto& param : expr->params) {
        compiler.locals.push_back(Local{param, compiler.scopeDepth, std::nullopt});
    }
    
    // Compile the function body
    for (const auto& statement : expr->body) {
        compiler.compileStatement(statement.get());
    }
    
    // Emit return
    compiler.emitReturn();
    
    // Create a closure for the lambda
    emitBytes((uint8_t)OpCode::OP_CLOSURE, makeConstant(Value(compiler.function)));
}

void Compiler::visitBreakStmt(const BreakStmt* stmt) {
    (void)stmt; // Unused parameter
    if (breakJumps.empty()) {
        throw std::runtime_error("Cannot use 'break' outside of a loop.");
    }
    
    // Emit a jump instruction and record it for later patching
    int jump = emitJump((uint8_t)OpCode::OP_JUMP);
    breakJumps.back().push_back(jump);
}

void Compiler::visitContinueStmt(const ContinueStmt* stmt) {
    (void)stmt; // Unused parameter
    if (continueJumps.empty()) {
        throw std::runtime_error("Cannot use 'continue' outside of a loop.");
    }
    
    // Emit a forward jump to the continue target (end of loop body, before loop back)
    int jump = emitJump((uint8_t)OpCode::OP_JUMP);
    continueJumps.back().push_back(jump);
}

std::string Compiler::valueTypeToString(ValueType type) {
    switch (type) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOLEAN: return "bool";
        case ValueType::NUMBER: return "number";
        case ValueType::STRING: return "string";
        case ValueType::ARRAY: return "array";
        case ValueType::OBJECT: return "object";
        case ValueType::CALLABLE: return "function";
        case ValueType::MODULE: return "module";
        case ValueType::CLASS: return "class";
        case ValueType::INSTANCE: return "instance";
        default: return "unknown";
    }
}

std::string Compiler::tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::TYPE_INT: return "int";
        case TokenType::TYPE_FLOAT: return "float";
        case TokenType::TYPE_STRING: return "string";
        case TokenType::TYPE_BOOL: return "bool";
        case TokenType::TYPE_ARRAY: return "array";
        case TokenType::TYPE_OBJECT: return "object";
        case TokenType::TYPE_ANY: return "any";
        default: return "unknown";
    }
}

bool Compiler::validateType(const std::optional<Token>& typeAnnotation, ValueType actualType) {
    if (!typeAnnotation.has_value()) {
        return true; // No type annotation means no validation needed
    }
    
    TokenType expectedType = typeAnnotation.value().type;
    
    // Map token types to value types
    switch (expectedType) {
        case TokenType::TYPE_INT:
        case TokenType::TYPE_FLOAT:
            return actualType == ValueType::NUMBER;
        case TokenType::TYPE_STRING:
            return actualType == ValueType::STRING;
        case TokenType::TYPE_BOOL:
            return actualType == ValueType::BOOLEAN;
        case TokenType::TYPE_ARRAY:
            return actualType == ValueType::ARRAY;
        case TokenType::TYPE_OBJECT:
            return actualType == ValueType::OBJECT;
        case TokenType::TYPE_ANY:
            return true; // 'any' type accepts everything
        default:
            return true;
    }
}

ValueType Compiler::getExpressionType(const Expr* expr) {
    // Try to determine the type of an expression at compile time
    // This is a best-effort analysis - some types can only be known at runtime
    
    if (auto* literal = dynamic_cast<const LiteralExpr*>(expr)) {
        // Map LiteralValueType to ValueType
        switch (literal->valueType) {
            case LiteralValueType::NIL:
                return ValueType::NIL;
            case LiteralValueType::BOOLEAN:
                return ValueType::BOOLEAN;
            case LiteralValueType::NUMBER:
                return ValueType::NUMBER;
            case LiteralValueType::STRING:
                return ValueType::STRING;
            default:
                return ValueType::NIL;
        }
    }
    
    if (auto* binary = dynamic_cast<const BinaryExpr*>(expr)) {
        // Most binary operations return numbers
        if (binary->op.type == TokenType::PLUS) {
            // Could be number or string concatenation
            ValueType leftType = getExpressionType(binary->left.get());
            ValueType rightType = getExpressionType(binary->right.get());
            if (leftType == ValueType::STRING || rightType == ValueType::STRING) {
                return ValueType::STRING;
            }
            return ValueType::NUMBER;
        }
        
        // Comparison operators return boolean
        if (binary->op.type == TokenType::EQUAL_EQUAL ||
            binary->op.type == TokenType::BANG_EQUAL ||
            binary->op.type == TokenType::GREATER ||
            binary->op.type == TokenType::GREATER_EQUAL ||
            binary->op.type == TokenType::LESS ||
            binary->op.type == TokenType::LESS_EQUAL) {
            return ValueType::BOOLEAN;
        }
        
        return ValueType::NUMBER; // Default for arithmetic
    }
    
    if (auto* unary = dynamic_cast<const UnaryExpr*>(expr)) {
        if (unary->op.type == TokenType::BANG) {
            return ValueType::BOOLEAN;
        }
        return ValueType::NUMBER; // Minus operator
    }
    
    if (dynamic_cast<const ArrayExpr*>(expr)) {
        return ValueType::ARRAY;
    }
    
    if (dynamic_cast<const ObjectExpr*>(expr)) {
        return ValueType::OBJECT;
    }
    
    if (auto* variable = dynamic_cast<const VariableExpr*>(expr)) {
        // Try to find the variable's declared type
        int local = resolveLocal(variable->name);
        if (local != -1 && locals[local].typeAnnotation.has_value()) {
            Token typeToken = locals[local].typeAnnotation.value();
            switch (typeToken.type) {
                case TokenType::TYPE_INT:
                case TokenType::TYPE_FLOAT:
                    return ValueType::NUMBER;
                case TokenType::TYPE_STRING:
                    return ValueType::STRING;
                case TokenType::TYPE_BOOL:
                    return ValueType::BOOLEAN;
                case TokenType::TYPE_ARRAY:
                    return ValueType::ARRAY;
                case TokenType::TYPE_OBJECT:
                    return ValueType::OBJECT;
                default:
                    break;
            }
        }
    }
    
    // Unknown type at compile time
    return ValueType::NIL; // Use NIL as "unknown" marker
}

void Compiler::visitMatchStmt(const MatchStmt* stmt) {
    // Compile the match expression
    compileExpression(stmt->expression.get());
    
    std::vector<int> endJumps;  // Track jumps to the end of match statement
    size_t numCases = stmt->cases.size();
    
    // Compile each case
    for (size_t i = 0; i < numCases; i++) {
        const MatchCase& matchCase = stmt->cases[i];
        
        // Safety checks
        if (!matchCase.value || !matchCase.action) {
            throw std::runtime_error("Invalid match case: null value or action");
        }
        
        // Duplicate the match value on stack for comparison
        emitByte((uint8_t)OpCode::OP_DUP);
        
        // Compile the case value
        compileExpression(matchCase.value.get());
        
        // Check if they're equal
        emitByte((uint8_t)OpCode::OP_EQUAL);
        
        // If not equal, jump to next case (OP_JUMP_IF_FALSE pops the comparison result)
        int skipJump = emitJump((uint8_t)OpCode::OP_JUMP_IF_FALSE);
        
        // Case matched! (comparison result was already popped by JUMP_IF_FALSE)
        emitByte((uint8_t)OpCode::OP_POP);  // Pop the match value (we're done with it)
        
        // Execute the case action
        compileStatement(matchCase.action.get());
        
        // Jump to end after executing case
        endJumps.push_back(emitJump((uint8_t)OpCode::OP_JUMP));
        
        // Patch the skip jump to here (case didn't match, comparison already popped)
        patchJump(skipJump);
        // Match value is still on stack for next iteration or cleanup
    }
    
    // At this point, if no case matched, match value is still on stack
    // Pop it before default case
    emitByte((uint8_t)OpCode::OP_POP);
    
    // Compile default case if present
    if (stmt->defaultCase) {
        compileStatement(stmt->defaultCase.get());
    }
    
    // Patch all end jumps to here (after default case)
    for (size_t i = 0; i < endJumps.size(); i++) {
        patchJump(endJumps[i]);
    }
}

void Compiler::visitTryStmt(const TryStmt* stmt) {
    // Store positions for patching later
    std::vector<int> endJumps; // Jumps to skip catch and finally blocks when no exception occurs
    
    // Emit the TRY instruction and handler information
    emitByte((uint8_t)OpCode::OP_TRY);
    
    // Placeholders: tryEnd(2 bytes), catchStart(2 bytes), finallyStart(2 bytes) = 6 bytes total
    int handlerInfoStart = chunk->code.size();
    emitBytes(0x00, 0x00); // Placeholder for tryEnd
    emitBytes(0x00, 0x00); // Placeholder for catchStart  
    emitBytes(0x00, 0x00); // Placeholder for finallyStart
    

    
    // Compile the try block
    compileStatement(stmt->tryBlock.get());
    
    // Now we know where the try block ends
    int tryEnd = chunk->code.size();
    
    // Patch the tryEnd information
    int tryEndPos = handlerInfoStart;
    chunk->code[tryEndPos] = (tryEnd >> 8) & 0xFF;
    chunk->code[tryEndPos + 1] = tryEnd & 0xFF;
    
    // Jump over catch block when try block completes normally (no exception)
    if (stmt->catchBlock) {
        int jumpOverCatch = emitJump((uint8_t)OpCode::OP_JUMP);
        endJumps.push_back(jumpOverCatch);
    }
    
    int catchStart = -1;
    // Compile catch block if present
    if (stmt->catchBlock) {
        catchStart = chunk->code.size(); // Current position is catch start
        
        // Patch the catchStart information
        int catchStartPos = handlerInfoStart + 2;
        chunk->code[catchStartPos] = (catchStart >> 8) & 0xFF;
        chunk->code[catchStartPos + 1] = catchStart & 0xFF;
        
        // Set up catch variable as local
        beginScope();
        if (stmt->catchVar.lexeme != "") {
            // Add the catch variable to locals 
            // When an exception is caught, the exception value is on the stack at this point
            // So we need to store the exception value (which is on top of the stack)
            // into the local slot for the catch variable
            locals.push_back(Local{stmt->catchVar, scopeDepth, std::nullopt});
            
            // The exception value is now on top of the stack
            // We need to store it in the catch variable's local slot (which is the last slot added)
            int slot = locals.size() - 1;
            emitBytes((uint8_t)OpCode::OP_SET_LOCAL, slot);
        }
        
        // Compile the rest of the catch block
        compileStatement(stmt->catchBlock.get());
        endScope();
    } else {
        // If no catch block, set catchStart to -1 (0xFFFF in bytecode)
        int catchStartPos = handlerInfoStart + 2;
        chunk->code[catchStartPos] = 0xFF;
        chunk->code[catchStartPos + 1] = 0xFF;
    }
    
    // If there's a finally block, we need to emit appropriate jump
    if (stmt->finallyBlock) {
        // In a full implementation, we would handle the finally block properly
        // For now, the finally block will be handled by the VM's exception mechanism
        
        int finallyStart = chunk->code.size();
        // Patch the finallyStart information
        int finallyStartPos = handlerInfoStart + 4;
        chunk->code[finallyStartPos] = (finallyStart >> 8) & 0xFF;
        chunk->code[finallyStartPos + 1] = finallyStart & 0xFF;
        
        compileStatement(stmt->finallyBlock.get());
    } else {
        // If no finally block, set finallyStart to -1 (0xFFFF in bytecode) 
        int finallyStartPos = handlerInfoStart + 4;
        chunk->code[finallyStartPos] = 0xFF;
        chunk->code[finallyStartPos + 1] = 0xFF;
    }
    
    // Patch the jumps that skip catch blocks when no exception occurs
    for (int jump : endJumps) {
        patchJump(jump);
    }
    
    // Emit END_TRY to mark the end of the exception handling construct
    emitByte((uint8_t)OpCode::OP_END_TRY);
}

void Compiler::visitThrowStmt(const ThrowStmt* stmt) {
    // Compile the value to throw
    compileExpression(stmt->value.get());
    
    // Emit the throw instruction
    emitByte((uint8_t)OpCode::OP_THROW);
}

} // namespace neutron
