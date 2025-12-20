/*
 * Neutron Programming Language
 * Copyright (c) 2025 yasakei
 * 
 * This software is distributed under the Neutron Public License 1.0.
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

void run(const std::string& source, neutron::VM& vm);
void runFile(const std::string& path, neutron::VM& vm);
void runPrompt(neutron::VM& vm);

void run(const std::string& source, neutron::VM& vm) {
    // Split source into lines for error reporting
    std::vector<std::string> lines;
    std::istringstream iss(source);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    
    // Configure error handler
    neutron::ErrorHandler::setSourceLines(lines);
    neutron::ErrorHandler::setColorEnabled(true);
    neutron::ErrorHandler::setStackTraceEnabled(true);
    
    try {
        neutron::Scanner scanner(source);
        std::vector<neutron::Token> tokens = scanner.scanTokens();
        
        neutron::Parser parser(tokens);
        std::vector<std::unique_ptr<neutron::Stmt>> statements = parser.parse();
        
        neutron::Compiler compiler(vm);
        neutron::Function* function = compiler.compile(statements);
        
        vm.interpret(function);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        exit(1);
    }
}

void runFile(const std::string& path, neutron::VM& vm) {
    // Set current file for error reporting
    neutron::ErrorHandler::setCurrentFile(path);
    vm.currentFileName = path;
    
    // Add the script's directory to the module search path
    std::string directory;
    const size_t last_slash_idx = path.rfind('/');
    if (std::string::npos != last_slash_idx) {
        directory = path.substr(0, last_slash_idx);
    }
    vm.add_module_search_path(directory);

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
    
    run(source, vm);
}

void runPrompt(neutron::VM& vm) {
    std::string line;
    std::cout << "Neutron " << neutron::Version::getVersion() << " REPL" << std::endl;
    std::cout << "Platform: " << neutron::Version::getPlatform() << std::endl;
    std::cout << "Type Ctrl+C to exit" << std::endl;
    std::cout << std::endl;
    
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            break;
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
            std::cout << "Running: " << config->name << " v" << config->version << std::endl;
            std::cout << "Entry: " << config->entry << "\n" << std::endl;
            
            neutron::VM vm;
            vm.commandLineArgs.push_back(entryFile);
            runFile(entryFile, vm);
            return 0;
        }
        
        else if (arg == "build") {
            // Check for --no-bundle flag (bundling is now default)
            bool bundleLibs = true;
            if (argc > 2 && std::string(argv[2]) == "--no-bundle") {
                bundleLibs = false;
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
            
            std::cout << "Building: " << config->name << " v" << config->version << std::endl;
            std::cout << "Entry: " << config->entry << std::endl;
            if (bundleLibs) {
                std::cout << "Mode: Bundle shared libraries" << std::endl;
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
            
            std::cout << "Output: " << outputPath << "\n" << std::endl;
            
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
                bundleLibs
            );
            
            return success ? 0 : 1;
        }
        
        else if (arg == "install") {
            if (argc < 3) {
                std::cerr << "Usage: neutron install <package>" << std::endl;
                std::cerr << "Available packages: box" << std::endl;
                return 1;
            }
            
            std::string package = argv[2];
            if (package == "box") {
                return neutron::ProjectManager::installBox() ? 0 : 1;
            } else {
                std::cerr << "Error: Unknown package: " << package << std::endl;
                std::cerr << "Available packages: box" << std::endl;
                return 1;
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
        std::string filePath = argv[1];
        
        // Store all command line arguments in VM (script name and any additional args)
        for (int i = 1; i < argc; i++) {
            vm.commandLineArgs.push_back(std::string(argv[i]));
        }
        
        runFile(filePath, vm);
    } else {
        runPrompt(vm);
    }
    
    return 0;
}