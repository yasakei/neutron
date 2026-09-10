/* 
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 *
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * For full license text, see LICENSE file in the root directory.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Code Documentation: Main Entry Point (main.cpp)
 * ===============================================
 * 
 * This file is the entry point for the Neutron interpreter and CLI tool.
 * It handles command-line parsing, file execution, and REPL mode.
 * 
 * What This File Includes:
 * ------------------------ 
 * - run(): Compile and execute Neutron source code
 * - runFile(): Load and execute a Neutron source file
 * - runPrompt(): Interactive REPL for experimentation
 * - main(): Command-line interface and dispatch
 * 
 * How It Works:
 * ------------- 
 * The main function parses command-line arguments and dispatches to
 * appropriate handlers:
 * - File execution: Load .nt files and run through the compiler/VM
 * - REPL: Interactive read-eval-print loop for experimentation
 * - Utilities: fmt (code formatter)
 * - Checkpoint resume: --resume for durable execution
 * 
 * Adding Features:
 * ---------------- 
 * - New CLI commands: Add else-if branch in main() with your command logic
 * - New execution modes: Extend run() with additional flags/parameters
 * - Integration hooks: Use VM::registerComponent() for plugins
 * 
 * What You Should NOT Do:
 * ----------------------- 
 * - Do NOT modify the VM directly from multiple threads
 * - Do NOT bypass the error handler for error reporting
 * - Do NOT remove safety checks for .ntsc (safe) files
 * - Do NOT change command-line argument parsing without updating help text
 * 
 * Command-Line Interface:
 * ----------------------- 
 * neutron [options] [script.nt] [args...]
 * 
 * Options:
 *   --version, -v     Show version information
 *   --resume <file>   Resume from checkpoint
 *   --no-jit          Disable JIT compilation
 *   fmt <file>        Format Neutron source
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <sys/stat.h>
#include "compiler/scanner.h"
#include "compiler/parser.h"
#include "vm.h"
#include "checkpoint.h"
#include "compiler/compiler.h"
#include "compiler/bytecode.h"
#include "modules/module_loader.h"
#include "types/version.h"
#include "runtime/error_handler.h"
#include "platform/platform.h"
#include "formatter.h"

// Forward declarations - because C++ demands forward declarations
void runFile(const std::string& path, neutron::VM& vm);
void runPrompt(neutron::VM& vm);

/**
 * @brief Compile and execute a Neutron source string in the given VM.
 *
 * This is the primary entry point for executing Neutron code. It performs:
 * 1. Lexical analysis (scanning) to produce tokens
 * 2. Parsing to build an AST
 * 3. Bytecode compilation
 * 4. VM interpretation
 *
 * Error handling is done via the ErrorHandler singleton, which tracks
 * errors and produces colored diagnostics with source context.
 *
 * @param source The Neutron source code to compile and run.
 * @param vm The virtual machine instance used for compilation and execution.
 *           The VM's current file/name and module search paths may be used
 *           for diagnostics and module resolution.
 * @param isSafeFile This parameter is deprecated and ignored. All files are now compiled without safety restrictions.
 */
void run(const std::string& source, neutron::VM& vm) {
    // Split source into lines for error reporting
    // The ErrorHandler uses these for producing helpful diagnostics
    std::vector<std::string> lines;
    std::istringstream iss(source);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    // Configure error handler
    // Enable colors and stack traces because debugging is hard enough
    neutron::ErrorHandler::setSourceLines(lines);
    neutron::ErrorHandler::setColorEnabled(true);
    neutron::ErrorHandler::setStackTraceEnabled(true);

    try {
        // Phase 1: Scanning - turn source into tokens
        // If you've ever wondered where "unexpected token" errors come from, this is it
        neutron::Scanner scanner(source);
        std::vector<neutron::Token> tokens = scanner.scanTokens();

        // Phase 2: Parsing - build an abstract syntax tree
        // Where syntax errors are born and dreams go to die
        neutron::Parser parser(tokens);
        std::vector<std::unique_ptr<neutron::Stmt>> statements = parser.parse();

        // Stop if there were any syntax errors
        // No point compiling code that isn't even valid
        if (neutron::ErrorHandler::hadError()) {
            neutron::ErrorHandler::printSummary();
            return;
        }

        // Phase 3: Compilation - AST to bytecode
        // The compiler doesn't judge your coding style (the formatter does that)
        neutron::Compiler compiler(vm);
        neutron::Function* function = compiler.compile(statements);

        // Phase 4: Interpretation - execute the bytecode
        // This is where the magic happens (or the crash, depending on your code quality)
        vm.interpret(function);
    } catch (const std::exception& e) {
        // Runtime errors get the full treatment: error message + stack trace + exit
        // Because "something went wrong" is not a helpful error message
        neutron::ErrorHandler::reportRuntimeError(e.what(), vm.currentFileName);
        neutron::ErrorHandler::printSummary();
        exit(1);
    }
}

