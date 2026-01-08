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
ErrorHandler::ErrorCallback ErrorHandler::errorCallback = nullptr;
bool ErrorHandler::useColor = true;
bool ErrorHandler::showStackTrace = true;
std::string* ErrorHandler::currentFileName = new std::string("");
std::vector<std::string>* ErrorHandler::sourceLines = new std::vector<std::string>();
std::unordered_map<std::string, std::vector<std::string>>* ErrorHandler::fileSources = new std::unordered_map<std::string, std::vector<std::string>>();
bool ErrorHandler::cleanedUp = false;
bool ErrorHandler::hasError = false;
int ErrorHandler::errorCount = 0;

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

void ErrorHandler::setErrorCallback(ErrorCallback callback) {
    errorCallback = callback;
}

void ErrorHandler::setColorEnabled(bool enabled) {
    useColor = enabled && isTerminalColorSupported();
}

void ErrorHandler::setStackTraceEnabled(bool enabled) {
    showStackTrace = enabled;
}

void ErrorHandler::setCurrentFile(const std::string& fileName) {
    if (currentFileName) {
        *currentFileName = fileName;
    }
}

/**
 * @brief Store the provided source lines as the current active source and, when a current file is set, associate them with that file.
 *
 * Copies `lines` into the internal buffer used for error reporting; if a current file name has been set, the same lines
 * are also stored in the per-file source map so future errors for that file can retrieve the source.
 *
 * @param lines The file's source text split into individual lines.
 */
void ErrorHandler::setSourceLines(const std::vector<std::string>& lines) {
    if (sourceLines) {
        *sourceLines = lines;
    }
    if (currentFileName && !currentFileName->empty() && fileSources) {
        (*fileSources)[*currentFileName] = lines;
    }
}

/**
 * @brief Store a file's source text as a vector of lines for later error reporting.
 *
 * Splits the provided `source` text into lines (by newline characters) and associates
 * the resulting line vector with `fileName` in the internal per-file source map.
 * If the internal map does not yet exist it will be created. Any existing entry for
 * `fileName` will be replaced.
 *
 * @param fileName Name or path of the source file to register.
 * @param source Full contents of the file; will be split into lines and stored.
 */
void ErrorHandler::addFileSource(const std::string& fileName, const std::string& source) {
    if (!fileSources) {
        fileSources = new std::unordered_map<std::string, std::vector<std::string>>();
    }
    
    std::vector<std::string> lines;
    std::stringstream ss(source);
    std::string line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    
    (*fileSources)[fileName] = lines;
}

/**
 * @brief Prints a compact summary of accumulated errors.
 *
 * If any errors have been recorded, emits a single-line summary to standard error
 * indicating the total number of errors (e.g., "Found 3 errors.").
 */
void ErrorHandler::printSummary() {
    if (errorCount > 0) {
        std::cerr << "\nFound " << errorCount << " error" << (errorCount == 1 ? "" : "s") << "." << std::endl;
    }
}

/**
 * @brief Clears all tracked error state and stored source data.
 *
 * Resets the error flag and error counter, clears the current file name,
 * and removes all cached source lines and per-file source mappings.
 */
void ErrorHandler::reset() {
    hasError = false;
    errorCount = 0;
    if (currentFileName) *currentFileName = "";
    if (sourceLines) sourceLines->clear();
    if (fileSources) fileSources->clear();
}

/**
 * @brief Map an ErrorType enum value to its canonical error type name.
 *
 * @return std::string The human-readable error type name (for example "SyntaxError",
 * "RuntimeError", "TypeError"). Returns "Error" for unknown or default cases.
 */
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

