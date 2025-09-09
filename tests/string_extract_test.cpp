#include <iostream>
#include <string>

int main() {
    // Simulate what our scanner does
    std::string source = "print(\"Hello\");";
    int start = 6;   // Position of first quote
    int current = 13; // Position after second quote
    
    std::cout << "Source: " << source << std::endl;
    std::cout << "start: " << start << std::endl;
    std::cout << "current: " << current << std::endl;
    
    // This is what our scanner does:
    std::string value = source.substr(start + 1, current - start - 2);
    std::cout << "Extracted value: '" << value << "'" << std::endl;
    
    // Let's check each character
    for (int i = start; i <= current; i++) {
        if (i < static_cast<int>(source.length())) {
            std::cout << "source[" << i << "] = '" << source[i] << "'" << std::endl;
        } else {
            std::cout << "source[" << i << "] = (out of bounds)" << std::endl;
        }
    }
    
    return 0;
}