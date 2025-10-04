#ifndef NEUTRON_SCANNER_H
#define NEUTRON_SCANNER_H

#include "token.h"
#include <string>
#include <vector>
#include <map>

namespace neutron {

class Scanner {
private:
    std::string source;
    std::vector<Token> tokens;
    int start;
    int current;
    int line;
    std::map<std::string, TokenType> keywords;

    bool isAtEnd();
    void scanToken();
    char advance();
    void addToken(TokenType type, const std::string& literal);
    void addToken(TokenType type);
    bool match(char expected);
    char peek();
    char peekNext();
    
    // Helpers for literals
    void string(char quote);
    void number();
    void identifier();

    // Helper functions
    bool isDigit(char c);
    bool isAlpha(char c);
    bool isAlphaNumeric(char c);
    TokenType identifierType();

public:
    Scanner(const std::string& source);
    std::vector<Token> scanTokens();
};

} // namespace neutron

#endif // NEUTRON_SCANNER_H