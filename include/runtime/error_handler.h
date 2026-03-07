#ifndef NEUTRON_ERROR_HANDLER_H
#define NEUTRON_ERROR_HANDLER_H

/*
 * Code Documentation: Error Handling System (error_handler.h)
 * ===========================================================
 * 
 * This header defines the ErrorHandler class - Neutron's comprehensive
 * error reporting and diagnostics system.
 * 
 * What This File Includes:
 * ------------------------
 * - ErrorType enum: Categorized error types
 * - ErrorInfo struct: Complete error information container
 * - StackFrame struct: Call stack information
 * - ErrorHandler class: Main error reporting interface
 * - NeutronException: Recoverable exception for REPL
 * 
 * How It Works:
 * -------------
 * The error handler provides:
 * 1. Categorized errors (syntax, runtime, type, etc.)
 * 2. Source code highlighting at error location
 * 3. Stack traces for runtime errors
 * 4. Helpful suggestions for fixing errors
 * 5. Colored terminal output (when enabled)
 * 
 * Error Reporting Flow:
 * @code
 * 1. Error detected (e.g., undefined variable)
 * 2. Create ErrorInfo with type, message, location
 * 3. Add stack trace frames from VM
 * 4. ErrorHandler formats and displays error
 * 5. Optionally throw NeutronException for recovery
 * @endcode
 * 
 * Error Categories:
 * - SYNTAX_ERROR: Parsing failures
 * - RUNTIME_ERROR: Execution failures
 * - TYPE_ERROR: Type mismatches
 * - REFERENCE_ERROR: Undefined identifiers
 * - RANGE_ERROR: Index out of bounds
 * - ARGUMENT_ERROR: Wrong argument count
 * - DIVISION_ERROR: Division by zero
 * - MODULE_ERROR: Module loading failures
 * - IO_ERROR: File operation failures
 * - LEXICAL_ERROR: Tokenization failures
 * 
 * Adding Features:
 * ----------------
 * - New error types: Add to ErrorType enum, update getErrorTypeName()
 * - Custom suggestions: Extend getSuggestion() for new error patterns
 * - Error recovery: Add recovery strategies for specific error types
 * - External logging: Use setErrorCallback() for custom logging
 * 
 * What You Should NOT Do:
 * -----------------------
 * - Do NOT ignore error return values
 * - Do NOT continue execution after fatal errors
 * - Do NOT modify error state during error reporting
 * - Do NOT use ErrorHandler from multiple threads without synchronization
 */

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <unordered_map>
#include <functional>
#include "token.h"

namespace neutron {

// Forward declarations
class ErrorHandler;
class NeutronException;

// Error types for categorization (moved to namespace level for proper visibility)
enum class ErrorType {
    SYNTAX_ERROR,        ///< Parsing errors (unexpected tokens, missing semicolons)
    RUNTIME_ERROR,       ///< Execution errors (undefined variables, type errors)
    TYPE_ERROR,          ///< Type mismatch errors (expected number, got string)
    REFERENCE_ERROR,     ///< Undefined variable/function errors
    RANGE_ERROR,         ///< Index out of bounds, invalid range
    ARGUMENT_ERROR,      ///< Wrong number of arguments to function
    DIVISION_ERROR,      ///< Division or modulo by zero
    STACK_ERROR,         ///< Stack overflow/underflow
    MODULE_ERROR,        ///< Module loading or resolution failures
    IO_ERROR,            ///< File operation failures
    LEXICAL_ERROR        ///< Scanner/tokenization errors (invalid characters)
};

// Stack frame information for error reporting
struct StackFrame {
    std::string functionName;  ///< Function name (or "<script>" for top-level)
    std::string fileName;      ///< Source file name
    int line;                  ///< Source line number
    int column;                ///< Column position (0-based)

    StackFrame(const std::string& func, const std::string& file, int ln, int col = 0)
        : functionName(func), fileName(file), line(ln), column(col) {}
};

// Comprehensive error information
struct ErrorInfo {
    ErrorType type;                    ///< Error category
    std::string message;               ///< Human-readable description
    std::string fileName;              ///< File where error occurred
    int line;                          ///< Line number (-1 if unknown)
    int column;                        ///< Column position (-1 if unknown)
    std::string sourceLine;            ///< The actual line of code (for highlighting)
    std::string suggestion;            ///< Helpful suggestion for fixing
    std::vector<StackFrame> stackTrace;  ///< Call stack (for runtime errors)

    ErrorInfo(ErrorType t, const std::string& msg, const std::string& file = "",
              int ln = -1, int col = -1)
        : type(t), message(msg), fileName(file), line(ln), column(col) {}
};

} // namespace neutron

