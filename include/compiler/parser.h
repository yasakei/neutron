#ifndef NEUTRON_PARSER_H
#define NEUTRON_PARSER_H

/*
 * Code Documentation: Recursive Descent Parser (parser.h)
 * =======================================================
 * 
 * This header defines the Parser class - the second phase of the compiler
 * pipeline. It transforms tokens into an Abstract Syntax Tree (AST).
 * 
 * What This File Includes:
 * ------------------------
 * - Parser class: Recursive descent parser for Neutron
 * - Expression parsing: Full operator precedence hierarchy
 * - Statement parsing: All statement types (var, if, while, class, etc.)
 * - Error recovery: Synchronization for continued parsing after errors
 * 
 * How It Works:
 * -------------
 * The parser uses recursive descent with Pratt parsing for expressions:
 * 
 * Expression Precedence (lowest to highest):
 *   assignment, ternary, logic_or, logic_and, bitwise_or, bitwise_xor,
 *   bitwise_and, equality, comparison, bitwise_shift, term, factor,
 *   unary, call, primary
 * 
 * Each precedence level has a corresponding method. Lower-precedence
 * methods call higher-precedence methods, creating the correct
 * associativity and precedence.
 * 
 * Error Recovery:
 * The synchronize() method skips tokens until a statement boundary
 * is found, allowing the parser to continue and report multiple errors.
 * 
 * Adding Features:
 * ----------------
 * - New operators: Add to appropriate precedence level, update grammar
 * - New statements: Add parse method, update statement() dispatcher
 * - New expressions: Add to expression hierarchy, update visitor pattern
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT modify the token stream during parsing
 * - Do NOT skip error recovery (it enables multiple error reports)
 * - Do NOT change precedence without understanding operator associativity
 * - Do NOT forget to update expr.h/stmt.h when adding new node types
 * 
 * Grammar Reference:
 * ------------------
 * program        → declaration*
 * declaration    → varDecl | classDecl | funDecl | statement
 * varDecl        → "var" IDENTIFIER "=" expression ";"
 * expression     → assignment | ternary
 * assignment     → IDENTIFIER "=" assignment | logic_or
 * ternary        → logic_and ("?" expression ":" expression)?
 * logic_or       → logic_and ("or" logic_and)*
 * logic_and      → bitwise_and ("and" bitwise_and)*
 * ... (continues down precedence hierarchy)
 */

#include "token.h"
#include "expr.h"
#include <vector>
#include <memory>

namespace neutron {

/**
 * @brief Parser - The Syntax Analyzer: Turning tokens into trees.
 * 
 * The Parser is the second stage of the compiler pipeline. It consumes
 * tokens from the Scanner and produces an Abstract Syntax Tree (AST).
 * 
 * The parser implements:
 * - Recursive descent for statements and declarations
 * - Pratt parsing for expressions (operator precedence parsing)
 * - Error recovery via synchronization at statement boundaries
 * 
 * Error Handling:
 * When a syntax error is detected:
 * 1. Report the error with location information
 * 2. Synchronize to the next statement boundary
 * 3. Continue parsing to find more errors
 * 
 * This approach maximizes error detection in a single pass.
 * 
 * Example:
 * @code
 * Parser parser(tokens);
 * auto statements = parser.parse();
 * // statements: AST nodes representing the program
 * @endcode
 */
class Parser {
private:
    std::vector<Token> tokens;  ///< Token stream from scanner
    int current;                ///< Current token index

    // Token stream navigation
    bool isAtEnd();             ///< Check if at end of tokens
    Token advance();            ///< Advance and return current token
    bool check(TokenType type); ///< Check if current token matches type
    bool match(std::initializer_list<TokenType> types);  ///< Match any of given types
    Token peek();               ///< Look at current token
    Token previous();           ///< Look at previous token
    Token consume(TokenType type, const std::string& message);  ///< Expect and consume

    // Error handling
    void error(Token token, const std::string& message);  ///< Report syntax error

