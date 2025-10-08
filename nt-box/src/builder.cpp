#include "builder.h"
#include "platform.h"
#include <iostream>
#include <cstdlib>
#include <fstream>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

namespace box {

Builder::Builder() {
}

std::string Builder::getCompiler() {
    if (Platform::isWindows()) {
        // Check if we're in MSYS2/MINGW64 environment
        const char* msystem = getenv("MSYSTEM");
        if (msystem && (std::string(msystem).find("MINGW") != std::string::npos || 
                       std::string(msystem).find("MSYS") != std::string::npos)) {
            // Use g++ in MSYS2/MINGW64
            return "g++";
        }
        // Otherwise use MSVC
        return "cl";
    } else {
        // Check for clang++ or g++
        if (system("which clang++ > /dev/null 2>&1") == 0) {
            return "clang++";
        } else {
            return "g++";
        }
    }
}

std::vector<std::string> Builder::getLinkerFlags() {
    std::vector<std::string> flags;
    
    if (Platform::isWindows()) {
        // Check if we're using MINGW64/MSYS2
        const char* msystem = getenv("MSYSTEM");
        if (msystem && (std::string(msystem).find("MINGW") != std::string::npos || 
                       std::string(msystem).find("MSYS") != std::string::npos)) {
            // MINGW64/MSYS2 uses GCC flags
            flags.push_back("-shared");
            flags.push_back("-fPIC");
        } else {
            // MSVC flags
            flags.push_back("/LD");  // Create DLL
            flags.push_back("/MD");  // Multithreaded DLL runtime
        }
    } else if (Platform::isMacOS()) {
        flags.push_back("-shared");
        flags.push_back("-fPIC");
        flags.push_back("-dynamiclib");
    } else {  // Linux
        flags.push_back("-shared");
        flags.push_back("-fPIC");
    }
    
    return flags;
}

std::vector<std::string> Builder::getIncludePaths() {
    std::vector<std::string> paths;
    
    // Try to find Neutron installation
    std::string neutronDir = findNeutronDir();
    
    if (!neutronDir.empty()) {
        paths.push_back(neutronDir + "/include");
    }
    
    // Add standard locations
    paths.push_back("../include");
    paths.push_back("../../include");
    
    return paths;
}

std::string Builder::findNeutronDir() {
    // Check environment variable
    char* neutronHome = getenv("NEUTRON_HOME");
    if (neutronHome) {
        return std::string(neutronHome);
    }
    
    // Check standard installation locations
    std::vector<std::string> candidates;
    
#ifdef _WIN32
    candidates.push_back("C:\\Program Files\\Neutron");
    candidates.push_back("C:\\Neutron");
    
    // MSYS2/MINGW64 paths
    const char* msystem = getenv("MSYSTEM");
    if (msystem) {
        candidates.push_back("/mingw64/neutron");
        candidates.push_back("/usr/local/neutron");
        candidates.push_back("/opt/neutron");
    }
#else
    candidates.push_back("/usr/local/neutron");
    candidates.push_back("/opt/neutron");
    char* home = getenv("HOME");
    if (home) {
        candidates.push_back(std::string(home) + "/.neutron");
    }
#endif
    
    // Check current directory and parent
    candidates.push_back(".");
    candidates.push_back("..");
    
    for (const auto& dir : candidates) {
        std::string testPath = dir + "/include/neutron.h";
        std::ifstream test(testPath);
        if (test.good()) {
            return dir;
        }
    }
    
    return "";
}

std::string Builder::generateBuildCommand(const std::string& moduleName,
                                         const std::string& sourcePath,
                                         const std::string& outputPath) {
    std::string compiler = getCompiler();
    std::vector<std::string> linkerFlags = getLinkerFlags();
    std::vector<std::string> includePaths = getIncludePaths();
    
    std::string command;
    
    // Check if we're using MSVC or MINGW64
    bool isMSVC = (Platform::isWindows() && compiler == "cl");
    
    if (isMSVC) {
        // MSVC command
        command = compiler + " ";
        command += "/std:c++17 ";
        
        // Add include paths
        for (const auto& path : includePaths) {
            command += "/I\"" + path + "\" ";
        }
        
        // Source file
        command += "\"" + sourcePath + "/native.cpp\" ";
        
        // Linker flags
        command += "/LD /MD ";
        
        // Output
        command += "/Fe:\"" + outputPath + "\" ";
        
        // Link against neutron runtime
        std::string neutronDir = findNeutronDir();
        if (!neutronDir.empty()) {
            command += "/link /LIBPATH:\"" + neutronDir + "/build\" neutron_runtime.lib ";
        }
    } else {
        // GCC/Clang command (Linux, macOS, MINGW64)
        command = compiler + " ";
        command += "-std=c++17 ";
        command += "-fPIC -shared ";
        
        // Add include paths
        for (const auto& path : includePaths) {
            command += "-I\"" + path + "\" ";
        }
        
        // Source file
        command += "\"" + sourcePath + "/native.cpp\" ";
        
        // Output
        command += "-o \"" + outputPath + "\" ";
        
        // Link against neutron runtime if found
        std::string neutronDir = findNeutronDir();
        if (!neutronDir.empty()) {
            command += "-L\"" + neutronDir + "/build\" ";
            // Don't use -rpath on Windows/MINGW64
            if (!Platform::isWindows()) {
                command += "-Wl,-rpath,\"" + neutronDir + "/build\" ";
            }
            // Link against the neutron runtime library
            command += "-lneutron_runtime ";
        }
    }
    
    return command;
}

bool Builder::executeCommand(const std::string& command) {
    std::cout << "Executing: " << command << std::endl;
    int result = system(command.c_str());
    return result == 0;
}

bool Builder::buildNative(const std::string& moduleName,
                         const std::string& sourcePath,
                         const std::string& outputDir,
                         const std::string& version) {
    std::cout << "Building native module: " << moduleName << std::endl;
    std::cout << "Version: " << version << std::endl;
    std::cout << "Platform: " << Platform::getOSString() << std::endl;
    
    // Check if source exists
    std::string nativeCpp = sourcePath + "/native.cpp";
    std::ifstream sourceFile(nativeCpp);
    if (!sourceFile.good()) {
        std::cerr << "Error: Source file not found: " << nativeCpp << std::endl;
        return false;
    }
    
    // Extract just the module name (remove path if present)
    std::string baseModuleName = moduleName;
    size_t lastSlash = moduleName.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        baseModuleName = moduleName.substr(lastSlash + 1);
    }
    
