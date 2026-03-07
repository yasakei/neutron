#ifndef NEUTRON_SCANNER_H
#define NEUTRON_SCANNER_H

/*
 * Code Documentation: Lexical Scanner (scanner.h)
 * ===============================================
 * 
 * This header defines the Scanner class - the first phase of the compiler
 * pipeline. It transforms raw source code into a stream of tokens.
 * 
 * What This File Includes:
 * ------------------------
 * - Scanner class: Lexical analyzer for Neutron source code
 * - Token generation: Converts character sequences to typed tokens
 * - Literal handling: Strings, numbers, identifiers
 * 
 * How It Works:
 * -------------
 * The scanner uses a recursive descent approach to tokenize source:
 * 1. Scan character by character
 * 2. Recognize lexemes (identifier, number, string, operator)
 * 3. Create tokens with type, lexeme, and literal value
 * 4. Handle keywords by looking up identifiers in a keyword map
 * 
 * The scanner operates in a single pass, maintaining:
 * - start: Beginning of current token
 * - current: Current character being examined
 * - line: Current line number for error reporting
 * 
 * Adding Features:
 * ----------------
 * - New token types: Add to TokenType enum in token.h, add recognition logic
 * - New literals: Add handler method (e.g., hexNumber(), character())
 * - New keywords: Add to keywords map in constructor
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT modify the source string during scanning
 * - Do NOT skip whitespace handling (it's essential for tokenization)
 * - Do NOT change token boundaries without updating error reporting
 * - Do NOT assume tokens are valid - parser will validate syntax
 * 
 * Token Format:
 * -------------
 * Each token contains:
 * - type: The kind of token (identifier, number, operator, etc.)
 * - lexeme: The actual source text
 * - literal: The parsed value (for numbers/strings)
 * - line: Source line for error messages
 */

#include "token.h"
#include <string>
#include <vector>
#include <map>

namespace neutron {

/**
 * @brief Scanner - The Lexical Analyzer: Where source code becomes tokens.
 * 
 * The Scanner is the first stage of the compiler pipeline. It reads
 * raw source code and produces a vector of Token objects. Each token
 * represents a meaningful unit: keywords, identifiers, operators, literals.
 * 
 * Design Philosophy:
 * - Single-pass scanning for efficiency
 * - No lookahead beyond one character
 * - Keywords are identified by looking up identifiers in a map
 * 
 * Example:
 * @code
 * Scanner scanner("var x = 42;");
 * auto tokens = scanner.scanTokens();
 * // tokens: [VAR, IDENTIFIER("x"), EQUAL, NUMBER(42), SEMICOLON, EOF]
 * @endcode
 */
class Scanner {
private:
    std::string source;           ///< The source code being scanned
    std::vector<Token> tokens;    ///< Output token stream
    int start;                    ///< Start index of current token
    int current;                  ///< Current character index
    int line;                     ///< Current line number

    /// Keyword map - identifiers are looked up here to determine if they're keywords
    std::map<std::string, TokenType> keywords;

    // Core scanning methods
    bool isAtEnd();               ///< Check if we've reached end of source
    void scanToken();             ///< Scan one token
    char advance();               ///< Advance and return current character
    void addToken(TokenType type, const std::string& literal);  ///< Add token with literal
    void addToken(TokenType type);  ///< Add token without literal
    bool match(char expected);    ///< Match and consume expected character
    char peek();                  ///< Look at current character
    char peekNext();              ///< Look at next character

    // Literal handlers - these parse specific literal types
    void string(char quote);      ///< Scan a string literal
    void rawString(char quote);   ///< Scan a raw string (no escape sequences)
    void number();                ///< Scan a numeric literal
    void identifier();            ///< Scan an identifier or keyword

    // Helper predicates
    bool isDigit(char c);         ///< Check if character is a digit
    bool isAlpha(char c);         ///< Check if character is alphabetic
    bool isAlphaNumeric(char c);  ///< Check if character is alphanumeric
    TokenType identifierType();   ///< Lookup identifier in keyword map

public:
    /**
     * @brief Construct a scanner for the given source.
     * @param source The Neutron source code to tokenize.
     */
    Scanner(const std::string& source);
    
    /**
     * @brief Scan the entire source and return all tokens.
     * @return Vector of tokens, including EOF marker.
     */
    std::vector<Token> scanTokens();
};

} // namespace neutron

#endif // NEUTRON_SCANNER_H