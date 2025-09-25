#ifndef NEUTRON_TOKEN_H
#define NEUTRON_TOKEN_H

#include <string>
#include <iostream>

namespace neutron {

enum class TokenType {
    // Single-character tokens
    LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,
    LEFT_BRACKET, RIGHT_BRACKET,
    COMMA, DOT, MINUS, PLUS, SEMICOLON, SLASH, STAR, COLON,

    // One or two character tokens
    BANG, BANG_EQUAL,
    EQUAL, EQUAL_EQUAL,
    GREATER, GREATER_EQUAL,
    LESS, LESS_EQUAL,

    // Literals
    IDENTIFIER, STRING, NUMBER,

    // Keywords
    AND, CLASS, ELSE, FALSE, FUN, FOR, IF, NIL, OR,
    SAY, RETURN, SUPER, THIS, TRUE, VAR, WHILE,
    
    // Library features
    USE,

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