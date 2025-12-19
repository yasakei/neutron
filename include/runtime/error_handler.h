#ifndef NEUTRON_ERROR_HANDLER_H
#define NEUTRON_ERROR_HANDLER_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "token.h"

namespace neutron {

// Error types for categorization
enum class ErrorType {
    SYNTAX_ERROR,        // Parsing errors
    RUNTIME_ERROR,       // Execution errors
    TYPE_ERROR,          // Type mismatch errors
    REFERENCE_ERROR,     // Undefined variable/function errors
    RANGE_ERROR,         // Index out of bounds, etc.
    ARGUMENT_ERROR,      // Wrong number of arguments
    DIVISION_ERROR,      // Division/modulo by zero
    STACK_ERROR,         // Stack overflow/underflow
    MODULE_ERROR,        // Module loading errors
    IO_ERROR,            // File operations
    LEXICAL_ERROR        // Scanner/tokenization errors
};

// Stack frame information for error reporting
struct StackFrame {
    std::string functionName;
    std::string fileName;
    int line;
    int column;

    StackFrame(const std::string& func, const std::string& file, int ln, int col = 0)
        : functionName(func), fileName(file), line(ln), column(col) {}
};

// Comprehensive error information
struct ErrorInfo {
    ErrorType type;
    std::string message;
    std::string fileName;
    int line;
    int column;
    std::string sourceLine;     // The actual line of code where error occurred
    std::string suggestion;     // Helpful suggestion for fixing the error
    std::vector<StackFrame> stackTrace;

    ErrorInfo(ErrorType t, const std::string& msg, const std::string& file = "", 
              int ln = -1, int col = -1)
        : type(t), message(msg), fileName(file), line(ln), column(col) {}
};

// Main error handler class
class ErrorHandler {
public:
    // Helper methods (public for NeutronException)
    static std::string getErrorTypeName(ErrorType type);

private:
    static bool useColor;
    static bool showStackTrace;
    static std::string currentFileName;
    static std::vector<std::string> sourceLines;  // Cache of source code lines

    // ANSI color codes (cross-platform compatible)
    static const std::string RESET;
    static const std::string RED;
    static const std::string YELLOW;
    static const std::string BLUE;
    static const std::string GREEN;
    static const std::string CYAN;
    static const std::string BOLD;
    static const std::string DIM;

    // Helper methods
    static std::string getColoredErrorType(ErrorType type);
    static std::string formatStackTrace(const std::vector<StackFrame>& trace);
    static std::string highlightErrorPosition(const std::string& line, int column, int length = 1);
    static std::string getSuggestion(ErrorType type, const std::string& message);

public:
    // Configuration
    static void setColorEnabled(bool enabled);
    static void setStackTraceEnabled(bool enabled);
    static void setCurrentFile(const std::string& fileName);
    static void setSourceLines(const std::vector<std::string>& lines);

    // Error reporting methods
    static void reportError(const ErrorInfo& error);
    static void reportSyntaxError(const std::string& message, const Token& token, 
                                    const std::string& suggestion = "");
    static void reportRuntimeError(const std::string& message, const std::string& fileName = "",
                                     int line = -1, const std::vector<StackFrame>& trace = {});
    static void reportLexicalError(const std::string& message, int line, int column,
                                     const std::string& fileName = "");

    // Specific error types with automatic suggestions
    static void typeError(const std::string& expected, const std::string& got, 
                          const std::string& fileName = "", int line = -1);
    static void referenceError(const std::string& name, const std::string& fileName = "", 
                               int line = -1, bool isFunction = false);
    static void rangeError(const std::string& message, const std::string& fileName = "", 
                          int line = -1);
    static void argumentError(int expected, int got, const std::string& functionName,
                             const std::string& fileName = "", int line = -1);
    static void divisionError(const std::string& fileName = "", int line = -1);
    static void stackOverflowError(const std::string& fileName = "", int line = -1);
    static void moduleError(const std::string& moduleName, const std::string& reason,
                           const std::string& fileName = "", int line = -1);

    // Format helpers
    static std::string formatErrorMessage(const std::string& message);
    static void printLine(const std::string& content, bool error = false);
    
    // Exit with error code
    [[noreturn]] static void fatal(const ErrorInfo& error);
    [[noreturn]] static void fatal(const std::string& message, ErrorType type = ErrorType::RUNTIME_ERROR);
    
    // Cleanup static data (called before library unload on Linux)
    static void cleanup();
};

// Exception class for recoverable errors (e.g., in REPL mode)
class NeutronException : public std::exception {
private:
    ErrorInfo error;
    std::string formattedMessage;

public:
    explicit NeutronException(const ErrorInfo& err);
    explicit NeutronException(ErrorType type, const std::string& message, 
                             const std::string& fileName = "", int line = -1);
    
    const char* what() const noexcept override;
    const ErrorInfo& getError() const { return error; }
};

} // namespace neutron

#endif // NEUTRON_ERROR_HANDLER_H
