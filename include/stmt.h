#pragma once

#include <memory>
#include <vector>
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
    CLASS
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
    std::unique_ptr<Expr> initializer;
    
    VarStmt(Token name, std::unique_ptr<Expr> initializer)
        : Stmt(StmtType::VAR), name(name), initializer(std::move(initializer)) {}
        
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
    std::unique_ptr<Stmt> elseBranch; // nullptr if no else clause
    
    IfStmt(std::unique_ptr<Expr> condition, 
           std::unique_ptr<Stmt> thenBranch, 
           std::unique_ptr<Stmt> elseBranch)
        : Stmt(StmtType::IF), 
          condition(std::move(condition)), 
          thenBranch(std::move(thenBranch)), 
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
    
    UseStmt(Token library) 
        : Stmt(StmtType::USE), library(library) {}
        
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

}
