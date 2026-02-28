/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * Neutron Code Formatter Implementation
 */

#include "formatter.h"
#include <fstream>
#include <iostream>
#include <regex>
#include <algorithm>

namespace neutron {

std::string Formatter::getIndent(int level, const Options& options) {
    if (level <= 0) return "";
    if (options.useSpaces) {
        return std::string(level * options.indentSize, ' ');
    } else {
        return std::string(level, '\t');
    }
}

std::string Formatter::trimRight(const std::string& str) {
    size_t end = str.find_last_not_of(" \t\r\n");
    return (end == std::string::npos) ? "" : str.substr(0, end + 1);
}

bool Formatter::isBlockOpener(const std::string& line) {
    // Check if line ends with { (ignoring whitespace and comments)
    std::string trimmed = trimRight(line);
    
    // Remove trailing comment
    size_t commentPos = trimmed.find("//");
    if (commentPos != std::string::npos) {
        trimmed = trimRight(trimmed.substr(0, commentPos));
    }
    
    return !trimmed.empty() && trimmed.back() == '{';
}

bool Formatter::isBlockCloser(const std::string& line) {
    std::string trimmed = line;
    // Remove leading whitespace
    size_t start = trimmed.find_first_not_of(" \t");
    if (start == std::string::npos) return false;
    trimmed = trimmed.substr(start);
    
    return !trimmed.empty() && trimmed[0] == '}';
}

bool Formatter::isControlFlow(const std::string& line) {
    static std::regex controlRegex(R"(^\s*(if|else|while|for|do|match|case|try|catch|finally)\b)");
    return std::regex_search(line, controlRegex);
}

int Formatter::countBraces(const std::string& line, char open, char close) {
    int count = 0;
    bool inString = false;
    bool inChar = false;
    bool escaped = false;
    
    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];
        
        if (escaped) {
            escaped = false;
            continue;
        }
        
        if (c == '\\') {
            escaped = true;
            continue;
        }
        
        if (c == '"' && !inChar) {
            inString = !inString;
            continue;
        }
        
        if (c == '\'' && !inString) {
            inChar = !inChar;
            continue;
        }
        
        if (inString || inChar) continue;
        
        // Check for line comment
        if (c == '/' && i + 1 < line.length() && line[i + 1] == '/') {
            break; // Rest of line is comment
        }
        
        if (c == open) count++;
        if (c == close) count--;
    }
    
    return count;
}

std::string Formatter::format(const std::string& source, const Options& options) {
    std::vector<std::string> lines;
    std::istringstream iss(source);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Remove \r if present (Windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    
    std::ostringstream result;
    int indentLevel = 0;
    bool lastWasBlank = false;
    bool inMultilineComment = false;
    
    for (size_t i = 0; i < lines.size(); i++) {
        std::string& currentLine = lines[i];
        
        // Get content without leading whitespace
        size_t contentStart = currentLine.find_first_not_of(" \t");
        std::string content = (contentStart == std::string::npos) ? "" : currentLine.substr(contentStart);
        
        // Trim trailing whitespace
        if (options.trimTrailingWhitespace) {
            content = trimRight(content);
        }
        
        // Handle blank lines - collapse multiple blanks into one
        if (content.empty()) {
            if (!lastWasBlank && i > 0) {
                result << "\n";
            }
            lastWasBlank = true;
            continue;
        }
        lastWasBlank = false;
        
        // Check for multiline comments
        if (content.find("/*") != std::string::npos) {
            inMultilineComment = true;
        }
        
        // Calculate indent adjustment for this line
        int thisLineIndent = indentLevel;
        
        // Check if line starts with closing brace - indent one less
        if (content[0] == '}' || content[0] == ']' || content[0] == ')') {
            thisLineIndent = std::max(0, thisLineIndent - 1);
        }
        
        // Handle else, catch, finally - same indent as if/try
        if (content.find("else") == 0 || content.find("catch") == 0 || 
            content.find("finally") == 0 || content.find("} else") == 0 ||
            content.find("} catch") == 0 || content.find("} finally") == 0) {
            // These should be at the same level as the closing brace
            if (content[0] != '}') {
                thisLineIndent = std::max(0, thisLineIndent - 1);
            }
        }
        
        // Handle case/default in match
        if ((content.find("case ") == 0 || content.find("default:") == 0 || content.find("default {") == 0) && indentLevel > 0) {
            thisLineIndent = std::max(0, thisLineIndent - 1);
        }
        
        // Write the formatted line
        result << getIndent(thisLineIndent, options) << content << "\n";
        
        // Calculate indent change for next line
        int braceBalance = countBraces(content, '{', '}');
        int bracketBalance = countBraces(content, '[', ']');
        int parenBalance = countBraces(content, '(', ')');
        
        indentLevel += braceBalance + bracketBalance;
        
        // Handle multi-line function calls/arrays
        if (parenBalance > 0 || bracketBalance > 0) {
            // Opening paren/bracket increases indent
        }
        
        // Ensure indent never goes negative
        indentLevel = std::max(0, indentLevel);
        
        // Check for end of multiline comment
        if (content.find("*/") != std::string::npos) {
            inMultilineComment = false;
        }
    }
    
    std::string formatted = result.str();
    
    // Remove trailing blank lines but keep one final newline
    while (formatted.length() > 1 && 
           formatted[formatted.length() - 1] == '\n' && 
           formatted[formatted.length() - 2] == '\n') {
        formatted.pop_back();
    }
    
    // Ensure final newline if option is set
    if (options.insertFinalNewline && !formatted.empty() && formatted.back() != '\n') {
        formatted += '\n';
    }
    
    return formatted;
}

bool Formatter::formatFile(const std::string& path, const Options& options) {
    // Read file
    std::ifstream inFile(path);
    if (!inFile.is_open()) {
        std::cerr << "Error: Could not open file: " << path << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string source = buffer.str();
    inFile.close();
    
    // Format
    std::string formatted = format(source, options);
    
    // Write back
    std::ofstream outFile(path);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not write to file: " << path << std::endl;
        return false;
    }
    
    outFile << formatted;
    outFile.close();
    
    return true;
}

bool Formatter::checkFormat(const std::string& path, const Options& options) {
    // Read file
    std::ifstream inFile(path);
    if (!inFile.is_open()) {
        std::cerr << "Error: Could not open file: " << path << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string source = buffer.str();
    inFile.close();
    
    // Format and compare
    std::string formatted = format(source, options);
    
    return source == formatted;
}

} // namespace neutron