/**
 * @brief Get the human-readable name for an ErrorType.
 * @param type The error type to translate.
 * @return The name of the error type (e.g., "SYNTAX_ERROR").
 */
std::string getErrorTypeName(neutron::ErrorType type);

/**
 * @brief Enable or disable colored output in error messages.
 * @param enabled True to enable colored output, false to disable it.
 * 
 * Colored output uses ANSI escape codes for terminal highlighting.
 * Automatically disabled when output is not a terminal.
 */
void setColorEnabled(bool enabled);

/**
 * @brief Enable or disable printing of stack traces with errors.
 * @param enabled True to print stack traces, false to omit them.
 * 
 * Stack traces show the call chain leading to runtime errors.
 * Essential for debugging, can be verbose for simple scripts.
 */
void setStackTraceEnabled(bool enabled);

/**
 * @brief Set the current source file name for contextual error reporting.
 * @param fileName The path or name of the current file.
 */
void setCurrentFile(const std::string& fileName);

/**
 * @brief Provide source lines for the current file to enable inline context.
 * @param lines Vector of source lines for the current file.
 * 
 * Source lines are used to display the error location with context:
 * @code
 * error.nt:5:10: RUNTIME ERROR: undefined variable 'x'
 *   4 | var y = 10;
 *   5 | print(x);
 *    |          ^
 *   6 | }
 * @endcode
 */
void setSourceLines(const std::vector<std::string>& lines);

/**
 * @brief Cache the source contents for a named file for later lookup.
 * @param fileName The file name to associate with the source.
 * @param source The full source text for the file.
 */
void addFileSource(const std::string& fileName, const std::string& source);

/**
 * @brief Print a brief summary of all errors reported so far.
 * 
 * Summary includes:
 * - Total error count
 * - Errors by type
 * - Suppressed error count (beyond MAX_DETAILED_ERRORS)
 */
void printSummary();

/**
 * @brief Reset internal error handler state to initial defaults.
 * 
 * Clears caches, counters, and source line storage.
 * Call between REPL iterations or batch compilations.
 */
void reset();

/**
 * @brief Report a fully populated ErrorInfo to the handler.
 * @param error The detailed error information to report.
 */
void reportError(const neutron::ErrorInfo& error);

/**
 * @brief Report a syntax error with token location and suggestion.
 * @param message Human-readable error message.
 * @param token The token where the error was detected.
 * @param suggestion Optional suggestion text to help fix the error.
 */
void reportSyntaxError(const std::string& message, const neutron::Token& token,
                       const std::string& suggestion = "");

/**
 * @brief Report a runtime error with optional file, line, and stack trace.
 * @param message Human-readable error message.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred.
 * @param trace Optional stack trace frames to include.
 */
void reportRuntimeError(const std::string& message, const std::string& fileName = "",
                        int line = -1, const std::vector<neutron::StackFrame>& trace = {});

/**
 * @brief Report a lexical/tokenization error at a specific location.
 * @param message Human-readable error message.
 * @param line Line number where the error occurred.
 * @param column Column index where the error occurred.
 * @param fileName Optional file name where the error occurred.
 */
void reportLexicalError(const std::string& message, int line, int column,
                        const std::string& fileName = "");

/**
 * @brief Report a type error (expected vs actual type mismatch).
 * @param expected Description of the expected type.
 * @param got Description of the actual type encountered.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred.
 */
void typeError(const std::string& expected, const std::string& got,
               const std::string& fileName = "", int line = -1);

/**
 * @brief Report an undefined reference to a variable or function.
 * @param name The identifier that was referenced but not found.
 * @param fileName Optional file name where the reference occurred.
 * @param line Optional line number where the reference occurred.
 * @param isFunction True if the missing reference was expected to be a function.
 */
void referenceError(const std::string& name, const std::string& fileName = "",
                    int line = -1, bool isFunction = false);

/**
 * @brief Report a range-related error such as index out of bounds.
 * @param message Human-readable message describing the range error.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred.
 */
void rangeError(const std::string& message, const std::string& fileName = "",
                int line = -1);

/**
 * @brief Report an argument count mismatch for a function call.
 * @param expected Number of arguments expected.
 * @param got Number of arguments actually provided.
 * @param functionName Name of the function that received wrong arguments.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred.
 */
void argumentError(int expected, int got, const std::string& functionName,
                   const std::string& fileName = "", int line = -1);

/**
 * @brief Report a division or modulo by zero error.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred.
 */
void divisionError(const std::string& fileName = "", int line = -1);

/**
 * @brief Report a stack overflow or related stack error.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred.
 */
