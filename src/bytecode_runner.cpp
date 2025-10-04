#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include "compiler/scanner.h"
#include "compiler/parser.h"
#include "vm.h"
#include "compiler/compiler.h"
#include "compiler/bytecode.h"

// This array will be populated with bytecode data during compilation
extern "C" {
    extern const unsigned char bytecode_data[];
    extern const size_t bytecode_size;
    extern const unsigned char constants_data[];
    extern const size_t constants_size;
}

int main() {
    // Initialize the VM
    neutron::VM vm;
    
    // Create a chunk with the embedded bytecode
    neutron::Chunk* chunk = new neutron::Chunk();
    
    // Load bytecode from embedded data
    for (size_t i = 0; i < bytecode_size; i++) {
        chunk->write(bytecode_data[i], 0);
    }
    
    // Create a function with the loaded chunk
    neutron::Function* function = new neutron::Function(nullptr, nullptr);
    function->chunk = chunk;
    
    // Execute the function
    vm.interpret(function);
    
    return 0;
}