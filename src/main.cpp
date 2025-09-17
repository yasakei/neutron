#include <iostream>
#include <fstream>
#include <string>
#include "scanner.h"
#include "parser.h"
#include "vm.h"
#include "compiler.h"

void run(const std::string& source, neutron::VM& vm) {
    std::cout << "run" << std::endl;
    neutron::Scanner scanner(source);
    std::vector<neutron::Token> tokens = scanner.scanTokens();
    
    neutron::Parser parser(tokens);
    std::vector<std::unique_ptr<neutron::Stmt>> statements = parser.parse();
    
    neutron::Compiler compiler(vm);
    neutron::Function* function = compiler.compile(statements);
    
    vm.interpret(function);
}

void runFile(const std::string& path) {
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
    
    neutron::VM vm;
    run(source, vm);
}

void runPrompt() {
    std::string line;
    std::cout << "Neutron REPL (Press Ctrl+C to exit)" << std::endl;
    
    neutron::VM vm;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        run(line, vm);
    }
}

int main(int argc, char* argv[]) {
    std::cout << "main" << std::endl;
    if (argc > 2) {
        std::cout << "Usage: neutron [script]" << std::endl;
        exit(1);
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        runPrompt();
    }
    
    return 0;
}
