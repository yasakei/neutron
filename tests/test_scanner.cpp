#include <iostream>
#include "scanner.h"

int main() {
    std::string source = R"(
        var name = "Neutron";
        print("Hello, " + name);
    )";
    
    neutron::Scanner scanner(source);
    std::vector<neutron::Token> tokens = scanner.scanTokens();
    
    std::cout << "Scanned tokens:" << std::endl;
    for (const auto& token : tokens) {
        std::cout << token << std::endl;
    }
    
    return 0;
}