/**
 * @brief Report an error by formatting and emitting either a detailed diagnostic or a compact summary.
 *
 * Increments the global error state and, if the number of reported errors exceeds the detailed limit,
 * emits a compact file/line-prefixed summary. Otherwise produces a multi-line diagnostic that includes
 * the error type, optional file and location, the message, an optional source line with a column
 * highlight, an optional suggestion, and an optional stack trace. Output uses ANSI colors when enabled.
 *
 * @param error Structure containing error details. Observed fields: `type`, `message`, `fileName`,
 *              `line`, `column`, `sourceLine`, `suggestion`, and `stackTrace`.
 */
void ErrorHandler::reportError(const ErrorInfo& error) {
    hasError = true;
    errorCount++;
    
    if (errorCallback) {
        errorCallback(error);
        return;
    }

    // If we've exceeded the detailed error limit, just print a compact message
    if (errorCount > MAX_DETAILED_ERRORS) {
        std::ostringstream oss;
        
        if (!error.fileName.empty()) {
            oss << "[" << error.fileName;
            if (error.line > 0) {
                oss << ":" << error.line;
            }
            oss << "] ";
        }
        
        oss << getErrorTypeName(error.type) << ": " << error.message;
        std::cerr << oss.str() << std::endl;
        return;
    }

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
    ErrorInfo error(ErrorType::SYNTAX_ERROR, message, currentFileName ? *currentFileName : "", token.line, 0);
    
    // Try to get the source line
    if (sourceLines && token.line > 0 && token.line <= static_cast<int>(sourceLines->size())) {
        error.sourceLine = (*sourceLines)[token.line - 1];
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

/**
 * @brief Report a runtime error with contextual source and stack information.
 *
 * Constructs an ErrorInfo for a runtime error using the provided message, file, and line;
 * attaches the provided stack trace, attempts to populate the corresponding source line
 * (preferring per-file sources if available; line numbers are 1-based), derives a suggested
 * fix, and forwards the error to the central reporting routine.
 *
 * @param message Error message describing the runtime failure.
 * @param fileName File name where the error occurred; if empty, the current file name is used.
 * @param line 1-based line number within the file where the error occurred.
 * @param trace Stack trace frames to associate with the runtime error.
 */
void ErrorHandler::reportRuntimeError(const std::string& message, const std::string& fileName,
                                      int line, const std::vector<StackFrame>& trace) {
    std::string actualFileName = fileName.empty() ? (currentFileName ? *currentFileName : "") : fileName;
    ErrorInfo error(ErrorType::RUNTIME_ERROR, message, actualFileName, line);
    error.stackTrace = trace;
    
    // Try to get the source line
    if (fileSources && fileSources->count(actualFileName)) {
        const auto& lines = (*fileSources)[actualFileName];
        if (line > 0 && line <= static_cast<int>(lines.size())) {
            error.sourceLine = lines[line - 1];
        }
    } else if (sourceLines && line > 0 && line <= static_cast<int>(sourceLines->size())) {
        error.sourceLine = (*sourceLines)[line - 1];
    }
    
    error.suggestion = getSuggestion(ErrorType::RUNTIME_ERROR, message);
    
    reportError(error);
}

/**
 * @brief Report a lexical (tokenization) error at a specific file location.
 *
 * Attempts to attach the source line for the given file and line (preferring per-file sources when available),
 * generates a contextual suggestion, and emits a formatted lexical error report.
 *
 * @param message Human-readable error message describing the lexical problem.
 * @param line 1-based line number where the error occurred.
 * @param column 1-based column number indicating the error position within the line.
 * @param fileName File path associated with the error; if empty, the current file context is used when available.
 */
void ErrorHandler::reportLexicalError(const std::string& message, int line, int column,
                                      const std::string& fileName) {
    std::string actualFileName = fileName.empty() ? (currentFileName ? *currentFileName : "") : fileName;
    ErrorInfo error(ErrorType::LEXICAL_ERROR, message, actualFileName, 
                   line, column);
    
    // Try to get the source line
    if (fileSources && fileSources->count(actualFileName)) {
        const auto& lines = (*fileSources)[actualFileName];
        if (line > 0 && line <= static_cast<int>(lines.size())) {
            error.sourceLine = lines[line - 1];
        }
    } else if (sourceLines && line > 0 && line <= static_cast<int>(sourceLines->size())) {
        error.sourceLine = (*sourceLines)[line - 1];
    }
    
    error.suggestion = getSuggestion(ErrorType::LEXICAL_ERROR, message);
    
    reportError(error);
}

void ErrorHandler::typeError(const std::string& expected, const std::string& got, 
                             const std::string& fileName, int line) {
    std::string message = "Expected " + expected + " but got " + got;
    ErrorInfo error(ErrorType::TYPE_ERROR, message, fileName.empty() ? ErrorHandler::getCurrentFileName() : fileName, line);
    error.suggestion = getSuggestion(ErrorType::TYPE_ERROR, message);
    reportError(error);
}

void ErrorHandler::referenceError(const std::string& name, const std::string& fileName, 
                                  int line, bool isFunction) {
    std::string message = "Undefined " + std::string(isFunction ? "function" : "variable") + " '" + name + "'";
    ErrorInfo error(ErrorType::REFERENCE_ERROR, message, fileName.empty() ? ErrorHandler::getCurrentFileName() : fileName, line);
    error.suggestion = getSuggestion(ErrorType::REFERENCE_ERROR, message);
    reportError(error);
}

void ErrorHandler::rangeError(const std::string& message, const std::string& fileName, int line) {
    ErrorInfo error(ErrorType::RANGE_ERROR, message, fileName.empty() ? ErrorHandler::getCurrentFileName() : fileName, line);
    error.suggestion = getSuggestion(ErrorType::RANGE_ERROR, message);
    reportError(error);
}

void ErrorHandler::argumentError(int expected, int got, const std::string& functionName,
                                const std::string& fileName, int line) {
    std::string message = "Function '" + functionName + "' expects " + std::to_string(expected) + 
                         " argument" + (expected != 1 ? "s" : "") + " but got " + std::to_string(got);
    ErrorInfo error(ErrorType::ARGUMENT_ERROR, message, fileName.empty() ? ErrorHandler::getCurrentFileName() : fileName, line);
    error.suggestion = getSuggestion(ErrorType::ARGUMENT_ERROR, message);
    reportError(error);
}

void ErrorHandler::divisionError(const std::string& fileName, int line) {
    std::string message = "Division by zero is not allowed";
    ErrorInfo error(ErrorType::DIVISION_ERROR, message, fileName.empty() ? ErrorHandler::getCurrentFileName() : fileName, line);
    error.suggestion = "Check that the divisor is not zero before performing division or modulo operations.";
    reportError(error);
}

void ErrorHandler::stackOverflowError(const std::string& fileName, int line) {
    std::string message = "Stack overflow - maximum call depth exceeded";
    ErrorInfo error(ErrorType::STACK_ERROR, message, fileName.empty() ? ErrorHandler::getCurrentFileName() : fileName, line);
    error.suggestion = "This usually indicates infinite recursion. Check your recursive function calls.";
    reportError(error);
}

void ErrorHandler::moduleError(const std::string& moduleName, const std::string& reason,
                               const std::string& fileName, int line) {
    std::string message = "Failed to load module '" + moduleName + "': " + reason;
    ErrorInfo error(ErrorType::MODULE_ERROR, message, fileName.empty() ? ErrorHandler::getCurrentFileName() : fileName, line);
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
    ErrorInfo error(type, message, getCurrentFileName());
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

void ErrorHandler::cleanup() {
    if (cleanedUp) return;  // Prevent double cleanup
    cleanedUp = true;
    
    if (sourceLines) {
        delete sourceLines;
        sourceLines = nullptr;
    }
    
    if (currentFileName) {
        delete currentFileName;
        currentFileName = nullptr;
    }
}

} // namespace neutron

// Note: Destructor attribute disabled to prevent conflicts with manual cleanup
// Manual cleanup in main() is sufficient for proper resource management