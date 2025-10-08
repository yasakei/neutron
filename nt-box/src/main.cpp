#include "platform.h"
#include "registry.h"
#include "installer.h"
#include "builder.h"
#include <iostream>
#include <vector>
#include <string>

using namespace box;

void printUsage() {
    std::cout << "Box - Neutron Package Manager v1.0.0\n" << std::endl;
    std::cout << "Usage: box <command> [options]\n" << std::endl;
    std::cout << "Commands:\n" << std::endl;
    std::cout << "  Installation:" << std::endl;
    std::cout << "    install <module>       Install a module from NUR" << std::endl;
    std::cout << "    uninstall <module>     Remove an installed module" << std::endl;
    std::cout << "    update <module>        Update a module to latest version" << std::endl;
    std::cout << "    list                   List installed modules" << std::endl;
    std::cout << std::endl;
    std::cout << "  Building:" << std::endl;
    std::cout << "    build native <module>  Build native module for current platform" << std::endl;
    std::cout << "    build nt <module>      Build Neutron source module (future)" << std::endl;
    std::cout << std::endl;
    std::cout << "  Information:" << std::endl;
    std::cout << "    search <query>         Search for modules in NUR" << std::endl;
    std::cout << "    info <module>          Show module information" << std::endl;
    std::cout << "    version                Show Box version" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  box install base64" << std::endl;
    std::cout << "  box search crypto" << std::endl;
    std::cout << "  box build native mymodule" << std::endl;
}

void printVersion() {
    std::cout << "Box Package Manager v1.0.0" << std::endl;
    std::cout << "Platform: " << Platform::getOSString() << std::endl;
    std::cout << "Library Extension: " << Platform::getLibraryExtension() << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "version" || command == "--version" || command == "-v") {
        printVersion();
        return 0;
    }
    
    if (command == "help" || command == "--help" || command == "-h") {
        printUsage();
        return 0;
    }
    
    if (command == "install") {
        if (argc < 3) {
            std::cerr << "Error: Module name required" << std::endl;
            std::cerr << "Usage: box install <module>" << std::endl;
            return 1;
        }
        
        std::string moduleName = argv[2];
        Installer installer;
        
        if (installer.install(moduleName, false)) {
            return 0;
        } else {
            return 1;
        }
    }
    
    if (command == "uninstall") {
        if (argc < 3) {
            std::cerr << "Error: Module name required" << std::endl;
            std::cerr << "Usage: box uninstall <module>" << std::endl;
            return 1;
        }
        
        std::string moduleName = argv[2];
        Installer installer;
        
        if (installer.uninstall(moduleName, true)) {
            return 0;
        } else {
            return 1;
        }
    }
    
    if (command == "update") {
        if (argc < 3) {
            std::cerr << "Error: Module name required" << std::endl;
            std::cerr << "Usage: box update <module>" << std::endl;
            return 1;
        }
        
        std::string moduleName = argv[2];
        Installer installer;
        
        if (installer.update(moduleName, true)) {
            return 0;
        } else {
            return 1;
        }
    }
    
    if (command == "list") {
        Installer installer;
        auto modules = installer.listInstalled(true);
        
        if (modules.empty()) {
            std::cout << "No modules installed" << std::endl;
        } else {
            std::cout << "Installed modules:" << std::endl;
            for (const auto& mod : modules) {
                std::cout << "  " << mod << std::endl;
            }
        }
        return 0;
    }
    
    if (command == "search") {
        if (argc < 3) {
            std::cerr << "Error: Search query required" << std::endl;
            std::cerr << "Usage: box search <query>" << std::endl;
            return 1;
        }
        
        std::string query = argv[2];
        Registry registry;
        
        if (!registry.fetchIndex()) {
            std::cerr << "Failed to fetch registry" << std::endl;
            return 1;
        }
        
        auto results = registry.search(query);
        
        if (results.empty()) {
            std::cout << "No modules found matching '" << query << "'" << std::endl;
        } else {
            std::cout << "Found " << results.size() << " module(s):" << std::endl;
            for (const auto& mod : results) {
                std::cout << "  " << mod << std::endl;
            }
        }
        return 0;
    }
    
    if (command == "info") {
        if (argc < 3) {
            std::cerr << "Error: Module name required" << std::endl;
            std::cerr << "Usage: box info <module>" << std::endl;
            return 1;
        }
        
        std::string moduleName = argv[2];
        Registry registry;
        
        if (!registry.fetchIndex()) {
            std::cerr << "Failed to fetch registry" << std::endl;
            return 1;
        }
        
        auto metadata = registry.fetchModuleMetadata(moduleName);
        
        if (metadata.name.empty()) {
            std::cerr << "Module not found: " << moduleName << std::endl;
            return 1;
        }
        
        std::cout << "Module: " << metadata.name << std::endl;
        if (!metadata.description.empty())
            std::cout << "Description: " << metadata.description << std::endl;
        if (!metadata.author.empty())
            std::cout << "Author: " << metadata.author << std::endl;
        if (!metadata.license.empty())
            std::cout << "License: " << metadata.license << std::endl;
        if (!metadata.repository.empty())
            std::cout << "Repository: " << metadata.repository << std::endl;
        std::cout << "Latest: " << metadata.latest << std::endl;
        
        std::cout << "\nAvailable Versions:" << std::endl;
        for (const auto& versionPair : metadata.versions) {
            std::cout << "  " << versionPair.first;
            if (versionPair.first == metadata.latest) {
                std::cout << " (latest)";
            }
            std::cout << std::endl;
            if (!versionPair.second.description.empty()) {
                std::cout << "    " << versionPair.second.description << std::endl;
            }
        }
        
        return 0;
    }
    
    if (command == "build") {
        if (argc < 4) {
            std::cerr << "Error: Build type and module name required" << std::endl;
            std::cerr << "Usage: box build <native|nt> <module> [version]" << std::endl;
            return 1;
        }
        
        std::string buildType = argv[2];
        std::string moduleName = argv[3];
        std::string version = (argc >= 5) ? argv[4] : "1.0.0";
        
        Builder builder;
        
        if (buildType == "native") {
            std::string sourcePath = "./" + moduleName;
            std::string outputDir = "./box-modules";
            
            if (builder.buildNative(moduleName, sourcePath, outputDir, version)) {
                std::cout << "✓ Successfully built " << moduleName << " v" << version << std::endl;
                return 0;
            } else {
                std::cerr << "Failed to build " << moduleName << std::endl;
                return 1;
            }
        } else if (buildType == "nt") {
            std::cerr << "Neutron source builds not yet implemented" << std::endl;
            return 1;
        } else {
            std::cerr << "Unknown build type: " << buildType << std::endl;
            std::cerr << "Valid types: native, nt" << std::endl;
            return 1;
        }
    }
    
    std::cerr << "Unknown command: " << command << std::endl;
    std::cerr << "Run 'box help' for usage information" << std::endl;
    return 1;
}
