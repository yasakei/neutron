#ifndef NEUTRON_TOKEN_H
#define NEUTRON_TOKEN_H

#include <string>
#include <iostream>

namespace neutron {

enum class TokenType {
    // Single-character tokens
    LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,
    LEFT_BRACKET, RIGHT_BRACKET,
    COMMA, DOT, MINUS, PLUS, SEMICOLON, SLASH, STAR, COLON, PERCENT, AMPERSAND, PIPE, CARET, TILDE, QUESTION,

    // One or two character tokens
    BANG, BANG_EQUAL,
    EQUAL, EQUAL_EQUAL,
    GREATER, GREATER_EQUAL, GREATER_GREATER,
    LESS, LESS_EQUAL, LESS_LESS,
    PLUS_PLUS, MINUS_MINUS,  // ++ and --
    AND_SYM, OR_SYM,        // && and ||
    ARROW,  // => for match cases

    // Literals
    IDENTIFIER, STRING, NUMBER,

    // Keywords
    AND, CLASS, ELSE, ELIF, FALSE, FUN, FOR, IF, NIL, OR,
    SAY, RETURN, STATIC, SUPER, THIS, TRUE, VAR, WHILE, DO,
    BREAK, CONTINUE,
    MATCH, CASE, DEFAULT,  // Match statement
    TRY, CATCH, FINALLY, THROW, RETRY,  // Exception handling
    
    // Library features
    USE, USING, FROM,

    // Type annotations (optional type safety)
    TYPE_INT, TYPE_FLOAT, TYPE_STRING, TYPE_BOOL, 
    TYPE_ARRAY, TYPE_OBJECT, TYPE_ANY,
    
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;

    Token(TokenType type, const std::string& lexeme, int line)
        : type(type), lexeme(lexeme), line(line) {}
};

// Declaration of the operator<< function
std::ostream& operator<<(std::ostream& os, const Token& token);

} // namespace neutron

#endif // NEUTRON_TOKEN_H