#ifndef NEUTRON_EXPR_H
#define NEUTRON_EXPR_H

#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include "token.h"

namespace neutron {

// Forward declarations
class Compiler;
class Expr;
class Stmt;

// Expression types
enum class ExprType {
    LITERAL,
    VARIABLE,
    BINARY,
    UNARY,
    GROUPING,
    MEMBER,
    CALL,
    ASSIGN,
    OBJECT
};

// Literal types
enum class LiteralValueType {
    NIL,
    BOOLEAN,
    NUMBER,
    STRING
};

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

// Base expression class
class Expr {
public:
    ExprType type;
    
    Expr(ExprType type) : type(type) {}
    virtual ~Expr() = default;
    virtual void accept(Compiler* compiler) const = 0;
};

// Literal expression (numbers, strings, booleans, nil)
class LiteralExpr : public Expr {
public:
    // Store values directly instead of using a union
    std::shared_ptr<void> value; // Could be string, double, bool, or nullptr
    LiteralValueType valueType;
    
    LiteralExpr() : Expr(ExprType::LITERAL), value(nullptr), valueType(LiteralValueType::NIL) {
    }
    
    LiteralExpr(bool val) : Expr(ExprType::LITERAL), valueType(LiteralValueType::BOOLEAN) {
        auto ptr = std::make_shared<bool>(val);
        value = std::static_pointer_cast<void>(ptr);
    }
    
    LiteralExpr(double val) : Expr(ExprType::LITERAL), valueType(LiteralValueType::NUMBER) {
        auto ptr = std::make_shared<double>(val);
        value = std::static_pointer_cast<void>(ptr);
    }
    
    LiteralExpr(const std::string& val) : Expr(ExprType::LITERAL), valueType(LiteralValueType::STRING) {
        auto ptr = std::make_shared<std::string>(val);
        value = std::static_pointer_cast<void>(ptr);
    }

    void accept(Compiler* compiler) const override;
};

// Variable expression
class VariableExpr : public Expr {
public:
    Token name;
    
    VariableExpr(Token name) 
        : Expr(ExprType::VARIABLE), name(name) {}

    void accept(Compiler* compiler) const override;
};

// Binary expression (e.g., addition, subtraction, etc.)
class BinaryExpr : public Expr {
public:
    std::unique_ptr<Expr> left;
    Token op;
    std::unique_ptr<Expr> right;
    
    BinaryExpr(std::unique_ptr<Expr> left, Token op, std::unique_ptr<Expr> right)
        : Expr(ExprType::BINARY), left(std::move(left)), op(op), right(std::move(right)) {}

    void accept(Compiler* compiler) const override;
};

// Unary expression (e.g., negation)
class UnaryExpr : public Expr {
public:
    Token op;
    std::unique_ptr<Expr> right;
    
    UnaryExpr(Token op, std::unique_ptr<Expr> right)
        : Expr(ExprType::UNARY), op(op), right(std::move(right)) {}

    void accept(Compiler* compiler) const override;
};

// Grouping expression (parentheses)
class GroupingExpr : public Expr {
public:
    std::unique_ptr<Expr> expression;
    
    GroupingExpr(std::unique_ptr<Expr> expression)
        : Expr(ExprType::GROUPING), expression(std::move(expression)) {}

    void accept(Compiler* compiler) const override;
};

// Member access expression (e.g., obj.property)
class MemberExpr : public Expr {
public:
    std::unique_ptr<Expr> object;
    Token property;
    
    MemberExpr(std::unique_ptr<Expr> object, Token property)
        : Expr(ExprType::MEMBER), object(std::move(object)), property(property) {}

    void accept(Compiler* compiler) const override;
};

// Base statement class
class Stmt {
public:
    StmtType type;
    
    Stmt(StmtType type) : type(type) {}
    virtual ~Stmt() = default;
    virtual void accept(Compiler* compiler) const = 0;
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
    Token name;
    std::vector<std::unique_ptr<FunctionStmt>> methods;

    ClassStmt(Token name, std::vector<std::unique_ptr<FunctionStmt>> methods)
        : Stmt(StmtType::CLASS), name(name), methods(std::move(methods)) {}

    void accept(Compiler* compiler) const override;
};

// Call expression (function calls)
class CallExpr : public Expr {
public:
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;
    
    CallExpr(std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> arguments)
        : Expr(ExprType::CALL), callee(std::move(callee)), arguments(std::move(arguments)) {}

    void accept(Compiler* compiler) const override;
};

// Assignment expression
class AssignExpr : public Expr {
public:
    Token name;
    std::unique_ptr<Expr> value;
    
    AssignExpr(Token name, std::unique_ptr<Expr> value)
        : Expr(ExprType::ASSIGN), name(name), value(std::move(value)) {}

    void accept(Compiler* compiler) const override;
};

// Object literal expression
class ObjectExpr : public Expr {
public:
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> properties;
    
    ObjectExpr(std::vector<std::pair<std::string, std::unique_ptr<Expr>>> properties)
        : Expr(ExprType::OBJECT), properties(std::move(properties)) {}

    void accept(Compiler* compiler) const override;
};

} // namespace neutron

#endif // NEUTRON_EXPR_H