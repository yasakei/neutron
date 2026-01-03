#include "compiler/parser.h"
#include "token.h"
#include "runtime/error_handler.h"
#include <stdexcept>
#include <iostream>
#include <memory>
#include "compiler/compiler.h"
#include "types/value.h"

namespace neutron {

// Helper functions
ValueType tokenTypeToValueType(TokenType type) {
    switch (type) {
        case TokenType::TYPE_INT: return ValueType::NUMBER;
        case TokenType::TYPE_FLOAT: return ValueType::NUMBER;
        case TokenType::TYPE_STRING: return ValueType::OBJ_STRING;
        case TokenType::TYPE_BOOL: return ValueType::BOOLEAN;
        case TokenType::TYPE_ARRAY: return ValueType::ARRAY;
        case TokenType::TYPE_OBJECT: return ValueType::OBJECT;
        default: return ValueType::NIL; // Should not happen
    }
}

std::string tokenTypeToString(TokenType type) {
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

std::string valueTypeToString(ValueType type) {
    switch (type) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOLEAN: return "bool";
        case ValueType::NUMBER: return "number";
        case ValueType::OBJ_STRING: return "string";
        case ValueType::ARRAY: return "array";
        case ValueType::OBJECT: return "object";
        case ValueType::CALLABLE: return "callable";
        case ValueType::MODULE: return "module";
        case ValueType::CLASS: return "class";
        case ValueType::INSTANCE: return "instance";
        default: return "unknown";
    }
}

ValueType literalValueTypeToValueType(LiteralValueType type) {
    switch (type) {
        case LiteralValueType::NIL: return ValueType::NIL;
        case LiteralValueType::BOOLEAN: return ValueType::BOOLEAN;
        case LiteralValueType::NUMBER: return ValueType::NUMBER;
        case LiteralValueType::STRING: return ValueType::OBJ_STRING;
        default: return ValueType::NIL; // Should not happen
    }
}

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens), current(0) {}

// Internal exception for parser panic mode
class ParserException : public std::exception {};

std::vector<std::unique_ptr<Stmt>> Parser::parse() {
    std::vector<std::unique_ptr<Stmt>> statements;
    
    while (!isAtEnd()) {
        try {
            statements.push_back(statement());
        } catch (const ParserException&) {
            synchronize();
        }
    }
    
    return statements;
}

std::unique_ptr<Stmt> Parser::statement() {
    if (match({TokenType::SAY})) {
        return sayStatement();
    }
    
    // Check for static var
    if (match({TokenType::STATIC})) {
        if (!match({TokenType::VAR})) {
            error(previous(), "Expect 'var' after 'static'.");
        }
        return varDeclaration(true);  // Pass isStatic = true
    }
    
    if (match({TokenType::VAR})) {
        return varDeclaration();
    }
    
    if (match({TokenType::IF})) {
        return ifStatement();
    }

    if (match({TokenType::WHILE})) {
        return whileStatement();
    }

    if (match({TokenType::DO})) {
        return doWhileStatement();
    }

    if (match({TokenType::FOR})) {
        return forStatement();
    }

    if (match({TokenType::CLASS})) {
        return classDeclaration();
    }
    
    if (match({TokenType::USE, TokenType::USING})) {
        return useStatement();
    }
    
    if (match({TokenType::FUN})) {
        return functionDeclaration();
    }
    
    if (match({TokenType::RETURN})) {
        return returnStatement();
    }
    
    if (match({TokenType::BREAK})) {
        consume(TokenType::SEMICOLON, "Expect ';' after 'break'.");
        return std::make_unique<BreakStmt>();
    }
    
    if (match({TokenType::CONTINUE})) {
        consume(TokenType::SEMICOLON, "Expect ';' after 'continue'.");
        return std::make_unique<ContinueStmt>();
    }
    
    if (match({TokenType::MATCH})) {
        return matchStatement();
    }
    
    if (match({TokenType::TRY})) {
        return tryStatement();
    }
    
    if (match({TokenType::THROW})) {
        return throwStatement();
    }
    
    if (match({TokenType::RETRY})) {
        return retryStatement();
    }
    
    if (match({TokenType::SAFE})) {
        return safeStatement();
    }
    
    if (check(TokenType::LEFT_BRACE)) {
        return block();
    }
    
    return expressionStatement();
}

std::unique_ptr<Stmt> Parser::sayStatement() {
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'say'.");
    std::unique_ptr<Expr> value = expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after value.");
    consume(TokenType::SEMICOLON, "Expect ';' after value.");
    return std::make_unique<SayStmt>(std::move(value));
}

std::unique_ptr<Stmt> Parser::expressionStatement() {
    std::unique_ptr<Expr> expr = expression();
    consume(TokenType::SEMICOLON, "Expect ';' after expression.");
    return std::make_unique<ExpressionStmt>(std::move(expr));
}

