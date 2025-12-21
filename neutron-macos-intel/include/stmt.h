#pragma once

#include <memory>
#include <vector>
#include <optional>
#include "token.h"
#include "expr.h"

namespace neutron {

// Statement types
enum class StmtType {
    EXPRESSION,
    SAY,
    VAR,
    BLOCK,
    IF,
    WHILE,
    DO_WHILE,
    USE,
    FUNCTION,
    RETURN,
    CLASS,
    BREAK,
    CONTINUE,
    MATCH,
    TRY,
    THROW,
    RETRY,
    SAFE
};

// Base statement class
class Stmt {
public:
    StmtType type;
    
    Stmt(StmtType type) : type(type) {}
    virtual ~Stmt() = default;
    virtual void accept(Compiler* compiler) const = 0;  // Visitor pattern
};

// Expression statement
class ExpressionStmt : public Stmt {
public:
    std::unique_ptr<Expr> expression;
    
    ExpressionStmt(std::unique_ptr<Expr> expression)
        : Stmt(StmtType::EXPRESSION), expression(std::move(expression)) {}
        
    void accept(Compiler* compiler) const override;
};

// Say statement
class SayStmt : public Stmt {
public:
    std::unique_ptr<Expr> expression;
    
    SayStmt(std::unique_ptr<Expr> expression)
        : Stmt(StmtType::SAY), expression(std::move(expression)) {}
        
    void accept(Compiler* compiler) const override;
};

// Variable declaration statement
class VarStmt : public Stmt {
public:
    Token name;
    std::optional<Token> typeAnnotation;  // Optional type annotation (e.g., int, string, bool)
    std::unique_ptr<Expr> initializer;
    bool isStatic;  // True if declared with 'static' keyword
    
    VarStmt(Token name, std::unique_ptr<Expr> initializer, std::optional<Token> typeAnnotation = std::nullopt, bool isStatic = false)
        : Stmt(StmtType::VAR), name(name), typeAnnotation(typeAnnotation), initializer(std::move(initializer)), isStatic(isStatic) {}
        
    void accept(Compiler* compiler) const override;
};

// Variable assignment statement
class AssignStmt : public Stmt {
public:
    Token name;
    std::unique_ptr<Expr> value;
    
    AssignStmt(Token name, std::unique_ptr<Expr> value)
        : Stmt(StmtType::VAR), name(name), value(std::move(value)) {}
        
    void accept(Compiler* compiler) const override;
};

// Block statement
class BlockStmt : public Stmt {
public:
    std::vector<std::unique_ptr<Stmt>> statements;
    bool createsScope;
    
    BlockStmt(std::vector<std::unique_ptr<Stmt>> statements, bool createsScope = true)
        : Stmt(StmtType::BLOCK), statements(std::move(statements)), createsScope(createsScope) {}
        
    void accept(Compiler* compiler) const override;
};

// If statement
class IfStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenBranch;
    std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Stmt>>> elifBranches; // elif conditions and branches
    std::unique_ptr<Stmt> elseBranch; // nullptr if no else clause
    
    IfStmt(std::unique_ptr<Expr> condition, 
           std::unique_ptr<Stmt> thenBranch, 
           std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Stmt>>> elifBranches,
           std::unique_ptr<Stmt> elseBranch)
        : Stmt(StmtType::IF), 
          condition(std::move(condition)), 
          thenBranch(std::move(thenBranch)), 
          elifBranches(std::move(elifBranches)),
          elseBranch(std::move(elseBranch)) {}
          
    void accept(Compiler* compiler) const override;
};

// While statement
class WhileStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;

    WhileStmt(std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> body)
        : Stmt(StmtType::WHILE),
          condition(std::move(condition)),
          body(std::move(body)) {}
          
    void accept(Compiler* compiler) const override;
};

// Do-While statement
class DoWhileStmt : public Stmt {
public:
    std::unique_ptr<Stmt> body;
    std::unique_ptr<Expr> condition;
    
    DoWhileStmt(std::unique_ptr<Stmt> body, std::unique_ptr<Expr> condition)
        : Stmt(StmtType::DO_WHILE), body(std::move(body)), condition(std::move(condition)) {}
        
    void accept(Compiler* compiler) const override;
};

// Use statement
class UseStmt : public Stmt {
public:
    Token library;
    bool isFilePath;  // true if importing a .nt file, false if importing a module
    std::vector<Token> importedSymbols; // Symbols to import (empty = import all/standard behavior)
    
    UseStmt(Token library, bool isFilePath = false, std::vector<Token> importedSymbols = {}) 
        : Stmt(StmtType::USE), library(library), isFilePath(isFilePath), importedSymbols(std::move(importedSymbols)) {}
        
    void accept(Compiler* compiler) const override;
};

// Function parameter with optional type annotation
struct FunctionParam {
    Token name;
    std::optional<Token> typeAnnotation;
    
