#include "server.h"
#include <iostream>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    // Add flags if needed
    
    neutron::lsp::LSPServer server;
    server.run();
    
    return 0;
}
