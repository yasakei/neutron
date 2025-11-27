#ifndef NEUTRON_PARSER_H
#define NEUTRON_PARSER_H

#include "token.h"
#include "expr.h"
#include <vector>
#include <memory>

namespace neutron {

class Parser {
private:
    std::vector<Token> tokens;
    int current;

    bool isAtEnd();
    Token advance();
    bool check(TokenType type);
    bool match(std::initializer_list<TokenType> types);
    Token peek();
    Token previous();
    Token consume(TokenType type, const std::string& message);
    
    // Error handling
    [[noreturn]] void error(Token token, const std::string& message);
    
    // Parsing methods
    std::unique_ptr<Expr> expression();
    std::unique_ptr<Expr> assignment();
    std::unique_ptr<Expr> logic_or();
    std::unique_ptr<Expr> logic_and();
    std::unique_ptr<Expr> equality();
    std::unique_ptr<Expr> comparison();
    std::unique_ptr<Expr> term();
    std::unique_ptr<Expr> factor();
    std::unique_ptr<Expr> unary();
    std::unique_ptr<Expr> call();
    std::unique_ptr<Expr> primary();
    std::unique_ptr<Expr> objectLiteral();
    std::unique_ptr<Expr> arrayLiteral();
    std::unique_ptr<Expr> lambdaFunction();
    
    std::unique_ptr<Stmt> statement();
    std::unique_ptr<Stmt> sayStatement();
    std::unique_ptr<Stmt> expressionStatement();
    std::unique_ptr<Stmt> varDeclaration(bool isStatic = false);
    std::unique_ptr<Stmt> ifStatement();
    std::unique_ptr<Stmt> whileStatement();
    std::unique_ptr<Stmt> forStatement();
    std::unique_ptr<Stmt> classDeclaration();
    std::unique_ptr<Stmt> useStatement();
    std::unique_ptr<Stmt> functionDeclaration();
    std::unique_ptr<Stmt> returnStatement();
    std::unique_ptr<Stmt> matchStatement();
    std::unique_ptr<Stmt> tryStatement();
    std::unique_ptr<Stmt> throwStatement();
    
    std::unique_ptr<Stmt> block();
    
    // Synchronization for error recovery
    void synchronize();

public:
    Parser(const std::vector<Token>& tokens);
    std::vector<std::unique_ptr<Stmt>> parse();
};

} // namespace neutron

#endif // NEUTRON_PARSER_H