void stackOverflowError(const std::string& fileName = "", int line = -1);

/**
 * @brief Report a module loading or resolution error.
 * @param moduleName Name of the module that failed to load.
 * @param reason Human-readable reason for the failure.
 * @param fileName Optional file name where the error occurred.
 * @param line Optional line number where the error occurred.
 */
void moduleError(const std::string& moduleName, const std::string& reason,
                 const std::string& fileName = "", int line = -1);

/**
 * @brief Format an error message for consistent presentation.
 * @param message The raw error message to format.
 * @return The formatted error message string.
 */
std::string formatErrorMessage(const std::string& message);

/**
 * @brief Print a single line to the error output.
 * @param content The line content to print.
 * @param error If true, the line will be printed with error highlighting.
 */
void printLine(const std::string& content, bool error = false);

/**
 * @brief Terminate the process with detailed error information.
 * @param error The ErrorInfo describing the fatal condition.
 */
[[noreturn]] void fatal(const neutron::ErrorInfo& error);

/**
 * @brief Terminate the process with a simple message and error type.
 * @param message Human-readable message describing the fatal condition.
 * @param type The ErrorType to classify the fatal error.
 */
[[noreturn]] void fatal(const std::string& message, neutron::ErrorType type = neutron::ErrorType::RUNTIME_ERROR);

/**
 * @brief Release all internal static resources and caches.
 * 
 * Called before library unload to prevent memory leaks.
 * Do NOT call during normal operation.
 */
void cleanup();

namespace neutron {

/**
 * @brief ErrorHandler - Central Error Reporting System.
 * 
 * The ErrorHandler provides:
 * - Categorized error reporting with appropriate formatting
 * - Source code highlighting at error location
 * - Stack traces for runtime errors
 * - Helpful suggestions for common errors
 * - Colored terminal output (when enabled)
 * - Error counting and summary reporting
 * 
 * Usage Example:
 * @code
 * // Configure error handler
 * ErrorHandler::setColorEnabled(true);
 * ErrorHandler::setSourceLines(sourceLines);
 * 
 * // Report an error
 * ErrorHandler::reportRuntimeError("undefined variable 'x'", "main.nt", 5);
 * 
 * // Check for errors
 * if (ErrorHandler::hadError()) {
 *     ErrorHandler::printSummary();
 *     return 1;
 * }
 * @endcode
 */
class ErrorHandler {
public:
    /**
     * @brief ErrorCallback - Custom error handler function type.
     * 
     * Set via setErrorCallback() to receive notifications
     * for all reported errors.
     */
    using ErrorCallback = std::function<void(const ErrorInfo&)>;
    static void setErrorCallback(ErrorCallback callback);

    // Helper methods (public for NeutronException)
    static std::string getErrorTypeName(ErrorType type);

private:
    // Static state - error handler is a singleton
    static ErrorCallback errorCallback;     ///< Custom error callback
    static bool useColor;                   ///< Enable colored output
    static bool showStackTrace;             ///< Enable stack trace display
    static std::string* currentFileName;    ///< Current file being processed
    static std::vector<std::string>* sourceLines;  ///< Source lines for current file
    static std::unordered_map<std::string, std::vector<std::string>>* fileSources;  ///< Cached source for all files
    static bool cleanedUp;                  ///< Flag to prevent double cleanup
    static bool hasError;                   ///< True if any error occurred
    static int errorCount;                  ///< Total errors reported
    static const int MAX_DETAILED_ERRORS = 4;  ///< Max errors with full details

    // ANSI color codes (cross-platform compatible)
    static const std::string RESET;
    static const std::string RED;
    static const std::string YELLOW;
    static const std::string BLUE;
    static const std::string GREEN;
    static const std::string CYAN;
    static const std::string BOLD;
    static const std::string DIM;

    // Internal helper methods
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

    // Exit with error code (does not return)
    [[noreturn]] static void fatal(const ErrorInfo& error);
    [[noreturn]] static void fatal(const std::string& message, ErrorType type = ErrorType::RUNTIME_ERROR);

    // Cleanup static data (called before library unload)
    static void cleanup();

    // State queries
    static bool isCleanedUp() { return cleanedUp; }
    static bool hadError() { return hasError; }
    static void resetError() { hasError = false; }

    // Get current filename safely
    static std::string getCurrentFileName() { return currentFileName ? *currentFileName : ""; }
};

/**
 * @brief NeutronException - Recoverable exception for REPL and testing.
 * 
 * Unlike fatal errors, NeutronException can be caught and handled.
 * Used in REPL mode to continue execution after errors.
 */
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