    FunctionParam(Token name, std::optional<Token> typeAnnotation = std::nullopt)
        : name(name), typeAnnotation(typeAnnotation) {}
};

// Function declaration statement
class FunctionStmt : public Stmt {
public:
    Token name;
    std::vector<FunctionParam> params;
    std::optional<Token> returnType;  // Optional return type annotation
    std::vector<std::unique_ptr<Stmt>> body;
    
    FunctionStmt(Token name, std::vector<FunctionParam> params, std::vector<std::unique_ptr<Stmt>> body, std::optional<Token> returnType = std::nullopt)
        : Stmt(StmtType::FUNCTION), name(name), params(std::move(params)), returnType(returnType), body(std::move(body)) {}
        
    void accept(Compiler* compiler) const override;
};

// Return statement
class ReturnStmt : public Stmt {
public:
    std::unique_ptr<Expr> value;
    
    ReturnStmt(std::unique_ptr<Expr> value)
        : Stmt(StmtType::RETURN), value(std::move(value)) {}
        
    void accept(Compiler* compiler) const override;
};

// Class declaration statement
class ClassStmt : public Stmt {
public:
    ClassStmt(Token name, std::vector<std::unique_ptr<Stmt>> body)
        : Stmt(StmtType::CLASS), name(name), body(std::move(body)) {}

    Token name;
    std::vector<std::unique_ptr<Stmt>> body;
    
    void accept(Compiler* compiler) const override;
};

// Break statement
class BreakStmt : public Stmt {
public:
    BreakStmt() : Stmt(StmtType::BREAK) {}
    
    void accept(Compiler* compiler) const override;
};

// Continue statement
class ContinueStmt : public Stmt {
public:
    ContinueStmt() : Stmt(StmtType::CONTINUE) {}
    
    void accept(Compiler* compiler) const override;
};

// Match case clause
struct MatchCase {
    std::unique_ptr<Expr> value;  // Case value to match against
    std::unique_ptr<Stmt> action;  // Statement to execute if matched
};

// Match statement
class MatchStmt : public Stmt {
public:
    std::unique_ptr<Expr> expression;  // Expression to match
    std::vector<MatchCase> cases;  // List of cases
    std::unique_ptr<Stmt> defaultCase;  // Optional default case
    
    MatchStmt(std::unique_ptr<Expr> expression, 
              std::vector<MatchCase> cases,
              std::unique_ptr<Stmt> defaultCase)
        : Stmt(StmtType::MATCH), 
          expression(std::move(expression)),
          cases(std::move(cases)),
          defaultCase(std::move(defaultCase)) {}
    
    void accept(Compiler* compiler) const override;
};

// Try-Catch statement
class TryStmt : public Stmt {
public:
    std::unique_ptr<Stmt> tryBlock;  // Try block
    Token catchVar;  // Exception variable name (optional)
    std::unique_ptr<Stmt> catchBlock;  // Catch block (optional)
    std::unique_ptr<Stmt> finallyBlock;  // Finally block (optional)
    
    TryStmt(std::unique_ptr<Stmt> tryBlock,
            Token catchVar,
            std::unique_ptr<Stmt> catchBlock,
            std::unique_ptr<Stmt> finallyBlock)
        : Stmt(StmtType::TRY),
          tryBlock(std::move(tryBlock)),
          catchVar(catchVar),
          catchBlock(std::move(catchBlock)),
          finallyBlock(std::move(finallyBlock)) {}
    
    void accept(Compiler* compiler) const override;
};

// Throw statement
class ThrowStmt : public Stmt {
public:
    std::unique_ptr<Expr> value;  // Value to throw
    
    ThrowStmt(std::unique_ptr<Expr> value)
        : Stmt(StmtType::THROW), value(std::move(value)) {}
    
    void accept(Compiler* compiler) const override;
};

// Retry statement
class RetryStmt : public Stmt {
public:
    std::unique_ptr<Expr> count; // Number of retries
    std::unique_ptr<Stmt> body; // Code to retry
    Token catchVar; // Optional catch variable
    std::unique_ptr<Stmt> catchBlock; // Optional catch block
    
    RetryStmt(std::unique_ptr<Expr> count, 
              std::unique_ptr<Stmt> body,
              Token catchVar,
              std::unique_ptr<Stmt> catchBlock)
        : Stmt(StmtType::RETRY), 
          count(std::move(count)), 
          body(std::move(body)),
          catchVar(catchVar),
          catchBlock(std::move(catchBlock)) {}
    
    void accept(Compiler* compiler) const override;
};

// Safe block statement - enforces type annotations
class SafeStmt : public Stmt {
public:
    std::unique_ptr<Stmt> body;  // Block that requires type annotations
    
    SafeStmt(std::unique_ptr<Stmt> body)
        : Stmt(StmtType::SAFE), body(std::move(body)) {}
    
    void accept(Compiler* compiler) const override;
};

}