std::unique_ptr<Stmt> Parser::varDeclaration(bool isStatic) {
    // Check for optional type annotation
    std::optional<Token> typeAnnotation = std::nullopt;
    if (match({TokenType::TYPE_INT, TokenType::TYPE_FLOAT, TokenType::TYPE_STRING, 
               TokenType::TYPE_BOOL, TokenType::TYPE_ARRAY, TokenType::TYPE_OBJECT, 
               TokenType::TYPE_ANY})) {
        typeAnnotation = previous();
    }
    
    std::vector<std::unique_ptr<Stmt>> statements;
    
    do {
        Token name = consume(TokenType::IDENTIFIER, "Expect variable name.");
        
        std::unique_ptr<Expr> initializer = nullptr;
        if (match({TokenType::EQUAL})) {
            initializer = expression();
        }
        
        // Static variables must have an initializer
        if (isStatic && !initializer) {
            error(name, "Static variables must be initialized.");
        }

        if (typeAnnotation && initializer && typeAnnotation->type != TokenType::TYPE_ANY) {
            // Only perform compile-time type checking for literal expressions
            // Non-literal expressions will be type-checked at runtime
            if (initializer->type == ExprType::LITERAL) {
                LiteralExpr* literal = static_cast<LiteralExpr*>(initializer.get());
                ValueType expectedType = tokenTypeToValueType(typeAnnotation->type);
                ValueType actualType = literalValueTypeToValueType(literal->valueType);
                if (actualType != expectedType) {
                    error(name, "Invalid type assignment. Expected " + tokenTypeToString(typeAnnotation->type) + " but got " + valueTypeToString(actualType) + ".");
                }
            }
            // Note: Type checking for non-literal expressions (function calls, operations, etc.)\n            // is handled at runtime by the VM's typed global/local assignment opcodes
        }
        
        statements.push_back(std::make_unique<VarStmt>(name, std::move(initializer), typeAnnotation, isStatic));
        
    } while (match({TokenType::COMMA}));
    
    consume(TokenType::SEMICOLON, "Expect ';' after variable declaration.");
    
    if (statements.size() == 1) {
        return std::move(statements[0]);
    } else {
        // Return a block that doesn't create a new scope, so variables are declared in the current scope
        return std::make_unique<BlockStmt>(std::move(statements), false);
    }
}

std::unique_ptr<Stmt> Parser::ifStatement() {
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
    std::unique_ptr<Expr> condition = expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after if condition.");
    
    std::unique_ptr<Stmt> thenBranch = statement();
    
    // Handle multiple elif clauses
    std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Stmt>>> elifBranches;
    while (match({TokenType::ELIF})) {
        consume(TokenType::LEFT_PAREN, "Expect '(' after 'elif'.");
        std::unique_ptr<Expr> elifCondition = expression();
        consume(TokenType::RIGHT_PAREN, "Expect ')' after elif condition.");
        std::unique_ptr<Stmt> elifBranch = statement();
        elifBranches.push_back({std::move(elifCondition), std::move(elifBranch)});
    }
    
    std::unique_ptr<Stmt> elseBranch = nullptr;
    if (match({TokenType::ELSE})) {
        elseBranch = statement();
    }
    
    return std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch), std::move(elifBranches), std::move(elseBranch));
}

std::unique_ptr<Stmt> Parser::whileStatement() {
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
    std::unique_ptr<Expr> condition = expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after while condition.");
    std::unique_ptr<Stmt> body = statement();

    return std::make_unique<WhileStmt>(std::move(condition), std::move(body));
}

std::unique_ptr<Stmt> Parser::doWhileStatement() {
    std::unique_ptr<Stmt> body = statement();
    consume(TokenType::WHILE, "Expect 'while' after do-while body.");
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
    std::unique_ptr<Expr> condition = expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after condition.");
    consume(TokenType::SEMICOLON, "Expect ';' after do-while condition.");
    
    return std::make_unique<DoWhileStmt>(std::move(body), std::move(condition));
}

std::unique_ptr<Stmt> Parser::forStatement() {
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");

    std::unique_ptr<Stmt> initializer;
    if (match({TokenType::SEMICOLON})) {
        initializer = nullptr;
    } else if (match({TokenType::VAR})) {
        initializer = varDeclaration();
    } else {
        initializer = expressionStatement();
    }

    std::unique_ptr<Expr> condition = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        condition = expression();
    }
    consume(TokenType::SEMICOLON, "Expect ';' after loop condition.");

    std::unique_ptr<Expr> increment = nullptr;
    if (!check(TokenType::RIGHT_PAREN)) {
        increment = expression();
    }
    consume(TokenType::RIGHT_PAREN, "Expect ')' after for clauses.");

    std::unique_ptr<Stmt> body = statement();

    if (increment != nullptr) {
        std::vector<std::unique_ptr<Stmt>> statements;
        statements.push_back(std::move(body));
        statements.push_back(std::make_unique<ExpressionStmt>(std::move(increment)));
        body = std::make_unique<BlockStmt>(std::move(statements));
    }

    if (condition == nullptr) {
        condition = std::make_unique<LiteralExpr>(true, previous().line);
    }

    body = std::make_unique<WhileStmt>(std::move(condition), std::move(body));

    if (initializer != nullptr) {
        std::vector<std::unique_ptr<Stmt>> statements;
        statements.push_back(std::move(initializer));
        statements.push_back(std::move(body));
        body = std::make_unique<BlockStmt>(std::move(statements));
    }

    return body;
}

