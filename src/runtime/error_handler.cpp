#include "runtime/error_handler.h"
#include <sstream>
#include <algorithm>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

namespace neutron {

// Initialize static members
bool ErrorHandler::useColor = true;
bool ErrorHandler::showStackTrace = true;
std::string ErrorHandler::currentFileName = "";
std::vector<std::string> ErrorHandler::sourceLines;

// ANSI color codes
const std::string ErrorHandler::RESET = "\033[0m";
const std::string ErrorHandler::RED = "\033[31m";
const std::string ErrorHandler::YELLOW = "\033[33m";
const std::string ErrorHandler::BLUE = "\033[34m";
const std::string ErrorHandler::GREEN = "\033[32m";
const std::string ErrorHandler::CYAN = "\033[36m";
const std::string ErrorHandler::BOLD = "\033[1m";
const std::string ErrorHandler::DIM = "\033[2m";

// Check if terminal supports colors
static bool isTerminalColorSupported() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        // Enable ANSI escape sequences on Windows 10+
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
        return true;
    }
    return false;
#else
    return ISATTY(FILENO(stderr));
#endif
}

void ErrorHandler::setColorEnabled(bool enabled) {
    useColor = enabled && isTerminalColorSupported();
}

void ErrorHandler::setStackTraceEnabled(bool enabled) {
    showStackTrace = enabled;
}

void ErrorHandler::setCurrentFile(const std::string& fileName) {
    currentFileName = fileName;
}

void ErrorHandler::setSourceLines(const std::vector<std::string>& lines) {
    sourceLines = lines;
}

std::string ErrorHandler::getErrorTypeName(ErrorType type) {
    switch (type) {
        case ErrorType::SYNTAX_ERROR:     return "SyntaxError";
        case ErrorType::RUNTIME_ERROR:    return "RuntimeError";
        case ErrorType::TYPE_ERROR:       return "TypeError";
        case ErrorType::REFERENCE_ERROR:  return "ReferenceError";
        case ErrorType::RANGE_ERROR:      return "RangeError";
        case ErrorType::ARGUMENT_ERROR:   return "ArgumentError";
        case ErrorType::DIVISION_ERROR:   return "DivisionError";
        case ErrorType::STACK_ERROR:      return "StackError";
        case ErrorType::MODULE_ERROR:     return "ModuleError";
        case ErrorType::IO_ERROR:         return "IOError";
        case ErrorType::LEXICAL_ERROR:    return "LexicalError";
        default:                          return "Error";
    }
}

std::string ErrorHandler::getColoredErrorType(ErrorType type) {
    std::string typeName = getErrorTypeName(type);
    if (!useColor) return typeName;
    
    std::string color;
    switch (type) {
        case ErrorType::SYNTAX_ERROR:
        case ErrorType::LEXICAL_ERROR:
            color = YELLOW;
            break;
        case ErrorType::RUNTIME_ERROR:
        case ErrorType::DIVISION_ERROR:
        case ErrorType::STACK_ERROR:
            color = RED;
            break;
        case ErrorType::TYPE_ERROR:
        case ErrorType::REFERENCE_ERROR:
        case ErrorType::ARGUMENT_ERROR:
            color = RED + BOLD;
            break;
        default:
            color = RED;
            break;
    }
    
    return color + BOLD + typeName + RESET;
}

std::string ErrorHandler::highlightErrorPosition(const std::string& line, int column, int length) {
    if (column < 0 || line.empty()) return "";
    
    std::ostringstream oss;
    
    // Add spacing to align with the error position
    for (int i = 0; i < column && i < static_cast<int>(line.length()); ++i) {
        if (line[i] == '\t') {
            oss << '\t';
        } else {
            oss << ' ';
        }
    }
    
    // Add error indicator
    if (useColor) {
        oss << RED << BOLD;
    }
    
    if (length > 1) {
        for (int i = 0; i < length; ++i) {
            oss << '^';
        }
    } else {
        oss << '^';
    }
    
    if (useColor) {
        oss << RESET;
    }
    
    return oss.str();
}

std::string ErrorHandler::formatStackTrace(const std::vector<StackFrame>& trace) {
    if (trace.empty() || !showStackTrace) return "";
    
    std::ostringstream oss;
    oss << "\nStack trace:\n";
    
    for (size_t i = 0; i < trace.size(); ++i) {
        const auto& frame = trace[i];
        
        if (useColor) {
            oss << DIM << "  at " << RESET;
        } else {
            oss << "  at ";
        }
        
        if (useColor) {
            oss << CYAN << frame.functionName << RESET;
        } else {
            oss << frame.functionName;
        }
        
        oss << " (";
        
        if (!frame.fileName.empty()) {
            if (useColor) {
                oss << BLUE << frame.fileName << RESET;
            } else {
                oss << frame.fileName;
            }
        }
        
        if (frame.line > 0) {
            oss << ":" << frame.line;
            if (frame.column > 0) {
                oss << ":" << frame.column;
            }
        }
        
        oss << ")\n";
    }
    
    return oss.str();
}

