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
 * It handles command-line parsing, file execution, REPL mode, and project
 * management commands.
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
 * - File execution: Load .nt/.ntsc files and run through the compiler/VM
 * - REPL: Interactive read-eval-print loop for experimentation
 * - Project commands: init, run, build (via ProjectManager)
 * - Utilities: fmt (code formatter), install (package manager)
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
 *   init <name>       Initialize new project
 *   run               Run project entry point
 *   build [opts]      Build project executable
 *   install [pkg]     Install packages
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
#include "project/project_manager.h"
#include "project/project_config.h"
#include "project/project_builder.h"
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
 * @param isSafeFile When true, compile the source with safety restrictions
 *                   appropriate for "safe" files (e.g., sandboxing or reduced
 *                   permissions). Default is false.
 * 
 * Safety Notes:
 * - Safe files (.ntsc) have restricted I/O and system access
 * - Errors during compilation stop execution before VM interpretation
 * - Runtime errors trigger stack trace output and process exit
 */
void run(const std::string& source, neutron::VM& vm, bool isSafeFile = false) {
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
        neutron::Compiler compiler(vm, isSafeFile);
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
 * Special handling for .ntsc (Neutron Safe Code) files:
 * - These files are compiled with restricted permissions
 * - No file I/O, no system calls, no network access
 * - Suitable for untrusted code execution
 * 
 * @param path Path to the Neutron source file (.nt or .ntsc).
 * @param vm The VM instance for execution.
 */
void runFile(const std::string& path, neutron::VM& vm) {
    // Set current file for error reporting
    // Stack traces will show this path, so make it count
    neutron::ErrorHandler::setCurrentFile(path);
    vm.currentFileName = path;

    // Check if this is a .ntsc file (Neutron Safe Code)
    // Safe files are sandboxed - they can't touch the filesystem or run system commands
    bool isSafeFile = false;
    if (path.length() >= 5 && path.substr(path.length() - 5) == ".ntsc") {
        isSafeFile = true;
        vm.isSafeFile = true;
    }

    // Add the script's directory to the module search path
    // This allows modules in the same directory to be imported relatively
    std::string directory;
    // Find last path separator — handle both Unix '/' and Windows '\'
    const size_t last_slash = path.rfind('/');
    const size_t last_backslash = path.rfind('\\');
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
    run(source, vm, isSafeFile);
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
        
        // Project commands
        else if (arg == "init") {
            std::string projectName = argc > 2 ? argv[2] : "";
            return neutron::ProjectManager::initProject(projectName) ? 0 : 1;
        }
        
        else if (arg == "run") {
            // Check if we're in a Neutron project
            std::string projectRoot = neutron::ProjectManager::findProjectRoot(".");
            if (projectRoot.empty()) {
                std::cerr << "Error: Not in a Neutron project. Run './neutron init' to create one." << std::endl;
                return 1;
            }
            
            // Load config and run the entry file
            auto config = neutron::ProjectManager::loadConfig(projectRoot);
            if (!config) {
                std::cerr << "Error: Failed to load project configuration" << std::endl;
                return 1;
            }

            std::string entryFile = projectRoot + "/" + config->entry;

            neutron::VM vm;
            vm.commandLineArgs.push_back(entryFile);
            runFile(entryFile, vm);
            return 0;
        }
        
        else if (arg == "build") {
            bool bundleLibs = true;
            bool aotCompile = true;  // AOT is now the default
            std::string targetArch = "";  // Cross-compilation target

            for (int i = 2; i < argc; i++) {
                std::string flag = argv[i];
                if (flag == "--no-bundle") {
                    bundleLibs = false;
                } else if (flag == "--aot" || flag == "-c") {
                    aotCompile = true;
                } else if (flag == "--no-aot" || flag == "--interpret") {
                    aotCompile = false;  // Allow fallback to interpreter mode
                } else if (flag == "--target" && i + 1 < argc) {
                    targetArch = argv[++i];  // Cross-compilation target
                } else if (flag.find("--target=") == 0) {
                    targetArch = flag.substr(9);  // --target=xxx format
                }
            }

            // Check if we're in a Neutron project
            std::string projectRoot = neutron::ProjectManager::findProjectRoot(".");
            if (projectRoot.empty()) {
                std::cerr << "Error: Not in a Neutron project. Run './neutron init' to create one." << std::endl;
                return 1;
            }

            // Load config
            auto config = neutron::ProjectManager::loadConfig(projectRoot);
            if (!config) {
                std::cerr << "Error: Failed to load project configuration" << std::endl;
                return 1;
            }

            std::string entryFile = projectRoot + "/" + config->entry;

            std::cout << "Building: " << config->name << std::endl;
            std::cout << "  Mode: " << (aotCompile ? "AOT (native)" : "Interpreter") << std::endl;
            if (!targetArch.empty()) {
                std::cout << "  Target: " << targetArch << " (cross-compilation)" << std::endl;
            }

            // Create build directory
            std::string buildDir = projectRoot + "/build";
            std::filesystem::create_directories(buildDir);

            // Output executable name
            std::string outputName = config->name;
#ifdef _WIN32
            outputName += ".exe";
#endif
            std::string outputPath = buildDir + "/" + outputName;

            std::cout << "  Output: " << outputPath << std::endl;
            std::cout << std::endl;
            
            // Read the entry file
            std::ifstream file(entryFile);
            if (!file.is_open()) {
                std::cerr << "Error: Could not open entry file: " << entryFile << std::endl;
                return 1;
            }
            
            std::string line;
            std::string source;
            while (std::getline(file, line)) {
                source += line + "\n";
            }
            file.close();
            
            // Build using project-aware builder
            bool success = neutron::ProjectBuilder::buildProjectExecutable(
                projectRoot,
                source,
                entryFile,
                outputPath,
                neutron::platform::getExecutablePath(),
                bundleLibs,
                aotCompile,
                targetArch
            );
            
            return success ? 0 : 1;
        }
        
        else if (arg == "install") {
            // If no arguments, install all dependencies from .quark
            if (argc < 3) {
                std::string projectRoot = neutron::ProjectManager::findProjectRoot(".");
                if (projectRoot.empty()) {
                    std::cerr << "Error: Not in a Neutron project" << std::endl;
                    return 1;
                }

                std::string quarkPath = projectRoot + "/.quark";
                std::ifstream quarkFile(quarkPath);
                if (!quarkFile.is_open()) {
                    std::cerr << "Error: No .quark file found" << std::endl;
                    return 1;
                }

                std::cout << "Reading dependencies from .quark..." << std::endl;
                std::cout << std::endl;

                std::string line;
                bool inDeps = false;
                std::vector<std::string> deps;

                while (std::getline(quarkFile, line)) {
                    if (line == "[dependencies]") {
                        inDeps = true;
                        continue;
                    }
                    if (inDeps && line[0] != '[' && !line.empty() && line[0] != '#') {
                        size_t eq = line.find('=');
                        if (eq != std::string::npos) {
                            std::string name = line.substr(0, eq);
                            std::string version = line.substr(eq + 1);
                            // Trim whitespace
                            name.erase(0, name.find_first_not_of(" \t\r\n"));
                            name.erase(name.find_last_not_of(" \t\r\n") + 1);
                            version.erase(0, version.find_first_not_of(" \t\r\n"));
                            version.erase(version.find_last_not_of(" \t\r\n") + 1);
                            // Remove quotes if present
                            if (version.size() >= 2) {
                                if ((version.front() == '"' && version.back() == '"') ||
                                    (version.front() == '\'' && version.back() == '\'')) {
                                    version = version.substr(1, version.size() - 2);
                                }
                            }
                            if (!name.empty() && !version.empty()) {
                                deps.push_back(name + "@" + version);
                            }
                        }
                    }
                }
                quarkFile.close();

                if (deps.empty()) {
                    std::cout << "No dependencies found in .quark" << std::endl;
                    return 0;
                }

                std::cout << "Found " << deps.size() << " dependenc" << (deps.size() == 1 ? "y" : "ies") << ":" << std::endl;
                for (const auto& dep : deps) {
                    std::cout << "  - " << dep << std::endl;
                }
                std::cout << std::endl;

                // Find box executable
                std::string boxExe = neutron::platform::getExecutablePath();
                size_t lastSlash = boxExe.find_last_of("/\\");
                if (lastSlash != std::string::npos) {
                    boxExe = boxExe.substr(0, lastSlash) + "/box";
                } else {
                    boxExe = "./box";
                }

                // Install each dependency
                int installed = 0;
                for (const auto& dep : deps) {
                    std::string cmd = boxExe + " install \"" + dep + "\"";
                    int result = system(cmd.c_str());
                    if (result == 0) {
                        installed++;
                    }
                }

                std::cout << std::endl;
                std::cout << "Installed " << installed << "/" << deps.size() << " packages" << std::endl;
                return (installed == deps.size()) ? 0 : 1;
            }

            // Install specific package
            std::string package = argv[2];
            std::string boxPath = neutron::platform::getExecutablePath();
            size_t lastSlash = boxPath.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                boxPath = boxPath.substr(0, lastSlash) + "/box";
            } else {
                boxPath = "./box";
            }

            std::string cmd = "\"" + boxPath + "\" install " + package;
            for (int i = 3; i < argc; i++) {
                cmd += " " + std::string(argv[i]);
            }

            return system(cmd.c_str());
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
                            if (ext == ".nt" || ext == ".ntsc") {
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