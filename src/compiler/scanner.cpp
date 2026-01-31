/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, for both open source and commercial purposes.
 * 
 * Conditions:
 * 
 * 1. The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 * 
 * 2. Attribution is appreciated but NOT required.
 *    Suggested (optional) credit:
 *    "Built using Neutron Programming Language (c) yasakei"
 * 
 * 3. The name "Neutron" and the name of the copyright holder may not be used
 *    to endorse or promote products derived from this Software without prior
 *    written permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// Windows macro undefs - must be before any includes that might include Windows headers
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    // Undefine Windows macros that conflict with C++ code
    // NOTE: Do NOT undefine FAR, NEAR, IN, OUT as they are needed by Windows headers
    #undef TRUE
    #undef FALSE
    #undef DELETE
    #undef ERROR
    #undef OPTIONAL
    #undef interface
    #undef small
    #undef max
    #undef min
#endif

#include "compiler/scanner.h"
#include "token.h"
#include "runtime/error_handler.h"
#include <stdexcept>
#include <string>

namespace neutron {

Scanner::Scanner(const std::string& source)
    : source(source), start(0), current(0), line(1) {
    keywords["and"] = TokenType::AND;
    keywords["class"] = TokenType::CLASS;
    keywords["elif"] = TokenType::ELIF;
    keywords["else"] = TokenType::ELSE;
    keywords["false"] = TokenType::FALSE;
    keywords["for"] = TokenType::FOR;
    keywords["fun"] = TokenType::FUN;
    keywords["if"] = TokenType::IF;
    keywords["nil"] = TokenType::NIL;
    keywords["or"] = TokenType::OR;
    keywords["say"] = TokenType::SAY;
    keywords["return"] = TokenType::RETURN;
    keywords["static"] = TokenType::STATIC;
    keywords["super"] = TokenType::SUPER;
    keywords["this"] = TokenType::THIS;
    keywords["true"] = TokenType::TRUE;
    keywords["var"] = TokenType::VAR;
    keywords["while"] = TokenType::WHILE;
    keywords["do"] = TokenType::DO;
    keywords["break"] = TokenType::BREAK;
    keywords["continue"] = TokenType::CONTINUE;
    keywords["match"] = TokenType::MATCH;
    keywords["case"] = TokenType::CASE;
    keywords["default"] = TokenType::DEFAULT;
    keywords["try"] = TokenType::TRY;
    keywords["catch"] = TokenType::CATCH;
    keywords["finally"] = TokenType::FINALLY;
    keywords["throw"] = TokenType::THROW;
    keywords["retry"] = TokenType::RETRY;
    keywords["safe"] = TokenType::SAFE;
    keywords["use"] = TokenType::USE;
    keywords["using"] = TokenType::USING;
    keywords["from"] = TokenType::FROM;
    
    // Type annotations (optional type safety)
    keywords["int"] = TokenType::TYPE_INT;
    keywords["float"] = TokenType::TYPE_FLOAT;
    keywords["string"] = TokenType::TYPE_STRING;
    keywords["bool"] = TokenType::TYPE_BOOL;
    keywords["array"] = TokenType::TYPE_ARRAY;
    keywords["object"] = TokenType::TYPE_OBJECT;
    keywords["any"] = TokenType::TYPE_ANY;
}

std::vector<Token> Scanner::scanTokens() {
    while (!isAtEnd()) {
        start = current;
        scanToken();
    }

    tokens.push_back(Token(TokenType::END_OF_FILE, "", line));
    return tokens;
}

bool Scanner::isAtEnd() {
    return current >= static_cast<int>(source.length());
}

void Scanner::scanToken() {
    char c = advance();
    switch (c) {
        case '(': addToken(TokenType::LEFT_PAREN); break;
        case ')': addToken(TokenType::RIGHT_PAREN); break;
        case '{': addToken(TokenType::LEFT_BRACE); break;
        case '}': addToken(TokenType::RIGHT_BRACE); break;
        case '[': addToken(TokenType::LEFT_BRACKET); break;
        case ']': addToken(TokenType::RIGHT_BRACKET); break;
        case ',': addToken(TokenType::COMMA); break;
        case '.': addToken(TokenType::DOT); break;
        case '-': 
            if (match('-')) {
                addToken(TokenType::MINUS_MINUS);
            } else if (match('>')) {
                addToken(TokenType::RETURN_ARROW);  // ->
            } else {
                addToken(TokenType::MINUS);
            }
            break;
        case '+': addToken(match('+') ? TokenType::PLUS_PLUS : TokenType::PLUS); break;
        case ';': addToken(TokenType::SEMICOLON); break;
        case ':': addToken(TokenType::COLON); break;
        case '*': addToken(TokenType::STAR); break;
        case '&': addToken(match('&') ? TokenType::AND_SYM : TokenType::AMPERSAND); break;
        case '|': addToken(match('|') ? TokenType::OR_SYM : TokenType::PIPE); break;
        case '%': addToken(TokenType::PERCENT); break;
        case '^': addToken(TokenType::CARET); break;
        case '~': addToken(TokenType::TILDE); break;
        case '?': addToken(TokenType::QUESTION); break;
        case '!': addToken(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG); break;
        case '=': 
            if (match('=')) {
                addToken(TokenType::EQUAL_EQUAL);
            } else if (match('>')) {
                addToken(TokenType::ARROW);  // =>
            } else {
                addToken(TokenType::EQUAL);
            }
            break;
        case '<': 
            if (match('=')) {
                addToken(TokenType::LESS_EQUAL);
            } else if (match('<')) {
                addToken(TokenType::LESS_LESS);
            } else {
                addToken(TokenType::LESS);
            }
            break;
        case '>': 
            if (match('=')) {
                addToken(TokenType::GREATER_EQUAL);
            } else if (match('>')) {
                addToken(TokenType::GREATER_GREATER);
            } else {
                addToken(TokenType::GREATER);
            }
            break;
        case '/':
            if (match('/')) {
                while (peek() != '\n' && !isAtEnd()) advance();
            } else {
                addToken(TokenType::SLASH);
            }
            break;
        case ' ':
        case '\r':
        case '\t':
            break;
        case '\n':
            line++;
            break;
        case '"': string('"'); break;
        case '\'': string('\''); break;
        default:
            if (isDigit(c)) {
                number();
            } else if (isAlpha(c)) {
                identifier();
            } else {
                std::string msg = "Unexpected character '";
                msg += c;
                msg += "'";
                ErrorHandler::reportLexicalError(msg, line, current - start);
                // Don't exit, just continue scanning
            }
            break;
    }
}

char Scanner::advance() {
    current++;
    return source[current - 1];
}

void Scanner::addToken(TokenType type) {
    addToken(type, "");
}

void Scanner::addToken(TokenType type, const std::string& literal) {
    if (type == TokenType::STRING) {
        tokens.push_back(Token(type, literal, line));
    } else {
        std::string text = source.substr(start, current - start);
        tokens.push_back(Token(type, text, line));
    }
}

bool Scanner::match(char expected) {
    if (isAtEnd()) return false;
    if (source[current] != expected) return false;

    current++;
    return true;
}

char Scanner::peek() {
    if (isAtEnd()) return '\0';
    return source[current];
}

char Scanner::peekNext() {
    if (current + 1 >= static_cast<int>(source.length())) return '\0';
    return source[current + 1];
}

void Scanner::string(char quote) {
    std::string value;
    bool hasInterpolation = false;
    std::vector<std::vector<Token>> chunks;  // Each chunk is either a STRING token or an expression group
    
    while (peek() != quote && !isAtEnd()) {
        if (peek() == '\n') line++;
        
        /*
         * String Interpolation: ${expression}
         * 
         * Neutron supports inline expression evaluation within strings:
         *   "Hello ${name}!"
         *   "Sum: ${a + b}"
         * 
         * Implementation Strategy:
         * 1. Split string into chunks at ${} boundaries
         * 2. String chunks become STRING tokens
         * 3. Expression chunks become token sequences wrapped in parentheses
         * 4. Parser reconstructs interpolated strings from chunks
         * 
         * Brace Balancing:
         * We track brace depth to handle nested braces in expressions:
         *   "Result: ${obj.method({ a: 1 })}" 
         * This ensures we don't prematurely close on inner braces.
         */
        if (peek() == '$' && peekNext() == '{') {
            hasInterpolation = true;
            
            if (!value.empty()) {
                std::vector<Token> stringChunk;
                stringChunk.push_back(Token(TokenType::STRING, value, line));
                chunks.push_back(stringChunk);
                value.clear();
            }
            
            advance();
            advance();
            
            int braceDepth = 1;
            int exprStart = current;
            
            while (braceDepth > 0 && !isAtEnd()) {
                if (peek() == '{') braceDepth++;
                else if (peek() == '}') braceDepth--;
                
                if (braceDepth > 0) {
                    advance();
                }
            }
            
            if (isAtEnd()) {
                ErrorHandler::reportLexicalError("Unterminated string interpolation - missing closing '}'", 
                                                line, current - start);
                exit(1);
            }
            
            std::string expr = source.substr(exprStart, current - exprStart);
            
            /*
             * Sub-scanner for interpolated expressions:
             * We create a separate scanner instance to tokenize the expression.
             * This allows complex expressions with operators, calls, etc.
             * The tokens are wrapped in parentheses to maintain proper precedence.
             */
            Scanner exprScanner(expr);
            std::vector<Token> exprTokens = exprScanner.scanTokens();
            
            if (!exprTokens.empty() && exprTokens.back().type == TokenType::END_OF_FILE) {
                exprTokens.pop_back();
            }
            
            std::vector<Token> exprChunk;
            exprChunk.push_back(Token(TokenType::LEFT_PAREN, "(", line));
            for (const auto& token : exprTokens) {
                exprChunk.push_back(token);
            }
            exprChunk.push_back(Token(TokenType::RIGHT_PAREN, ")", line));
            chunks.push_back(exprChunk);
            
            advance();
            
        } else if (peek() == '\\') {
            /*
             * Escape Sequence Processing:
             * 
             * Standard Escapes:
             * \n, \t, \r, \\, \", \', \a, \b, \f, \v - Single character escapes
             * 
             * Unicode Escapes:
             * \uXXXX - 4-digit hex for Basic Multilingual Plane (U+0000 to U+FFFF)
             * \UXXXXXXXX - 8-digit hex for full Unicode range (U+00000000 to U+10FFFF)
             * 
             * UTF-8 Encoding:
             * Unicode codepoints are encoded into UTF-8 byte sequences:
             * - 1 byte: 0x00-0x7F (ASCII)
             * - 2 bytes: 0x80-0x7FF
             * - 3 bytes: 0x800-0xFFFF
             * - 4 bytes: 0x10000-0x10FFFF
             */
            advance();
            if (!isAtEnd()) {
                char escaped = advance();
                switch (escaped) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '"': value += '"'; break;
                    case '\'': value += '\''; break;
                    case 'a': value += '\a'; break;  // Bell/alert
                    case 'b': value += '\b'; break;  // Backspace
                    case 'f': value += '\f'; break;  // Form feed
                    case 'v': value += '\v'; break;  // Vertical tab
                    case '0': value += '\0'; break;  // Null character
                    case 'u': {
                        // Unicode escape \uXXXX
                        std::string hexDigits;
                        for (int i = 0; i < 4 && !isAtEnd(); i++) {
                            char c = peek();
                            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                                hexDigits += advance();
                            } else {
                                ErrorHandler::reportLexicalError("Invalid Unicode escape sequence - expected 4 hex digits", 
                                                               line, current - start);
                                exit(1);
                            }
                        }
                        if (hexDigits.length() == 4) {
                            // Convert hex to Unicode codepoint
                            unsigned int codepoint = std::stoul(hexDigits, nullptr, 16);
                            // Simple UTF-8 encoding for Basic Multilingual Plane
                            if (codepoint <= 0x7F) {
                                value += static_cast<char>(codepoint);
                            } else if (codepoint <= 0x7FF) {
                                value += static_cast<char>(0xC0 | (codepoint >> 6));
                                value += static_cast<char>(0x80 | (codepoint & 0x3F));
                            } else {
                                value += static_cast<char>(0xE0 | (codepoint >> 12));
                                value += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                value += static_cast<char>(0x80 | (codepoint & 0x3F));
                            }
                        }
                        break;
                    }
                    case 'U': {
                        // Unicode escape \UXXXXXXXX
                        std::string hexDigits;
                        for (int i = 0; i < 8 && !isAtEnd(); i++) {
                            char c = peek();
                            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                                hexDigits += advance();
                            } else {
                                ErrorHandler::reportLexicalError("Invalid Unicode escape sequence - expected 8 hex digits", 
                                                               line, current - start);
                                exit(1);
                            }
                        }
                        if (hexDigits.length() == 8) {
                            // Convert hex to Unicode codepoint
                            unsigned int codepoint = std::stoul(hexDigits, nullptr, 16);
                            // Simple UTF-8 encoding (supports up to 4 bytes)
                            if (codepoint <= 0x7F) {
                                value += static_cast<char>(codepoint);
                            } else if (codepoint <= 0x7FF) {
                                value += static_cast<char>(0xC0 | (codepoint >> 6));
                                value += static_cast<char>(0x80 | (codepoint & 0x3F));
                            } else if (codepoint <= 0xFFFF) {
                                value += static_cast<char>(0xE0 | (codepoint >> 12));
                                value += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                value += static_cast<char>(0x80 | (codepoint & 0x3F));
                            } else if (codepoint <= 0x10FFFF) {
                                value += static_cast<char>(0xF0 | (codepoint >> 18));
                                value += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                                value += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                value += static_cast<char>(0x80 | (codepoint & 0x3F));
                            }
                        }
                        break;
                    }
                    case 'x': {
                        // Hexadecimal escape \xXX
                        std::string hexDigits;
                        for (int i = 0; i < 2 && !isAtEnd(); i++) {
                            char c = peek();
                            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                                hexDigits += advance();
                            } else {
                                ErrorHandler::reportLexicalError("Invalid hex escape sequence - expected 2 hex digits", 
                                                               line, current - start);
                                exit(1);
                            }
                        }
                        if (hexDigits.length() == 2) {
                            unsigned int byteValue = std::stoul(hexDigits, nullptr, 16);
                            value += static_cast<char>(byteValue);
                        }
                        break;
                    }
                    default: 
                        // Unknown escape sequence, include the backslash and character
                        value += '\\';
                        value += escaped; 
                        break;
                }
            }
        } else {
            value += advance();
        }
    }

    if (isAtEnd()) {
        ErrorHandler::reportLexicalError("Unterminated string - missing closing quote", 
                                        line, current - start);
        exit(1);
    }

    advance(); // Skip the closing quote

    if (hasInterpolation) {
        // Add the final string part as a chunk
        if (!value.empty()) {
            std::vector<Token> stringChunk;
            stringChunk.push_back(Token(TokenType::STRING, value, line));
            chunks.push_back(stringChunk);
        }
        
        // Now create concatenation: chunk1 + chunk2 + chunk3 + ...
        if (!chunks.empty()) {
            // Add first chunk
            for (const auto& token : chunks[0]) {
                tokens.push_back(token);
            }
            
            // Add remaining chunks with PLUS between them
            for (size_t i = 1; i < chunks.size(); i++) {
                tokens.push_back(Token(TokenType::PLUS, "+", line));
                for (const auto& token : chunks[i]) {
                    tokens.push_back(token);
                }
            }
        } else {
            // Empty interpolation, just add empty string
            addToken(TokenType::STRING, "");
        }
    } else {
        // No interpolation, just a regular string
        addToken(TokenType::STRING, value);
    }
}