std::string ErrorHandler::getSuggestion(ErrorType type, const std::string& message) {
    std::string lower = message;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Provide helpful suggestions based on error type and message content
    if (type == ErrorType::REFERENCE_ERROR) {
        if (lower.find("undefined variable") != std::string::npos) {
            return "Did you forget to declare the variable with 'var'? Or maybe you need to import a module with 'use'?";
        }
    } else if (type == ErrorType::SYNTAX_ERROR) {
        if (lower.find("expect ';'") != std::string::npos) {
            return "Every statement in Neutron must end with a semicolon.";
        } else if (lower.find("expect ')'") != std::string::npos || 
                   lower.find("expect '('") != std::string::npos) {
            return "Check that all parentheses are properly matched.";
        } else if (lower.find("expect '}'") != std::string::npos || 
                   lower.find("expect '{'") != std::string::npos) {
            return "Check that all braces are properly matched.";
        }
    } else if (type == ErrorType::TYPE_ERROR) {
        if (lower.find("operands must be numbers") != std::string::npos) {
            return "Mathematical operations require numeric values. Use type conversion if needed.";
        }
    } else if (type == ErrorType::ARGUMENT_ERROR) {
        return "Check the function definition to see how many arguments it expects.";
    } else if (type == ErrorType::MODULE_ERROR) {
        if (lower.find("could not find") != std::string::npos || 
            lower.find("not found") != std::string::npos) {
            return "Make sure the module file exists in the lib/ or box/ directory, or use 'use modulename;' to import it.";
        }
    } else if (type == ErrorType::RANGE_ERROR) {
        if (lower.find("index") != std::string::npos) {
            return "Array indices must be within bounds (0 to length-1).";
        }
    }
    
    return "";
}

void ErrorHandler::reportError(const ErrorInfo& error) {
    std::ostringstream oss;
    
    // Error type and location
    oss << "\n";
    oss << getColoredErrorType(error.type);
    
    if (!error.fileName.empty()) {
        if (useColor) {
            oss << " in " << BLUE << error.fileName << RESET;
        } else {
            oss << " in " << error.fileName;
        }
    }
    
    if (error.line > 0) {
        oss << " at line " << error.line;
        if (error.column > 0) {
            oss << ", column " << error.column;
        }
    }
    
    oss << ":\n";
    
    // Error message
    if (useColor) {
        oss << "  " << BOLD << error.message << RESET << "\n";
    } else {
        oss << "  " << error.message << "\n";
    }
    
    // Show the source line if available
    if (!error.sourceLine.empty()) {
        oss << "\n";
        
        // Line number
        if (error.line > 0) {
            if (useColor) {
                oss << DIM << std::setw(4) << error.line << " | " << RESET;
            } else {
                oss << std::setw(4) << error.line << " | ";
            }
        }
        
        oss << error.sourceLine << "\n";
        
        // Error position indicator
        if (error.column >= 0) {
            if (error.line > 0) {
                oss << "     | ";
            }
            oss << highlightErrorPosition(error.sourceLine, error.column) << "\n";
        }
    }
    
    // Suggestion
    if (!error.suggestion.empty()) {
        if (useColor) {
            oss << "\n" << GREEN << "Suggestion: " << RESET << error.suggestion << "\n";
        } else {
            oss << "\nSuggestion: " << error.suggestion << "\n";
        }
    }
    
    // Stack trace
    if (!error.stackTrace.empty() && showStackTrace) {
        oss << formatStackTrace(error.stackTrace);
    }
    
    std::cerr << oss.str() << std::endl;
}

void ErrorHandler::reportSyntaxError(const std::string& message, const Token& token, 
                                     const std::string& suggestion) {
    ErrorInfo error(ErrorType::SYNTAX_ERROR, message, currentFileName, token.line, 0);
    
    // Try to get the source line
    if (token.line > 0 && token.line <= static_cast<int>(sourceLines.size())) {
        error.sourceLine = sourceLines[token.line - 1];
        // Try to find column position
        if (!token.lexeme.empty()) {
            size_t pos = error.sourceLine.find(token.lexeme);
            if (pos != std::string::npos) {
                error.column = static_cast<int>(pos);
            }
        }
    }
    
    error.suggestion = suggestion.empty() ? getSuggestion(ErrorType::SYNTAX_ERROR, message) : suggestion;
    
    reportError(error);
}

void ErrorHandler::reportRuntimeError(const std::string& message, const std::string& fileName,
                                      int line, const std::vector<StackFrame>& trace) {
    ErrorInfo error(ErrorType::RUNTIME_ERROR, message, fileName.empty() ? currentFileName : fileName, line);
    error.stackTrace = trace;
    
    // Try to get the source line
    if (line > 0 && line <= static_cast<int>(sourceLines.size())) {
        error.sourceLine = sourceLines[line - 1];
    }
    
    error.suggestion = getSuggestion(ErrorType::RUNTIME_ERROR, message);
    
    reportError(error);
}

