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
    USE,
    FUNCTION,
    RETURN,
    CLASS,
    BREAK,
    CONTINUE,
    MATCH,
    TRY,
    THROW
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
    
    VarStmt(Token name, std::unique_ptr<Expr> initializer, std::optional<Token> typeAnnotation = std::nullopt)
        : Stmt(StmtType::VAR), name(name), typeAnnotation(typeAnnotation), initializer(std::move(initializer)) {}
        
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
    
    BlockStmt(std::vector<std::unique_ptr<Stmt>> statements)
        : Stmt(StmtType::BLOCK), statements(std::move(statements)) {}
        
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

// Use statement
class UseStmt : public Stmt {
public:
    Token library;
    bool isFilePath;  // true if importing a .nt file, false if importing a module
    
    UseStmt(Token library, bool isFilePath = false) 
        : Stmt(StmtType::USE), library(library), isFilePath(isFilePath) {}
        
    void accept(Compiler* compiler) const override;
};

// Function declaration statement
class FunctionStmt : public Stmt {
public:
    Token name;
    std::vector<Token> params;
    std::vector<std::unique_ptr<Stmt>> body;
    
    FunctionStmt(Token name, std::vector<Token> params, std::vector<std::unique_ptr<Stmt>> body)
        : Stmt(StmtType::FUNCTION), name(name), params(std::move(params)), body(std::move(body)) {}
        
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

}