/**
 * @brief Load and execute a Neutron source file.
 * 
 * This function handles file I/O, sets up error reporting context,
 * and configures the module search path to include the file's directory.
 * 
 * @param path Path to the Neutron source file (.nt).
 * @param vm The VM instance for execution.
 */
void runFile(const std::string& path, neutron::VM& vm) {
    // Set current file for error reporting
    // Stack traces will show this path, so make it count
    neutron::ErrorHandler::setCurrentFile(path);
    vm.currentFileName = path;

    // Add the script's directory to the module search path
    // This allows modules in the same directory to be imported relatively
    std::string directory;
    // Find last path separator — handle both Unix '/' and Windows '\\'
    const size_t last_slash = path.rfind('/');
    const size_t last_backslash = path.rfind('\\\\');
    size_t last_sep = std::string::npos;
    if (last_slash != std::string::npos && last_backslash != std::string::npos)
        last_sep = std::max(last_slash, last_backslash);
    else if (last_slash != std::string::npos)
        last_sep = last_slash;
    else if (last_backslash != std::string::npos)
        last_sep = last_backslash;

    if (last_sep != std::string::npos) {
        directory = path.substr(0, last_sep);
    }
    vm.add_module_search_path(directory);

    // Read the file content
    // If the file doesn't exist, we fail fast with a clear error
    std::ifstream file(path);
    if (!file.is_open()) {
        neutron::ErrorHandler::fatal("Could not open file: " + path, neutron::ErrorType::IO_ERROR);
    }

    std::string line;
    std::string source;

    while (std::getline(file, line)) {
        source += line + "\n";
    }

    file.close();

    // Execute the source
    run(source, vm);
}

/**
 * @brief Run the interactive REPL (Read-Eval-Print Loop).
 * 
 * The REPL allows interactive experimentation with Neutron.
 * Each line is compiled and executed immediately, with state
 * persisting across iterations (globals, functions, classes).
 * 
 * Features:
 * - Version and platform info on startup
 * - Graceful exit on EOF (Ctrl+D) or interrupt (Ctrl+C)
 * - Error recovery - one bad line doesn't crash the session
 * 
 * @param vm The VM instance (state persists across REPL iterations).
 */
void runPrompt(neutron::VM& vm) {
    std::string line;
    std::cout << "Neutron " << neutron::Version::getVersion() << " REPL" << std::endl;
    std::cout << "Platform: " << neutron::Version::getPlatform() << std::endl;
    std::cout << "Type Ctrl+C to exit" << std::endl;
    std::cout << std::endl;

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            break;  // EOF - time to go home
        }
        try {
            run(line, vm);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
}