void ErrorHandler::reportLexicalError(const std::string& message, int line, int column,
                                      const std::string& fileName) {
    ErrorInfo error(ErrorType::LEXICAL_ERROR, message, fileName.empty() ? currentFileName : fileName, 
                   line, column);
    
    // Try to get the source line
    if (line > 0 && line <= static_cast<int>(sourceLines.size())) {
        error.sourceLine = sourceLines[line - 1];
    }
    
    error.suggestion = getSuggestion(ErrorType::LEXICAL_ERROR, message);
    
    reportError(error);
}

void ErrorHandler::typeError(const std::string& expected, const std::string& got, 
                             const std::string& fileName, int line) {
    std::string message = "Expected " + expected + " but got " + got;
    ErrorInfo error(ErrorType::TYPE_ERROR, message, fileName.empty() ? currentFileName : fileName, line);
    error.suggestion = getSuggestion(ErrorType::TYPE_ERROR, message);
    reportError(error);
}

void ErrorHandler::referenceError(const std::string& name, const std::string& fileName, 
                                  int line, bool isFunction) {
    std::string message = "Undefined " + std::string(isFunction ? "function" : "variable") + " '" + name + "'";
    ErrorInfo error(ErrorType::REFERENCE_ERROR, message, fileName.empty() ? currentFileName : fileName, line);
    error.suggestion = getSuggestion(ErrorType::REFERENCE_ERROR, message);
    reportError(error);
}

void ErrorHandler::rangeError(const std::string& message, const std::string& fileName, int line) {
    ErrorInfo error(ErrorType::RANGE_ERROR, message, fileName.empty() ? currentFileName : fileName, line);
    error.suggestion = getSuggestion(ErrorType::RANGE_ERROR, message);
    reportError(error);
}

void ErrorHandler::argumentError(int expected, int got, const std::string& functionName,
                                const std::string& fileName, int line) {
    std::string message = "Function '" + functionName + "' expects " + std::to_string(expected) + 
                         " argument" + (expected != 1 ? "s" : "") + " but got " + std::to_string(got);
    ErrorInfo error(ErrorType::ARGUMENT_ERROR, message, fileName.empty() ? currentFileName : fileName, line);
    error.suggestion = getSuggestion(ErrorType::ARGUMENT_ERROR, message);
    reportError(error);
}

void ErrorHandler::divisionError(const std::string& fileName, int line) {
    std::string message = "Division by zero is not allowed";
    ErrorInfo error(ErrorType::DIVISION_ERROR, message, fileName.empty() ? currentFileName : fileName, line);
    error.suggestion = "Check that the divisor is not zero before performing division or modulo operations.";
    reportError(error);
}

void ErrorHandler::stackOverflowError(const std::string& fileName, int line) {
    std::string message = "Stack overflow - maximum call depth exceeded";
    ErrorInfo error(ErrorType::STACK_ERROR, message, fileName.empty() ? currentFileName : fileName, line);
    error.suggestion = "This usually indicates infinite recursion. Check your recursive function calls.";
    reportError(error);
}

void ErrorHandler::moduleError(const std::string& moduleName, const std::string& reason,
                               const std::string& fileName, int line) {
    std::string message = "Failed to load module '" + moduleName + "': " + reason;
    ErrorInfo error(ErrorType::MODULE_ERROR, message, fileName.empty() ? currentFileName : fileName, line);
    error.suggestion = getSuggestion(ErrorType::MODULE_ERROR, message);
    reportError(error);
}

std::string ErrorHandler::formatErrorMessage(const std::string& message) {
    if (useColor) {
        return RED + message + RESET;
    }
    return message;
}

void ErrorHandler::printLine(const std::string& content, bool error) {
    if (error) {
        std::cerr << content << std::endl;
    } else {
        std::cout << content << std::endl;
    }
}

[[noreturn]] void ErrorHandler::fatal(const ErrorInfo& error) {
    reportError(error);
    exit(1);
}

[[noreturn]] void ErrorHandler::fatal(const std::string& message, ErrorType type) {
    ErrorInfo error(type, message, currentFileName);
    fatal(error);
}

// NeutronException implementation
NeutronException::NeutronException(const ErrorInfo& err) : error(err) {
    std::ostringstream oss;
    oss << ErrorHandler::getErrorTypeName(err.type) << ": " << err.message;
    if (!err.fileName.empty()) {
        oss << " in " << err.fileName;
    }
    if (err.line > 0) {
        oss << " at line " << err.line;
    }
    formattedMessage = oss.str();
}

NeutronException::NeutronException(ErrorType type, const std::string& message, 
                                 const std::string& fileName, int line) 
    : error(type, message, fileName, line) {
    std::ostringstream oss;
    oss << ErrorHandler::getErrorTypeName(type) << ": " << message;
    if (!fileName.empty()) {
        oss << " in " << fileName;
    }
    if (line > 0) {
        oss << " at line " << line;
    }
    formattedMessage = oss.str();
}

const char* NeutronException::what() const noexcept {
    return formattedMessage.c_str();
}

} // namespace neutron
