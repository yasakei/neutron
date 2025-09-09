#include <iostream>
#include "scanner.h"

int main() {
    std::string source = "var a = \"Hello\";";
    
    std::cout << "Source: " << source << std::endl;
    std::cout << "Length: " << source.length() << std::endl;
    
    // Show each character with its position
    for (size_t i = 0; i < source.length(); i++) {
        std::cout << "Pos " << i << ": '" << source[i] << "'" << std::endl;
    }
    
    neutron::Scanner scanner(source);
    std::vector<neutron::Token> tokens = scanner.scanTokens();
    
    std::cout << "\nScanned tokens:" << std::endl;
    for (const auto& token : tokens) {
        std::cout << "Token type: " << static_cast<int>(token.type) 
                  << ", lexeme: '" << token.lexeme << "'" 
                  << ", line: " << token.line << std::endl;
    }
    
    return 0;
}