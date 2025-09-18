#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include "scanner.h"
#include "parser.h"
#include "vm.h"
#include "compiler.h"

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

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--version") {
            std::cout << "Neutron 0.1(Alpha)" << std::endl;
            return 0;
        } else if (arg == "--build-box" && argc > 2) {
            std::string module_name = argv[2];
            std::string command = "make box/" + module_name + "/" + module_name + ".so";
            std::cout << "Building module: " << module_name << std::endl;
            int result = system(command.c_str());
            return result;
        }
    }

    neutron::VM vm;
    if (argc > 2) {
        std::cout << "Usage: neutron [script]" << std::endl;
        exit(1);
    } else if (argc == 2) {
        runFile(argv[1], vm);
    } else {
        runPrompt(vm);
    }
    
    return 0;
}