void Scanner::rawString(char quote) {
    std::string value;
    
    // In raw strings, no escape sequences are processed
    while (peek() != quote && !isAtEnd()) {
        if (peek() == '\n') line++;
        value += advance();
    }

    if (isAtEnd()) {
        ErrorHandler::reportLexicalError("Unterminated raw string - missing closing quote", 
                                        line, current - start);
        exit(1);
    }

    advance(); // Skip the closing quote
    addToken(TokenType::STRING, value);
}

void Scanner::number() {
    while (isDigit(peek())) advance();

    if (peek() == '.' && isDigit(peekNext())) {
        advance();
        while (isDigit(peek())) advance();
    }

    addToken(TokenType::NUMBER, source.substr(start, current - start));
}

void Scanner::identifier() {
    while (isAlphaNumeric(peek())) advance();

    std::string text = source.substr(start, current - start);
    
    // Check for raw string prefix
    if (text == "r" && (peek() == '"' || peek() == '\'')) {
        char quote = advance(); // consume the quote
        rawString(quote);
        return;
    }
    
    TokenType type = keywords.count(text) ? keywords[text] : TokenType::IDENTIFIER;
    addToken(type);
}

bool Scanner::isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool Scanner::isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
            c == '_';
}

bool Scanner::isAlphaNumeric(char c) {
    return isAlpha(c) || isDigit(c);
}

} // namespace neutron
