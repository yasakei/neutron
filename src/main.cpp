#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <iomanip>
#include "compiler/scanner.h"
#include "compiler/parser.h"
#include "vm.h"
#include "compiler/compiler.h"
#include "compiler/bytecode.h"
#include "modules/module_loader.h"

void run(const std::string& source, neutron::VM& vm);
void runFile(const std::string& path, neutron::VM& vm);
void runPrompt(neutron::VM& vm);
void saveBytecodeToExecutable(const std::string& sourceCode, const std::string& outputPath, const std::string& sourcePath);

void run(const std::string& source, neutron::VM& vm) {
    neutron::Scanner scanner(source);
    std::vector<neutron::Token> tokens = scanner.scanTokens();
    
    neutron::Parser parser(tokens);
    std::vector<std::unique_ptr<neutron::Stmt>> statements = parser.parse();
    
    neutron::Compiler compiler(vm);
    neutron::Function* function = compiler.compile(statements);
    
    vm.interpret(function);
}

void runFile(const std::string& path, neutron::VM& vm) {
    // Add the script's directory to the module search path
    std::string directory;
    const size_t last_slash_idx = path.rfind('/');
    if (std::string::npos != last_slash_idx) {
        directory = path.substr(0, last_slash_idx);
    }
    vm.add_module_search_path(directory);

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << path << std::endl;
        exit(1);
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
    std::cout << "Neutron REPL (Press Ctrl+C to exit)" << std::endl;
    
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        run(line, vm);
    }
}

