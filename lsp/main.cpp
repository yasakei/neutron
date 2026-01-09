// Force JsonCpp to NOT use string_view
#define JSONCPP_HAS_STRING_VIEW 0
#define JSON_HAS_STRING_VIEW 0

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
