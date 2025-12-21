#ifndef NEUTRON_EXPR_H
#define NEUTRON_EXPR_H

#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include "token.h"

namespace neutron {

// Forward declarations
class Expr;
class Stmt;
class Compiler;  // For visitor pattern

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
    OBJECT,
    ARRAY,
    INDEX_GET,
    INDEX_SET,
    MEMBER_SET,
    THIS,
    FUNCTION,  // Lambda/anonymous function expression
    TERNARY
};

// Literal types
enum class LiteralValueType {
    NIL,
    BOOLEAN,
    NUMBER,
    STRING
};



// Base expression class
class Expr {
public:
    ExprType type;
    
    Expr(ExprType type) : type(type) {}
    virtual ~Expr() = default;
    virtual void accept(Compiler* compiler) const = 0;  // Visitor pattern
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

// Array literal expression
class ArrayExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elements;
    
    ArrayExpr(std::vector<std::unique_ptr<Expr>> elements)
        : Expr(ExprType::ARRAY), elements(std::move(elements)) {}
        
    void accept(Compiler* compiler) const override;
};

// Index get expression (array[index])
class IndexGetExpr : public Expr {
public:
    std::unique_ptr<Expr> array;
    std::unique_ptr<Expr> index;
    
    IndexGetExpr(std::unique_ptr<Expr> array, std::unique_ptr<Expr> index)
        : Expr(ExprType::INDEX_GET), array(std::move(array)), index(std::move(index)) {}
        
    void accept(Compiler* compiler) const override;
};

// Index set expression (array[index] = value)
class IndexSetExpr : public Expr {
public:
    std::unique_ptr<Expr> array;
    std::unique_ptr<Expr> index;
    std::unique_ptr<Expr> value;
    
    IndexSetExpr(std::unique_ptr<Expr> array, std::unique_ptr<Expr> index, std::unique_ptr<Expr> value)
        : Expr(ExprType::INDEX_SET), array(std::move(array)), index(std::move(index)), value(std::move(value)) {}
        
    void accept(Compiler* compiler) const override;
};

// Member set expression (obj.property = value)
class MemberSetExpr : public Expr {
public:
    std::unique_ptr<Expr> object;
    Token property;
    std::unique_ptr<Expr> value;
    
    MemberSetExpr(std::unique_ptr<Expr> object, Token property, std::unique_ptr<Expr> value)
        : Expr(ExprType::MEMBER_SET), object(std::move(object)), property(property), value(std::move(value)) {}
        
    void accept(Compiler* compiler) const override;
};

// This expression
class ThisExpr : public Expr {
public:
    Token keyword;
    
    ThisExpr(Token keyword)
        : Expr(ExprType::THIS), keyword(keyword) {}
        
    void accept(Compiler* compiler) const override;
};

// Function expression (lambda/anonymous function)
class FunctionExpr : public Expr {
public:
    std::vector<Token> params;
    std::vector<std::unique_ptr<Stmt>> body;
    
    FunctionExpr(std::vector<Token> params, std::vector<std::unique_ptr<Stmt>> body)
        : Expr(ExprType::FUNCTION), params(std::move(params)), body(std::move(body)) {}
        
    void accept(Compiler* compiler) const override;
};

// Ternary expression (condition ? then : else)
class TernaryExpr : public Expr {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> thenBranch;
    std::unique_ptr<Expr> elseBranch;
    
    TernaryExpr(std::unique_ptr<Expr> condition, std::unique_ptr<Expr> thenBranch, std::unique_ptr<Expr> elseBranch)
        : Expr(ExprType::TERNARY), condition(std::move(condition)), thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}
        
    void accept(Compiler* compiler) const override;
};

} // namespace neutron

#endif // NEUTRON_EXPR_H