std::unique_ptr<Stmt> Parser::classDeclaration() {
    Token name = consume(TokenType::IDENTIFIER, "Expect class name.");
    consume(TokenType::LEFT_BRACE, "Expect '{' before class body.");

    std::vector<std::unique_ptr<Stmt>> body;
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        // Check for 'init' method without 'fun' keyword
        if (check(TokenType::IDENTIFIER) && peek().lexeme == "init") {
            Token initToken = advance();
            consume(TokenType::LEFT_PAREN, "Expect '(' after 'init'.");
            std::vector<FunctionParam> params;
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    Token paramName = consume(TokenType::IDENTIFIER, "Expect parameter name.");
                    params.emplace_back(paramName, std::nullopt);  // No type annotations for init methods yet
                } while (match({TokenType::COMMA}));
            }
            consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
            consume(TokenType::LEFT_BRACE, "Expect '{' before init body.");
            
            // Parse body statements
            std::vector<std::unique_ptr<Stmt>> funcBody;
            while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
                funcBody.push_back(statement());
            }
            consume(TokenType::RIGHT_BRACE, "Expect '}' after init body.");
            
            body.push_back(std::make_unique<FunctionStmt>(initToken, std::move(params), std::move(funcBody)));
        } else {
            body.push_back(statement());
        }
    }

    consume(TokenType::RIGHT_BRACE, "Expect '}' after class body.");

    return std::make_unique<ClassStmt>(name, std::move(body));
}

std::unique_ptr<Stmt> Parser::useStatement() {
    bool isUsing = previous().type == TokenType::USING;
    std::vector<Token> importedSymbols;
    
    // Check for selective import: use (a, b) = from ...
    if (match({TokenType::LEFT_PAREN})) {
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                importedSymbols.push_back(consume(TokenType::IDENTIFIER, "Expect identifier in import list."));
            } while (match({TokenType::COMMA}));
        }
        consume(TokenType::RIGHT_PAREN, "Expect ')' after import list.");
        consume(TokenType::EQUAL, "Expect '=' after import list.");
        consume(TokenType::FROM, "Expect 'from' after import list.");
    }

    if (isUsing) {
        Token filePath = consume(TokenType::STRING, "Expect file path after 'using'.");
        consume(TokenType::SEMICOLON, "Expect ';' after using statement.");
        return std::make_unique<UseStmt>(filePath, true, importedSymbols);
    } else {
        Token moduleName = consume(TokenType::IDENTIFIER, "Expect module name.");
        consume(TokenType::SEMICOLON, "Expect ';' after use statement.");
        return std::make_unique<UseStmt>(moduleName, false, importedSymbols);
    }
}

std::unique_ptr<Stmt> Parser::block() {
    consume(TokenType::LEFT_BRACE, "Expect '{' before block.");
    
    std::vector<std::unique_ptr<Stmt>> statements;
    
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        statements.push_back(statement());
    }
    
    consume(TokenType::RIGHT_BRACE, "Expect '}' after block.");
    return std::make_unique<BlockStmt>(std::move(statements));
}

std::unique_ptr<Expr> Parser::expression() {
    return assignment();
}

std::unique_ptr<Expr> Parser::assignment() {
    std::unique_ptr<Expr> expr = ternary();

    if (match({TokenType::EQUAL})) {
        Token equals = previous();
        std::unique_ptr<Expr> value = assignment();

        if (expr->type == ExprType::VARIABLE) {
            Token name = static_cast<VariableExpr*>(expr.get())->name;
            return std::make_unique<AssignExpr>(name, std::move(value));
        } else if (expr->type == ExprType::INDEX_GET) {
            // Handle assignment to array index: array[index] = value
            IndexGetExpr* indexGet = static_cast<IndexGetExpr*>(expr.get());
            return std::make_unique<IndexSetExpr>(
                std::move(indexGet->array), 
                std::move(indexGet->index), 
                std::move(value)
            );
        } else if (expr->type == ExprType::MEMBER) {
            // Handle assignment to object property: obj.property = value
            MemberExpr* memberExpr = static_cast<MemberExpr*>(expr.get());
            return std::make_unique<MemberSetExpr>(
                std::move(memberExpr->object), 
                memberExpr->property, 
                std::move(value)
            );
        }

        error(equals, "Invalid assignment target.");
    }

    return expr;
}

