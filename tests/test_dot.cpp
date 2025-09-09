#include <iostream>
#include "scanner.h"

int main() {
    std::string source = "var result = n.math.add(x, y);";
    
    neutron::Scanner scanner(source);
    std::vector<neutron::Token> tokens = scanner.scanTokens();
    
    std::cout << "Scanned tokens:" << std::endl;
    for (const auto& token : tokens) {
        std::cout << "Token type: " << static_cast<int>(token.type) 
                  << ", lexeme: '" << token.lexeme 
                  << "', line: " << token.line << std::endl;
    }
    
    return 0;
}