    // Create output directory: box/{module_name}/
    std::string moduleOutputDir = outputDir + "/" + baseModuleName;
    std::string mkdirCmd;
#ifdef _WIN32
    mkdirCmd = "mkdir \"" + moduleOutputDir + "\" 2>nul";
#else
    mkdirCmd = "mkdir -p \"" + moduleOutputDir + "\"";
#endif
    system(mkdirCmd.c_str());
    
    // Generate output path: box/{module_name}/{module_name}.so
    std::string outputPath = moduleOutputDir + "/" + baseModuleName + Platform::getLibraryExtension();
    
    // Generate and execute build command
    std::string buildCommand = generateBuildCommand(baseModuleName, sourcePath, outputPath);
    
    if (executeCommand(buildCommand)) {
        std::cout << "✓ Built: " << outputPath << std::endl;
        
        // Create metadata.json
        if (createMetadata(baseModuleName, version, moduleOutputDir)) {
            std::cout << "✓ Created: " << moduleOutputDir << "/metadata.json" << std::endl;
        }
        
        return true;
    } else {
        std::cerr << "✗ Build failed" << std::endl;
        return false;
    }
}

bool Builder::createMetadata(const std::string& moduleName,
                            const std::string& version,
                            const std::string& outputDir) {
    std::string metadataPath = outputDir + "/metadata.json";
    std::ofstream metadataFile(metadataPath);
    
    if (!metadataFile.is_open()) {
        std::cerr << "Warning: Could not create metadata.json" << std::endl;
        return false;
    }
    
    // Write JSON metadata
    metadataFile << "{\n";
    metadataFile << "  \"name\": \"" << moduleName << "\",\n";
    metadataFile << "  \"version\": \"" << version << "\",\n";
    metadataFile << "  \"description\": \"" << moduleName << " native module for Neutron\",\n";
    metadataFile << "  \"platform\": \"" << Platform::getOSString() << "\",\n";
    metadataFile << "  \"library\": \"" << moduleName << Platform::getLibraryExtension() << "\"\n";
    metadataFile << "}\n";
    
    metadataFile.close();
    return true;
}

bool Builder::buildNeutron(const std::string& moduleName,
                          const std::string& sourcePath,
                          const std::string& outputDir) {
    std::cerr << "Neutron source module builds not yet implemented" << std::endl;
    return false;
}

} // namespace box
