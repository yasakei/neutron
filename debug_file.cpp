#include <iostream>
#include <fstream>
#include <sstream>

int main() {
    std::ifstream file("lib/convert.nt");
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();
        std::cout << "File content length: " << source.length() << std::endl;
        std::cout << "First 100 characters: ";
        for (int i = 0; i < 100 && i < source.length(); i++) {
            std::cout << (int)source[i] << " ";
        }
        std::cout << std::endl;
    } else {
        std::cout << "Could not open file" << std::endl;
    }
    return 0;
}