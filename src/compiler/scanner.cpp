#include "compiler/scanner.h"
#include "token.h"
#include <stdexcept>

namespace neutron {

Scanner::Scanner(const std::string& source)
    : source(source), start(0), current(0), line(1) {
    keywords["and"] = TokenType::AND;
    keywords["class"] = TokenType::CLASS;
    keywords["else"] = TokenType::ELSE;
    keywords["false"] = TokenType::FALSE;
    keywords["for"] = TokenType::FOR;
    keywords["fun"] = TokenType::FUN;
    keywords["if"] = TokenType::IF;
    keywords["nil"] = TokenType::NIL;
    keywords["or"] = TokenType::OR;
    keywords["say"] = TokenType::SAY;
    keywords["return"] = TokenType::RETURN;
    keywords["super"] = TokenType::SUPER;
    keywords["this"] = TokenType::THIS;
    keywords["true"] = TokenType::TRUE;
    keywords["var"] = TokenType::VAR;
    keywords["while"] = TokenType::WHILE;
    keywords["break"] = TokenType::BREAK;
    keywords["continue"] = TokenType::CONTINUE;
    keywords["match"] = TokenType::MATCH;
    keywords["case"] = TokenType::CASE;
    keywords["default"] = TokenType::DEFAULT;
    keywords["try"] = TokenType::TRY;
    keywords["catch"] = TokenType::CATCH;
    keywords["finally"] = TokenType::FINALLY;
    keywords["throw"] = TokenType::THROW;
    keywords["use"] = TokenType::USE;
    keywords["using"] = TokenType::USING;
    
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
        case '-': addToken(TokenType::MINUS); break;
        case '+': addToken(TokenType::PLUS); break;
        case ';': addToken(TokenType::SEMICOLON); break;
        case ':': addToken(TokenType::COLON); break;
        case '*': addToken(TokenType::STAR); break;
        case '%': addToken(TokenType::PERCENT); break;
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
        case '<': addToken(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS); break;
        case '>': addToken(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER); break;
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
                throw std::runtime_error("Unexpected character.");
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
        
        // Check for string interpolation ${...}
        if (peek() == '$' && peekNext() == '{') {
            hasInterpolation = true;
            
            // Add the string part before interpolation as a chunk
            if (!value.empty()) {
                std::vector<Token> stringChunk;
                stringChunk.push_back(Token(TokenType::STRING, value, line));
                chunks.push_back(stringChunk);
                value.clear();
            }
            
            // Skip ${
            advance();
            advance();
            
            // Scan the expression inside ${}
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
                throw std::runtime_error("Unterminated string interpolation.");
            }
            
            // Extract and scan the expression
            std::string expr = source.substr(exprStart, current - exprStart);
            
            // Create a sub-scanner for the expression
            Scanner exprScanner(expr);
            std::vector<Token> exprTokens = exprScanner.scanTokens();
            
            // Remove the EOF token from expression
            if (!exprTokens.empty() && exprTokens.back().type == TokenType::END_OF_FILE) {
                exprTokens.pop_back();
            }
            
            // Add expression tokens wrapped in parentheses as a chunk
            std::vector<Token> exprChunk;
            exprChunk.push_back(Token(TokenType::LEFT_PAREN, "(", line));
            for (const auto& token : exprTokens) {
                exprChunk.push_back(token);
            }
            exprChunk.push_back(Token(TokenType::RIGHT_PAREN, ")", line));
            chunks.push_back(exprChunk);
            
            // Skip the closing }
            advance();
            
        } else if (peek() == '\\') {
            // Handle escape sequences
            advance(); // Skip the backslash
            if (!isAtEnd()) {
                char escaped = advance();
                switch (escaped) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '"': value += '"'; break;
                    case '\'': value += '\''; break;
                    default: value += escaped; break;
                }
            }
        } else {
            value += advance();
        }
    }

    if (isAtEnd()) {
        throw std::runtime_error("Unterminated string.");
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
