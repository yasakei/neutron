#include <iostream>
#include <string>

int main() {
    std::string source = "var a = \"Hello\";";
    int start = 8;   // Position of first quote
    int current = 15; // Position after second quote
    
    // This is what our scanner does:
    std::string value = source.substr(start + 1, current - start - 2);
    std::cout << "Extracted value: '" << value << "'" << std::endl;
    
    return 0;
}