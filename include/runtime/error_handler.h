#ifndef NEUTRON_ERROR_HANDLER_H
#define NEUTRON_ERROR_HANDLER_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <unordered_map>
#include "token.h"

/**
 * Return the human-readable name for the given ErrorType.
 * @param type The error type to translate.
 * @returns The name of the error type (e.g., "SYNTAX_ERROR").
 */
/**
 * Enable or disable colored output in error messages.
 * @param enabled True to enable colored output, false to disable it.
 */
/**
 * Enable or disable printing of stack traces with errors.
 * @param enabled True to print stack traces, false to omit them.
 */
/**
 * Set the current source file name used for contextual error reporting.
 * @param fileName The path or name of the current file.
 */
/**
 * Provide source lines for the current file to enable inline context display.
 * @param lines Vector of source lines for the current file.
 */
/**
 * Cache the source contents for a named file for later context lookup.
 * @param fileName The file name to associate with the provided source.
 * @param source The full source text for the file (will be split into lines internally).
 */
/**
 * Print a brief summary of all errors reported so far, including totals and any suppressed details.
 */
/**
 * Reset internal error-handler state, including caches and counters, to initial defaults.
 */
/**
 * Report a fully populated ErrorInfo to the handler for formatted output and tracking.
 * @param error The detailed error information to report.
 */
/**
 * Report a syntax error associated with a token and optional suggestion.
 * @param message Human-readable error message describing the syntax problem.
 * @param token The token where the syntax error was detected.
 * @param suggestion Optional suggestion text to help fix the error.
 */
/**
 * Report a runtime error with optional file, line, and stack trace context.
 * @param message Human-readable error message describing the runtime failure.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred, or -1 if unknown.
 * @param trace Optional stack trace frames to include with the error.
 */
/**
 * Report a lexical/tokenization error at a specific location.
 * @param message Human-readable error message describing the lexical issue.
 * @param line Line number where the lexical error occurred.
 * @param column Column index where the lexical error occurred.
 * @param fileName Optional file name where the error occurred.
 */
/**
 * Report a type error describing an expected and actual type mismatch.
 * @param expected Description of the expected type.
 * @param got Description of the actual type encountered.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred, or -1 if unknown.
 */
/**
 * Report an undefined reference to a variable or function.
 * @param name The identifier that was referenced but not found.
 * @param fileName Optional file name where the reference occurred.
 * @param line Optional line number where the reference occurred, or -1 if unknown.
 * @param isFunction True if the missing reference was expected to be a function.
 */
/**
 * Report a range-related error such as index out of bounds.
 * @param message Human-readable message describing the range error.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred, or -1 if unknown.
 */
/**
 * Report an argument count mismatch for a function call.
 * @param expected Number of arguments expected.
 * @param got Number of arguments actually provided.
 * @param functionName Name of the function that received the wrong argument count.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred, or -1 if unknown.
 */
/**
 * Report a division or modulo by zero error.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred, or -1 if unknown.
 */
/**
 * Report a stack overflow or related stack error.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred, or -1 if unknown.
 */
/**
 * Report a module loading or resolution error with a reason.
 * @param moduleName Name of the module that failed to load or resolve.
 * @param reason Human-readable reason for the module error.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred, or -1 if unknown.
 */
/**
 * Format an error message for consistent presentation (may add prefixes or normalization).
 * @param message The raw error message to format.
 * @returns The formatted error message string.
 */
/**
 * Print a single line to the error output, optionally marking it as an error line.
 * @param content The line content to print.
 * @param error If true, the line will be printed with error highlighting.
 */
/**
 * Terminate the process with the provided detailed error information.
 * This function does not return.
 * @param error The ErrorInfo describing the fatal condition.
 */
/**
 * Terminate the process with a simple message and error type.
 * This function does not return.
 * @param message Human-readable message describing the fatal condition.
 * @param type The ErrorType to classify the fatal error.
 */
/**
 * Release or reset all internal static resources and caches used by the error handler.
 */
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
    static std::string* currentFileName;
    static std::vector<std::string>* sourceLines;  // Cache of source code lines (pointer to avoid destruction order issues)
    static std::unordered_map<std::string, std::vector<std::string>>* fileSources; // Cache of source code lines for all files
    static bool cleanedUp;  // Flag to prevent double cleanup
    static bool hasError;   // Flag to track if any error occurred
    static int errorCount;  // Total number of errors reported
    static const int MAX_DETAILED_ERRORS = 4; // Maximum number of errors to show with full details

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
    static void addFileSource(const std::string& fileName, const std::string& source);
    static void printSummary();
    static void reset();

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
    
    // Check if cleanup has been called
    static bool isCleanedUp() { return cleanedUp; }
    
    // Check if any error occurred
    static bool hadError() { return hasError; }
    static void resetError() { hasError = false; }
    
    // Get current filename safely
    static std::string getCurrentFileName() { return currentFileName ? *currentFileName : ""; }
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