void saveBytecodeToExecutable(const std::string& sourceCode, const std::string& outputPath, const std::string& sourcePath) {
    // Create a C++ source file with embedded source code
    std::string executableSourcePath = outputPath + "_main.cpp";
    std::ofstream executableFile(executableSourcePath);
    
    if (!executableFile.is_open()) {
        std::cerr << "Could not create executable source file: " << executableSourcePath << std::endl;
        exit(1);
    }
    
    executableFile << "// Auto-generated executable for " << sourcePath << "\n";
    executableFile << "#include <iostream>\n";
    executableFile << "#include <string>\n";
    executableFile << "#include <vector>\n";
    executableFile << "#include <map>\n";
    executableFile << "#include \"scanner.h\"\n";
    executableFile << "#include \"parser.h\"\n";
    executableFile << "#include \"vm.h\"\n";
    executableFile << "#include \"compiler.h\"\n\n";
    
    // Get used modules
    std::vector<std::string> usedModules = neutron::getUsedModules(sourceCode);
    
    // Embed module source code (for Neutron modules only)
    for (const auto& moduleName : usedModules) {
        std::string libModulePath = "lib/" + moduleName + ".nt";
        std::ifstream moduleFile(libModulePath);
        
        if (!moduleFile.is_open()) {
            // Check if it's a neutron module in the box directory (not native)
            std::string boxModulePath = "box/" + moduleName + "/" + moduleName + ".nt";
            std::ifstream boxModuleFile(boxModulePath);
            
            if (boxModuleFile.is_open()) {
                // It's a neutron module in the box directory
                std::string moduleSource((std::istreambuf_iterator<char>(boxModuleFile)), std::istreambuf_iterator<char>());
                executableFile << "const char* " << moduleName << "_source = R\"neutron_source(\n";
                executableFile << moduleSource;
                executableFile << "\n)neutron_source\";\n\n";
                boxModuleFile.close();
            } else {
                // It might be a native module - we'll handle it differently when compiling
                continue; // Don't embed native module source
            }
        } else {
            // It's a lib module
            std::string moduleSource((std::istreambuf_iterator<char>(moduleFile)), std::istreambuf_iterator<char>());
            executableFile << "const char* " << moduleName << "_source = R\"neutron_source(\n";
            executableFile << moduleSource;
            executableFile << "\n)neutron_source\";\n\n";
            moduleFile.close();
        }
    }
    
    // Write the source code as a string literal
    executableFile << "const char* embedded_source = R\"neutron_source(\n";
    executableFile << sourceCode;
    executableFile << "\n)neutron_source\";\n\n";
    
    // Write main function that uses the embedded source code
    executableFile << "int main() {\n";
    executableFile << "    // Initialize the VM\n";
    executableFile << "    neutron::VM vm;\n\n";
    
    // Load Neutron modules (not native ones)
    for (const auto& moduleName : usedModules) {
        std::string libModulePath = "lib/" + moduleName + ".nt";
        std::ifstream moduleFile(libModulePath);
        
        if (!moduleFile.is_open()) {
            // Check if it's a neutron module in the box directory
            std::string boxModulePath = "box/" + moduleName + "/" + moduleName + ".nt";
            std::ifstream boxModuleFile(boxModulePath);
            
            if (boxModuleFile.is_open()) {
                // It's a neutron module in the box directory
                executableFile << "    {\n";
                executableFile << "        neutron::Scanner scanner(" << moduleName << "_source);\n";
                executableFile << "        std::vector<neutron::Token> tokens = scanner.scanTokens();\n";
                executableFile << "        neutron::Parser parser(tokens);\n";
                executableFile << "        std::vector<std::unique_ptr<neutron::Stmt>> statements = parser.parse();\n";
                executableFile << "        neutron::Compiler compiler(vm);\n";
                executableFile << "        neutron::Function* function = compiler.compile(statements);\n";
                executableFile << "        vm.interpret(function);\n";
                executableFile << "    }\n";
                boxModuleFile.close();
            } else {
                // This is a native module - it will be linked statically, so no need to load here
                continue;
            }
        } else {
            // It's a lib module
            executableFile << "    {\n";
            executableFile << "        neutron::Scanner scanner(" << moduleName << "_source);\n";
            executableFile << "        std::vector<neutron::Token> tokens = scanner.scanTokens();\n";
            executableFile << "        neutron::Parser parser(tokens);\n";
            executableFile << "        std::vector<std::unique_ptr<neutron::Stmt>> statements = parser.parse();\n";
            executableFile << "        neutron::Compiler compiler(vm);\n";
            executableFile << "        neutron::Function* function = compiler.compile(statements);\n";
            executableFile << "        vm.interpret(function);\n";
            executableFile << "    }\n";
            moduleFile.close();
        }
    }
    
    executableFile << "    // Run the embedded source code\n";
    executableFile << "    neutron::Scanner scanner(embedded_source);\n";
    executableFile << "    std::vector<neutron::Token> tokens = scanner.scanTokens();\n\n";
    executableFile << "    neutron::Parser parser(tokens);\n";
    executableFile << "    std::vector<std::unique_ptr<neutron::Stmt>> statements = parser.parse();\n\n";
    executableFile << "    neutron::Compiler compiler(vm);\n";
    executableFile << "    neutron::Function* function = compiler.compile(statements);\n\n";
    executableFile << "    vm.interpret(function);\n\n";
    executableFile << "    return 0;\n";
    executableFile << "}\n";
    
    executableFile.close();
    
    // Build native modules if needed and prepare the compilation command
    std::string compileCommand = "g++ -std=c++17 -Wall -Wextra -O2 -Iinclude -I. -Ilibs/json -Ilibs/http -Ilibs/time -Ilibs/sys -Ibox -Ilibs/websocket " +
                                executableSourcePath + " " +
                                "build/parser.o build/scanner.o build/capi.o build/runtime.o build/module_utils.o " +
                                "build/bytecode.o build/debug.o build/compiler.o build/vm.o build/token.o build/module_registry.o " +
                                "build/sys/native.o build/convert/native.o build/json/native.o build/math/native.o " +
                                "build/http/native.o build/time/native.o build/module_loader.o ";
    
    // Check for and compile native modules that are used
    for (const auto& moduleName : usedModules) {
        std::string nativeCppPath = "box/" + moduleName + "/native.cpp";
        std::string nativeCPath = "box/" + moduleName + "/native.c";
        
        if (std::ifstream(nativeCppPath).good()) {
            // Compile native.cpp for this module
            std::string moduleObj = "build/box/" + moduleName + ".o";
            
            // Create the build directory if it doesn't exist
            system("mkdir -p build/box");
            
            // Compile the native module to an object file
            std::string objCmd = "g++ -std=c++17 -Wall -Wextra -O2 -fPIC -Iinclude -I. -Ilibs -Ibox -c " +
                                nativeCppPath + " -o " + moduleObj;
            int objResult = system(objCmd.c_str());
            if (objResult == 0) {
                compileCommand += moduleObj + " ";
                std::cout << "Compiled native module: " << moduleName << std::endl;
            } else {
                std::cerr << "Failed to compile native module: " << moduleName << std::endl;
                exit(1);
            }
        } else if (std::ifstream(nativeCPath).good()) {
            // Compile native.c for this module
            std::string moduleObj = "build/box/" + moduleName + ".o";
            
            // Create the build directory if it doesn't exist
            system("mkdir -p build/box");
            
            // Compile the native module to an object file
            std::string objCmd = "g++ -std=c++17 -Wall -Wextra -O2 -fPIC -Iinclude -I. -Ilibs -Ibox -c " +
                                nativeCPath + " -o " + moduleObj;
            int objResult = system(objCmd.c_str());
            if (objResult == 0) {
                compileCommand += moduleObj + " ";
                std::cout << "Compiled native module: " << moduleName << std::endl;
            } else {
                std::cerr << "Failed to compile native module: " << moduleName << std::endl;
                exit(1);
            }
        }
    }
    
    compileCommand += "-lcurl -ljsoncpp -o " + outputPath;
    
    std::cout << "Compiling to executable: " << outputPath << std::endl;
    int result = system(compileCommand.c_str());
    
    // Clean up the temporary source file
    std::remove(executableSourcePath.c_str());
    
    if (result == 0) {
        std::cout << "Executable created: " << outputPath << std::endl;
        // Make the file executable
        std::string chmodCommand = "chmod +x " + outputPath;
        if (system(chmodCommand.c_str()) != 0) {
            std::cerr << "Failed to make file executable" << std::endl;
        }
    } else {
        std::cerr << "Failed to create executable" << std::endl;
        exit(1);
    }
}


int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--version") {
            std::cout << "Neutron 1.0.2 (Alpha)" << std::endl;
            return 0;
        } else if (arg == "--build-box" && argc > 2) {
                        std::string module_name = argv[2];
                        std::string command = "make build-box MODULE=" + module_name;
                        int result = system(command.c_str());
                        if (result != 0) {
                            std::cerr << "Failed to build box module: " << module_name << std::endl;
                        }
                        return result;
        } else if (arg == "-b" && argc > 2) {
            // Binary conversion mode - create standalone executable
            std::string inputPath = argv[2];
            std::string outputPath = inputPath + ".out";
            
            if (argc > 3) {
                outputPath = argv[3];
            }
            
            // Read the source file
            std::ifstream file(inputPath);
            if (!file.is_open()) {
                std::cerr << "Could not open file: " << inputPath << std::endl;
                exit(1);
            }
            
            std::string line;
            std::string source;
            
            while (std::getline(file, line)) {
                source += line + "\n";
            }
            
            file.close();
            
            // Save source code to standalone executable
            saveBytecodeToExecutable(source, outputPath, inputPath);
            
            return 0;
        }
    }

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