    // Expression parsing (precedence hierarchy - lowest to highest)
    std::unique_ptr<Expr> expression();         ///< Parse any expression
    std::unique_ptr<Expr> assignment();         ///< Parse assignment (=)
    std::unique_ptr<Expr> ternary();            ///< Parse ternary (? :)
    std::unique_ptr<Expr> logic_or();           ///< Parse logical or
    std::unique_ptr<Expr> logic_and();          ///< Parse logical and
    std::unique_ptr<Expr> bitwise_or();         ///< Parse bitwise or (|)
    std::unique_ptr<Expr> bitwise_xor();        ///< Parse bitwise xor (^)
    std::unique_ptr<Expr> bitwise_and();        ///< Parse bitwise and (&)
    std::unique_ptr<Expr> equality();           ///< Parse equality (==, !=)
    std::unique_ptr<Expr> comparison();         ///< Parse comparison (<, >, <=, >=)
    std::unique_ptr<Expr> bitwise_shift();      ///< Parse shift (<<, >>)
    std::unique_ptr<Expr> term();               ///< Parse addition/subtraction (+, -)
    std::unique_ptr<Expr> factor();             ///< Parse multiplication/division (*, /, %)
    std::unique_ptr<Expr> unary();              ///< Parse unary (!, -, not)
    std::unique_ptr<Expr> call();               ///< Parse function calls and member access
    std::unique_ptr<Expr> primary();            ///< Parse literals, identifiers, grouping

    // Literal and composite expressions
    std::unique_ptr<Expr> objectLiteral();      ///< Parse object literals {key: value}
    std::unique_ptr<Expr> arrayLiteral();       ///< Parse array literals [items]
    std::unique_ptr<Expr> lambdaFunction();     ///< Parse lambda functions

    // Statement parsing
    std::unique_ptr<Stmt> statement();          ///< Parse any statement
    std::unique_ptr<Stmt> sayStatement();       ///< Parse say (print) statement
    std::unique_ptr<Stmt> expressionStatement();///< Parse expression as statement
    std::unique_ptr<Stmt> varDeclaration(bool isStatic = false);  ///< Parse var declaration
    std::unique_ptr<Stmt> ifStatement();        ///< Parse if statement
    std::unique_ptr<Stmt> whileStatement();     ///< Parse while loop
    std::unique_ptr<Stmt> doWhileStatement();   ///< Parse do-while loop
    std::unique_ptr<Stmt> forStatement();       ///< Parse for loop
    std::unique_ptr<Stmt> classDeclaration();   ///< Parse class definition
    std::unique_ptr<Stmt> useStatement();       ///< Parse use (import) statement
    std::unique_ptr<Stmt> functionDeclaration();///< Parse function definition
    std::unique_ptr<Stmt> returnStatement();    ///< Parse return statement
    std::unique_ptr<Stmt> matchStatement();     ///< Parse match (pattern matching)
    std::unique_ptr<Stmt> tryStatement();       ///< Parse try-catch-finally
    std::unique_ptr<Stmt> throwStatement();     ///< Parse throw statement
    std::unique_ptr<Stmt> retryStatement();     ///< Parse retry statement
    std::unique_ptr<Stmt> safeStatement();      ///< Parse safe block
    std::unique_ptr<Stmt> enumDeclaration();    ///< Parse enum declaration

    std::unique_ptr<Stmt> block();              ///< Parse block { statements }

    // Error recovery
    void synchronize();  ///< Skip tokens to next statement boundary

public:
    /**
     * @brief Construct a parser for the given tokens.
     * @param tokens The token stream from the scanner.
     */
    Parser(const std::vector<Token>& tokens);
    
    /**
     * @brief Parse the entire token stream and return the AST.
     * @return Vector of statement nodes (the program AST).
     */
    std::vector<std::unique_ptr<Stmt>> parse();
};

} // namespace neutron

#endif // NEUTRON_PARSER_H