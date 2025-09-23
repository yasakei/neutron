// Auto-generated executable for programs/use_math.nt
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include "scanner.h"
#include "parser.h"
#include "vm.h"
#include "compiler.h"

const char* math_source = R"neutron_source(
// n.math.nt - Math library for Neutron

)neutron_source";

const char* embedded_source = R"neutron_source(
use math;

say(math.sqrt(16));

)neutron_source";

int main() {
    // Initialize the VM
    neutron::VM vm;

    {
        neutron::Scanner scanner(math_source);
        std::vector<neutron::Token> tokens = scanner.scanTokens();
        neutron::Parser parser(tokens);
        std::vector<std::unique_ptr<neutron::Stmt>> statements = parser.parse();
        neutron::Compiler compiler(vm);
        neutron::Function* function = compiler.compile(statements);
        vm.interpret(function);
    }
    // Run the embedded source code
    neutron::Scanner scanner(embedded_source);
    std::vector<neutron::Token> tokens = scanner.scanTokens();

    neutron::Parser parser(tokens);
    std::vector<std::unique_ptr<neutron::Stmt>> statements = parser.parse();

    neutron::Compiler compiler(vm);
    neutron::Function* function = compiler.compile(statements);

    vm.interpret(function);

    return 0;
}