std::unique_ptr<Expr> Parser::ternary() {
    std::unique_ptr<Expr> expr = logic_or();

    if (match({TokenType::QUESTION})) {
        std::unique_ptr<Expr> thenBranch = expression();
        consume(TokenType::COLON, "Expect ':' after then branch of ternary operator.");
        std::unique_ptr<Expr> elseBranch = ternary();
        return std::make_unique<TernaryExpr>(std::move(expr), std::move(thenBranch), std::move(elseBranch));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::logic_or() {
    std::unique_ptr<Expr> expr = logic_and();

    while (match({TokenType::OR, TokenType::OR_SYM})) {
        Token op = previous();
        std::unique_ptr<Expr> right = logic_and();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::logic_and() {
    std::unique_ptr<Expr> expr = bitwise_or();

    while (match({TokenType::AND, TokenType::AND_SYM})) {
        Token op = previous();
        std::unique_ptr<Expr> right = bitwise_or();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::bitwise_or() {
    std::unique_ptr<Expr> expr = bitwise_xor();

    while (match({TokenType::PIPE})) {
        Token op = previous();
        std::unique_ptr<Expr> right = bitwise_xor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::bitwise_xor() {
    std::unique_ptr<Expr> expr = bitwise_and();

    while (match({TokenType::CARET})) {
        Token op = previous();
        std::unique_ptr<Expr> right = bitwise_and();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expr> Parser::bitwise_and() {
    std::unique_ptr<Expr> expr = equality();

    while (match({TokenType::AMPERSAND})) {
        Token op = previous();
        std::unique_ptr<Expr> right = equality();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }

    return expr;
}


std::unique_ptr<Expr> Parser::equality() {
    std::unique_ptr<Expr> expr = comparison();
    
    while (match({TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL})) {
        Token op = previous();
        std::unique_ptr<Expr> right = comparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    
    return expr;
}

std::unique_ptr<Expr> Parser::comparison() {
    std::unique_ptr<Expr> expr = bitwise_shift();
    
    while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, 
                  TokenType::LESS, TokenType::LESS_EQUAL,
                  TokenType::BANG_EQUAL, TokenType::EQUAL_EQUAL})) {
        Token operatorToken = previous();
        std::unique_ptr<Expr> right = bitwise_shift();
        expr = std::make_unique<BinaryExpr>(std::move(expr), operatorToken, std::move(right));
    }
    
    return expr;
}

std::unique_ptr<Expr> Parser::bitwise_shift() {
    std::unique_ptr<Expr> expr = term();
    
    while (match({TokenType::LESS_LESS, TokenType::GREATER_GREATER})) {
        Token op = previous();
        std::unique_ptr<Expr> right = term();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    
    return expr;
}

std::unique_ptr<Expr> Parser::term() {
    std::unique_ptr<Expr> expr = factor();
    
    while (match({TokenType::MINUS, TokenType::PLUS})) {
        Token op = previous();
        std::unique_ptr<Expr> right = factor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    
    return expr;
}

std::unique_ptr<Expr> Parser::factor() {
    std::unique_ptr<Expr> expr = unary();
    
    while (match({TokenType::SLASH, TokenType::STAR, TokenType::PERCENT})) {
        Token op = previous();
        std::unique_ptr<Expr> right = unary();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    
    return expr;
}

std::unique_ptr<Expr> Parser::unary() {
    if (match({TokenType::BANG, TokenType::MINUS, TokenType::TILDE})) {
        Token op = previous();
        std::unique_ptr<Expr> right = unary();
        return std::make_unique<UnaryExpr>(op, std::move(right));
    }
    
    // Handle prefix increment and decrement (++x, --x)
    if (match({TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
        Token op = previous();
        std::unique_ptr<Expr> operand = unary();
        
        // The operand must be an lvalue (variable or member access)
        if (operand->type != ExprType::VARIABLE && 
            operand->type != ExprType::MEMBER && 
            operand->type != ExprType::INDEX_GET) {
            error(op, "Invalid operand for prefix increment/decrement operator.");
        }
        
        // Convert ++x to x = x + 1 or --x to x = x - 1
        Token binaryOp = Token(op.type == TokenType::PLUS_PLUS ? TokenType::PLUS : TokenType::MINUS, 
                               op.type == TokenType::PLUS_PLUS ? "+" : "-", op.line);
        
        // Create: operand + 1 or operand - 1
        auto one = std::make_unique<LiteralExpr>(1.0, op.line);
        
        // Clone the operand for the binary expression (need a copy)
        std::unique_ptr<Expr> operandCopy;
        if (operand->type == ExprType::VARIABLE) {
            auto varExpr = static_cast<VariableExpr*>(operand.get());
            operandCopy = std::make_unique<VariableExpr>(varExpr->name);
        } else {
            // For member access or index access, we'll need to handle this more carefully
            // For now, let's report an error for complex lvalues
            error(op, "Prefix increment/decrement on complex expressions not yet supported.");
        }
        
        auto binaryExpr = std::make_unique<BinaryExpr>(std::move(operandCopy), binaryOp, std::move(one));
        
        // Create assignment: operand = (operand + 1)
        if (operand->type == ExprType::VARIABLE) {
            auto varExpr = static_cast<VariableExpr*>(operand.get());
            return std::make_unique<AssignExpr>(varExpr->name, std::move(binaryExpr));
        }
    }
    
    return call();
}

std::unique_ptr<Expr> Parser::call() {
    std::unique_ptr<Expr> expr = primary();
    
    while (true) {
        if (match({TokenType::LEFT_PAREN})) {
            std::vector<std::unique_ptr<Expr>> arguments;
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    if (arguments.size() >= 255) {
                        error(peek(), "Can't have more than 255 arguments.");
                    }
                    arguments.push_back(expression());
                } while (match({TokenType::COMMA}));
            }
            
            Token paren = consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");
            expr = std::make_unique<CallExpr>(std::move(expr), paren, std::move(arguments));
        } else if (match({TokenType::DOT})) {
            Token name = consume(TokenType::IDENTIFIER, "Expect property name after '.'.");
            expr = std::make_unique<MemberExpr>(std::move(expr), name);
        } else if (match({TokenType::LEFT_BRACKET})) {
            std::unique_ptr<Expr> index = expression();
            consume(TokenType::RIGHT_BRACKET, "Expect ']' after index.");
            expr = std::make_unique<IndexGetExpr>(std::move(expr), std::move(index));
        } else if (match({TokenType::PLUS_PLUS, TokenType::MINUS_MINUS})) {
            // Handle postfix increment and decrement (x++, x--)
            Token op = previous();
            
            // The operand must be an lvalue
            if (expr->type != ExprType::VARIABLE && 
                expr->type != ExprType::MEMBER && 
                expr->type != ExprType::INDEX_GET) {
                error(op, "Invalid operand for postfix increment/decrement operator.");
            }
            
            // Convert x++ to x = x + 1 or x-- to x = x - 1
            Token binaryOp = Token(op.type == TokenType::PLUS_PLUS ? TokenType::PLUS : TokenType::MINUS, 
                                   op.type == TokenType::PLUS_PLUS ? "+" : "-", op.line);
            
            // Create: operand + 1 or operand - 1
            auto one = std::make_unique<LiteralExpr>(1.0, op.line);
            
            // Clone the operand for the binary expression
            std::unique_ptr<Expr> operandCopy;
            if (expr->type == ExprType::VARIABLE) {
                auto varExpr = static_cast<VariableExpr*>(expr.get());
                operandCopy = std::make_unique<VariableExpr>(varExpr->name);
            } else {
                // For member access or index access, we'll need to handle this more carefully
                error(op, "Postfix increment/decrement on complex expressions not yet supported.");
            }
            
            auto binaryExpr = std::make_unique<BinaryExpr>(std::move(operandCopy), binaryOp, std::move(one));
            
            // Create assignment: operand = (operand + 1)
            // Note: This doesn't properly implement postfix semantics (returning old value)
            // but it's a reasonable approximation for now
            if (expr->type == ExprType::VARIABLE) {
                auto varExpr = static_cast<VariableExpr*>(expr.get());
                expr = std::make_unique<AssignExpr>(varExpr->name, std::move(binaryExpr));
            }
        } else {
            break;
        }
    }
    
    return expr;
}

std::unique_ptr<Expr> Parser::primary() {
    if (match({TokenType::FALSE})) {
        return std::make_unique<LiteralExpr>(false, previous().line);
    }
    
    if (match({TokenType::TRUE})) {
        return std::make_unique<LiteralExpr>(true, previous().line);
    }
    
    if (match({TokenType::NIL})) {
        auto expr = std::make_unique<LiteralExpr>();
        expr->line = previous().line;
        return expr;
    }
    
    if (match({TokenType::NUMBER})) {
        // Parse the number from the lexeme
        double num = std::stod(previous().lexeme);
        return std::make_unique<LiteralExpr>(num, previous().line);
    }
    
    if (match({TokenType::STRING})) {
        // Create a string value from the lexeme
        std::string str = previous().lexeme;
        auto expr = std::make_unique<LiteralExpr>(str, previous().line);
        return expr;
    }
    
    if (match({TokenType::THIS})) {
        return std::make_unique<ThisExpr>(previous());
    }
    
    if (match({TokenType::IDENTIFIER})) {
        return std::make_unique<VariableExpr>(previous());
    }
    
    if (match({TokenType::LEFT_PAREN})) {
        std::unique_ptr<Expr> expr = expression();
        consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
        return std::make_unique<GroupingExpr>(std::move(expr));
    }
    
    // Handle array literals
    if (match({TokenType::LEFT_BRACKET})) {
        return arrayLiteral();
    }
    
    // Handle object literals
    if (match({TokenType::LEFT_BRACE})) {
        return objectLiteral();
    }
    
    // Handle lambda/anonymous functions
    if (match({TokenType::FUN})) {
        return lambdaFunction();
    }
    
    error(peek(), "Expect expression.");
    return nullptr; // This line should never be reached
}

std::unique_ptr<Expr> Parser::arrayLiteral() {
    std::vector<std::unique_ptr<Expr>> elements;
    
    // Handle empty array
    if (!check(TokenType::RIGHT_BRACKET)) {
        do {
            if (elements.size() >= 255) {
                error(peek(), "Can't have more than 255 elements in an array.");
            }
            
            elements.push_back(expression());
        } while (match({TokenType::COMMA}));
    }
    
    consume(TokenType::RIGHT_BRACKET, "Expect ']' after array elements.");
    
    return std::make_unique<ArrayExpr>(std::move(elements));
}

std::unique_ptr<Expr> Parser::objectLiteral() {
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> properties;
    
    // Handle empty object
    if (!check(TokenType::RIGHT_BRACE)) {
        do {
            // Expect a string key
            Token key = consume(TokenType::STRING, "Expect property key as string.");
            
            // For STRING tokens, lexeme already contains the unquoted string
            std::string keyStr = key.lexeme;
            
            consume(TokenType::COLON, "Expect ':' after property key.");
            
            // Parse the value
            std::unique_ptr<Expr> value = expression();
            
            properties.push_back({keyStr, std::move(value)});
        } while (match({TokenType::COMMA}));
    }
    
    consume(TokenType::RIGHT_BRACE, "Expect '}' after object literal.");
    
    return std::make_unique<ObjectExpr>(std::move(properties));
}

std::unique_ptr<Expr> Parser::lambdaFunction() {
    // fun ( params ) { body }
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'fun' keyword.");
    
    std::vector<Token> params;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            if (params.size() >= 255) {
                error(peek(), "Can't have more than 255 parameters.");
            }
            
            Token param = consume(TokenType::IDENTIFIER, "Expect parameter name.");
            params.push_back(param);
        } while (match({TokenType::COMMA}));
    }
    
    consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TokenType::LEFT_BRACE, "Expect '{' before lambda body.");
    
    // Parse the body statements
    std::vector<std::unique_ptr<Stmt>> body;
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        body.push_back(statement());
    }
    
    consume(TokenType::RIGHT_BRACE, "Expect '}' after lambda body.");
    
    return std::make_unique<FunctionExpr>(params, std::move(body));
}

bool Parser::isAtEnd() {
    return peek().type == TokenType::END_OF_FILE;
}

Token Parser::advance() {
    if (!isAtEnd()) current++;
    return previous();
}

bool Parser::check(TokenType type) {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for (TokenType type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    
    return false;
}

Token Parser::peek() {
    return tokens[current];
}

Token Parser::previous() {
    if (current > 0 && current <= static_cast<int>(tokens.size())) {
        return tokens[current - 1];
    }
    // Return a dummy token if we're out of bounds
    return Token(TokenType::NIL, "", 0);
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    
    error(peek(), message);
    // error() throws, this is unreachable but satisfies compiler
#if defined(_MSC_VER)
    __assume(false);
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_unreachable();
#endif
}

void Parser::error(Token token, const std::string& message) {
    ErrorHandler::reportSyntaxError(message, token);
    throw ParserException();
}

void Parser::synchronize() {
    advance();
    
    while (!isAtEnd()) {
        if (previous().type == TokenType::SEMICOLON) return;
        
        switch (peek().type) {
            case TokenType::CLASS:
            case TokenType::FUN:
            case TokenType::VAR:
            case TokenType::FOR:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::SAY:
            case TokenType::RETURN:
                return;
            default:
                break;
        }
        
        advance();
    }
}

std::unique_ptr<Stmt> Parser::functionDeclaration() {
    Token name = consume(TokenType::IDENTIFIER, "Expect function name.");
    
    consume(TokenType::LEFT_PAREN, "Expect '(' after function name.");
    
    std::vector<FunctionParam> parameters;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            if (parameters.size() >= 255) {
                error(peek(), "Can't have more than 255 parameters.");
            }
            
            // Check for optional type annotation before parameter name
            std::optional<Token> typeAnnotation = std::nullopt;
            if (match({TokenType::TYPE_INT, TokenType::TYPE_FLOAT, TokenType::TYPE_STRING, 
                       TokenType::TYPE_BOOL, TokenType::TYPE_ARRAY, TokenType::TYPE_OBJECT, 
                       TokenType::TYPE_ANY})) {
                typeAnnotation = previous();
            }
            
            Token paramName = consume(TokenType::IDENTIFIER, "Expect parameter name.");
            parameters.emplace_back(paramName, typeAnnotation);
            
        } while (match({TokenType::COMMA}));
    }
    
    consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
    
    // Check for optional return type annotation
    std::optional<Token> returnType = std::nullopt;
    if (match({TokenType::RETURN_ARROW})) {
        if (match({TokenType::TYPE_INT, TokenType::TYPE_FLOAT, TokenType::TYPE_STRING, 
                   TokenType::TYPE_BOOL, TokenType::TYPE_ARRAY, TokenType::TYPE_OBJECT, 
                   TokenType::TYPE_ANY})) {
            returnType = previous();
        } else {
            error(peek(), "Expect return type after '->'.");
        }
    }
    
    consume(TokenType::LEFT_BRACE, "Expect '{' before function body.");
    
    std::vector<std::unique_ptr<Stmt>> body;
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        body.push_back(statement());
    }
    
    consume(TokenType::RIGHT_BRACE, "Expect '}' after function body.");
    
    return std::make_unique<FunctionStmt>(name, std::move(parameters), std::move(body), returnType);
}

std::unique_ptr<Stmt> Parser::returnStatement() {
    std::unique_ptr<Expr> value = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        value = expression();
    }
    consume(TokenType::SEMICOLON, "Expect ';' after return value.");
    return std::make_unique<ReturnStmt>(std::move(value));
}

void LiteralExpr::accept(Compiler* compiler) const {
    compiler->visitLiteralExpr(this);
}

void VariableExpr::accept(Compiler* compiler) const {
    compiler->visitVariableExpr(this);
}

void BinaryExpr::accept(Compiler* compiler) const {
    compiler->visitBinaryExpr(this);
}

void UnaryExpr::accept(Compiler* compiler) const {
    compiler->visitUnaryExpr(this);
}

void GroupingExpr::accept(Compiler* compiler) const {
    compiler->visitGroupingExpr(this);
}

void MemberExpr::accept(Compiler* compiler) const {
    compiler->visitMemberExpr(this);
}

void CallExpr::accept(Compiler* compiler) const {
    compiler->visitCallExpr(this);
}

void AssignExpr::accept(Compiler* compiler) const {
    compiler->visitAssignExpr(this);
}

void ObjectExpr::accept(Compiler* compiler) const {
    compiler->visitObjectExpr(this);
}

void ArrayExpr::accept(Compiler* compiler) const {
    compiler->visitArrayExpr(this);
}

void IndexGetExpr::accept(Compiler* compiler) const {
    compiler->visitIndexGetExpr(this);
}

void IndexSetExpr::accept(Compiler* compiler) const {
    compiler->visitIndexSetExpr(this);
}

void ExpressionStmt::accept(Compiler* compiler) const {
    compiler->visitExpressionStmt(this);
}

void SayStmt::accept(Compiler* compiler) const {
    compiler->visitSayStmt(this);
}

void VarStmt::accept(Compiler* compiler) const {
    compiler->visitVarStmt(this);
}

void AssignStmt::accept(Compiler* /*compiler*/) const {
}


void BlockStmt::accept(Compiler* compiler) const {
    compiler->visitBlockStmt(this);
}

void IfStmt::accept(Compiler* compiler) const {
    compiler->visitIfStmt(this);
}

void WhileStmt::accept(Compiler* compiler) const {
    compiler->visitWhileStmt(this);
}

void UseStmt::accept(Compiler* compiler) const {
    compiler->visitUseStmt(this);
}

void FunctionStmt::accept(Compiler* compiler) const {
    compiler->visitFunctionStmt(this);
}

void ReturnStmt::accept(Compiler* compiler) const {
    compiler->visitReturnStmt(this);
}

void ClassStmt::accept(Compiler* compiler) const {
    compiler->visitClassStmt(this);
}

void BreakStmt::accept(Compiler* compiler) const {
    compiler->visitBreakStmt(this);
}

void ContinueStmt::accept(Compiler* compiler) const {
    compiler->visitContinueStmt(this);
}

void MemberSetExpr::accept(Compiler* compiler) const {
    compiler->visitMemberSetExpr(this);
}

void ThisExpr::accept(Compiler* compiler) const {
    compiler->visitThisExpr(this);
}

void FunctionExpr::accept(Compiler* compiler) const {
    compiler->visitFunctionExpr(this);
}

void TernaryExpr::accept(Compiler* compiler) const {
    compiler->visitTernaryExpr(this);
}

void DoWhileStmt::accept(Compiler* compiler) const {
    compiler->visitDoWhileStmt(this);
}

void MatchStmt::accept(Compiler* compiler) const {
    compiler->visitMatchStmt(this);
}

void TryStmt::accept(Compiler* compiler) const {
    compiler->visitTryStmt(this);
}

void ThrowStmt::accept(Compiler* compiler) const {
    compiler->visitThrowStmt(this);
}

void RetryStmt::accept(Compiler* compiler) const {
    compiler->visitRetryStmt(this);
}

void SafeStmt::accept(Compiler* compiler) const {
    compiler->visitSafeStmt(this);
}

std::unique_ptr<Stmt> Parser::matchStatement() {
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'match'.");
    auto expr = expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after match expression.");
    consume(TokenType::LEFT_BRACE, "Expect '{' before match cases.");
    
    std::vector<MatchCase> cases;
    std::unique_ptr<Stmt> defaultCase = nullptr;
    
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        if (match({TokenType::CASE})) {
            // Parse case value
            auto caseValue = expression();
            consume(TokenType::ARROW, "Expect '=>' after case value.");
            
            // Parse case action (can be a block or single statement)
            std::unique_ptr<Stmt> action;
            if (check(TokenType::LEFT_BRACE)) {
                action = block();
            } else {
                action = statement();
            }
            
            MatchCase matchCase;
            matchCase.value = std::move(caseValue);
            matchCase.action = std::move(action);
            cases.push_back(std::move(matchCase));
            
        } else if (match({TokenType::DEFAULT})) {
            consume(TokenType::ARROW, "Expect '=>' after 'default'.");
            
            // Parse default action
            if (check(TokenType::LEFT_BRACE)) {
                defaultCase = block();
            } else {
                defaultCase = statement();
            }
            break;  // default must be last
        } else {
            throw std::runtime_error("Expect 'case' or 'default' in match statement.");
        }
    }
    
    consume(TokenType::RIGHT_BRACE, "Expect '}' after match cases.");
    
    return std::make_unique<MatchStmt>(std::move(expr), std::move(cases), std::move(defaultCase));
}

std::unique_ptr<Stmt> Parser::tryStatement() {
    // Expect a block after 'try' keyword
    std::unique_ptr<Stmt> tryBlock = block();
    
    Token catchVar = Token(TokenType::IDENTIFIER, "", current);  // Placeholder
    std::unique_ptr<Stmt> catchBlock = nullptr;
    std::unique_ptr<Stmt> finallyBlock = nullptr;
    
    if (match({TokenType::CATCH})) {
        consume(TokenType::LEFT_PAREN, "Expect '(' after 'catch'.");
        catchVar = consume(TokenType::IDENTIFIER, "Expect exception variable name.");
        consume(TokenType::RIGHT_PAREN, "Expect ')' after exception variable.");
        // The parser is now at '{' after ')', so call block() which expects '{'
        catchBlock = block();
    }
    
    if (match({TokenType::FINALLY})) {
        // The parser is now at '{' after 'finally', so call block() which expects '{'
        finallyBlock = block();
    }
    
    // In standard exception handling, a try block must have at least a catch or finally clause
    // However, for the Neutron language design, we'll allow try blocks to exist alone
    // This would be for cases where you just want to execute code that might throw
    // and let any exceptions propagate up (which is handled by the VM)
    
    return std::make_unique<TryStmt>(std::move(tryBlock), catchVar, 
                                      std::move(catchBlock), std::move(finallyBlock));
}

std::unique_ptr<Stmt> Parser::throwStatement() {
    auto value = expression();
    consume(TokenType::SEMICOLON, "Expect ';' after throw expression.");
    return std::make_unique<ThrowStmt>(std::move(value));
}

std::unique_ptr<Stmt> Parser::retryStatement() {
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'retry'.");
    auto count = expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after retry count.");
    
    std::unique_ptr<Stmt> body = block();
    
    Token catchVar = Token(TokenType::IDENTIFIER, "", current);
    std::unique_ptr<Stmt> catchBlock = nullptr;
    
    if (match({TokenType::CATCH})) {
        consume(TokenType::LEFT_PAREN, "Expect '(' after 'catch'.");
        catchVar = consume(TokenType::IDENTIFIER, "Expect exception variable name.");
        consume(TokenType::RIGHT_PAREN, "Expect ')' after exception variable.");
        catchBlock = block();
    }
    
    return std::make_unique<RetryStmt>(std::move(count), std::move(body), catchVar, std::move(catchBlock));
}

std::unique_ptr<Stmt> Parser::safeStatement() {
    consume(TokenType::LEFT_BRACE, "Expect '{' after 'safe'.");
    
    // Parse the block inside the safe statement
    std::vector<std::unique_ptr<Stmt>> statements;
    
    while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
        statements.push_back(statement());
    }
    
    consume(TokenType::RIGHT_BRACE, "Expect '}' after safe block.");
    
    // Create a block statement for the safe body
    auto body = std::make_unique<BlockStmt>(std::move(statements), true);
    
    return std::make_unique<SafeStmt>(std::move(body));
}

} // namespace neutron