int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        
        // Version command
        if (arg == "--version" || arg == "-v") {
            std::cout << neutron::Version::getFullVersion() << std::endl;
            std::cout << "Build: " << neutron::Version::getBuildDate() << std::endl;
            return 0;
        }

        else if (arg == "--resume") {
            if (argc < 3) {
                std::cerr << "Usage: neutron --resume <checkpoint_file>" << std::endl;
                return 1;
            }
            std::string checkpointFile = argv[2];
            neutron::VM vm;
            try {
                neutron::CheckpointManager::loadCheckpoint(vm, checkpointFile);
                // Resume execution
                vm.runPublic();
            } catch (const std::exception& e) {
                std::cerr << "Error resuming checkpoint: " << e.what() << std::endl;
                return 1;
            }
            return 0;
        }
        
        // Format command
        else if (arg == "fmt") {
            if (argc < 3) {
                std::cerr << "Usage: neutron fmt <file.nt> [--check]" << std::endl;
                std::cerr << "       neutron fmt <directory> [--check]" << std::endl;
                std::cerr << std::endl;
                std::cerr << "Options:" << std::endl;
                std::cerr << "  --check    Check if files are formatted (don't modify)" << std::endl;
                std::cerr << "  --indent N Set indent size (default: 4)" << std::endl;
                std::cerr << "  --tabs     Use tabs instead of spaces" << std::endl;
                return 1;
            }
            
            bool checkOnly = false;
            neutron::Formatter::Options fmtOptions;
            std::vector<std::string> targets;
            
            // Parse arguments
            for (int i = 2; i < argc; i++) {
                std::string fmtArg = argv[i];
                if (fmtArg == "--check") {
                    checkOnly = true;
                } else if (fmtArg == "--tabs") {
                    fmtOptions.useSpaces = false;
                } else if (fmtArg == "--indent" && i + 1 < argc) {
                    fmtOptions.indentSize = std::stoi(argv[++i]);
                } else if (fmtArg[0] != '-') {
                    targets.push_back(fmtArg);
                }
            }
            
            if (targets.empty()) {
                std::cerr << "Error: No files specified" << std::endl;
                return 1;
            }
            
            int errorCount = 0;
            int fileCount = 0;
            
            for (const auto& target : targets) {
                if (std::filesystem::is_directory(target)) {
                    // Format all .nt files in directory recursively
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(target)) {
                        if (entry.is_regular_file()) {
                            std::string ext = entry.path().extension().string();
                            if (ext == ".nt") {
                                std::string filePath = entry.path().string();
                                fileCount++;
                                
                                if (checkOnly) {
                                    if (!neutron::Formatter::checkFormat(filePath, fmtOptions)) {
                                        std::cout << "Would format: " << filePath << std::endl;
                                        errorCount++;
                                    }
                                } else {
                                    std::cout << "Formatting: " << filePath << std::endl;
                                    if (!neutron::Formatter::formatFile(filePath, fmtOptions)) {
                                        errorCount++;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    // Format single file
                    fileCount++;
                    if (checkOnly) {
                        if (!neutron::Formatter::checkFormat(target, fmtOptions)) {
                            std::cout << "Would format: " << target << std::endl;
                            errorCount++;
                        }
                    } else {
                        std::cout << "Formatting: " << target << std::endl;
                        if (!neutron::Formatter::formatFile(target, fmtOptions)) {
                            errorCount++;
                        }
                    }
                }
            }
            
            if (checkOnly) {
                if (errorCount > 0) {
                    std::cout << "\n" << errorCount << " file(s) would be formatted." << std::endl;
                    return 1;
                } else {
                    std::cout << "All " << fileCount << " file(s) are properly formatted." << std::endl;
                    return 0;
                }
            } else {
                std::cout << "\nFormatted " << fileCount << " file(s)." << std::endl;
                return errorCount > 0 ? 1 : 0;
            }
        }
        
        // Legacy --build-box command - deprecated in favor of nt-box system
        else if (arg == "--build-box" && argc > 2) {
            std::string module_name = argv[2];
            std::cerr << "Warning: --build-box is deprecated." << std::endl;
            std::cerr << "Use: box build native " << module_name << " instead" << std::endl;
            std::cerr << "Run 'box help' for more information." << std::endl;
            return 1;
        }

    }

    // Default behavior: run file or REPL
    neutron::VM vm;
    
    if (argc >= 2) {
        // Parse runtime flags before running the file
        std::string filePath;
        for (int i = 1; i < argc; i++) {
            std::string a = argv[i];
            if (a == "--no-jit") {
                vm.jitEnabled = false;
            } else if (filePath.empty() && a[0] != '-') {
                filePath = a;
            }
            vm.commandLineArgs.push_back(a);
        }
        
        if (filePath.empty()) {
            std::cerr << "Error: No source file specified." << std::endl;
            return 1;
        }
        
        runFile(filePath, vm);
    } else {
        runPrompt(vm);
    }
    
    return 0;
}
