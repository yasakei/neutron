#include "installer.h"
#include "platform.h"
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define mkdir(path, mode) _mkdir(path)
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <pwd.h>
#endif

namespace box {

Installer::Installer() {
    std::string homeDir;
#ifdef _WIN32
    char* userProfile = getenv("USERPROFILE");
    if (userProfile) homeDir = userProfile;
#else
    char* home = getenv("HOME");
    if (home) {
        homeDir = home;
    } else {
        struct passwd* pw = getpwuid(getuid());
        if (pw) homeDir = pw->pw_dir;
    }
#endif
    
    globalModulesDir = homeDir + "/.box/modules";
    localModulesDir = "./.box/modules";
}

std::string Installer::getInstallDir(bool global) {
    return global ? globalModulesDir : localModulesDir;
}

bool Installer::ensureDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) return true;
    
#ifdef _WIN32
    std::string command = "mkdir \"" + path + "\" 2>nul";
    return system(command.c_str()) == 0 || stat(path.c_str(), &st) == 0;
#else
    std::string command = "mkdir -p \"" + path + "\"";
    return system(command.c_str()) == 0;
#endif
}

bool Installer::install(const std::string& moduleSpec, bool global) {
    std::string moduleName = moduleSpec;
    std::string requestedVersion = "";
    
    size_t atPos = moduleSpec.find('@');
    if (atPos != std::string::npos) {
        moduleName = moduleSpec.substr(0, atPos);
        requestedVersion = moduleSpec.substr(atPos + 1);
    }
    
    std::cout << "Installing " << moduleName;
    if (!requestedVersion.empty()) std::cout << "@" << requestedVersion;
    std::cout << "..." << std::endl;
    
    if (!registry.fetchIndex()) {
        std::cerr << "Failed to fetch registry index" << std::endl;
        return false;
    }
    
    ModuleMetadata metadata = registry.fetchModuleMetadata(moduleName);
    if (metadata.name.empty()) {
        std::cerr << "Module not found: " << moduleName << std::endl;
        return false;
    }
    
    std::string versionToInstall = requestedVersion.empty() ? metadata.latest : requestedVersion;
    
    if (metadata.versions.find(versionToInstall) == metadata.versions.end()) {
        std::cerr << "Version not found: " << versionToInstall << std::endl;
        return false;
    }
    
    VersionMetadata versionMeta = metadata.versions[versionToInstall];
    
    std::string binaryURL;
    if (Platform::isLinux()) {
        binaryURL = versionMeta.entryLinux;
    } else if (Platform::isWindows()) {
        binaryURL = versionMeta.entryWin;
    } else if (Platform::isMacOS()) {
        binaryURL = versionMeta.entryMac;
    }
    
    if (binaryURL.empty()) {
        std::cerr << "No binary available for " << Platform::getOSString() << std::endl;
        return false;
    }
    
    std::string installDir = getInstallDir(global) + "/" + moduleName;
    if (!ensureDirectory(installDir)) {
        std::cerr << "Failed to create directory: " << installDir << std::endl;
        return false;
    }
    
    std::cout << "Downloading from " << binaryURL << "..." << std::endl;
    std::string binaryData = registry.download(binaryURL);
    if (binaryData.empty()) {
        std::cerr << "Failed to download module" << std::endl;
        return false;
    }
    
    std::string outputFile = installDir + "/" + moduleName + Platform::getLibraryExtension();
    std::ofstream outFile(outputFile, std::ios::binary);
    if (!outFile) {
        std::cerr << "Failed to create file: " << outputFile << std::endl;
        return false;
    }
    outFile.write(binaryData.data(), binaryData.size());
    outFile.close();
    
#ifndef _WIN32
    chmod(outputFile.c_str(), 0755);
#endif
    
    std::string metadataFile = installDir + "/metadata.json";
    std::ofstream metaFile(metadataFile);
    if (metaFile) {
        metaFile << "{\n";
        metaFile << "  \"name\": \"" << moduleName << "\",\n";
        metaFile << "  \"version\": \"" << versionToInstall << "\",\n";
        metaFile << "  \"description\": \"" << versionMeta.description << "\",\n";
        metaFile << "  \"platform\": \"" << Platform::getOSString() << "\",\n";
        metaFile << "  \"library\": \"" << moduleName << Platform::getLibraryExtension() << "\"\n";
        metaFile << "}\n";
        metaFile.close();
    }
    
    std::cout << "✓ Installed " << moduleName << "@" << versionToInstall << " to " << installDir << std::endl;
    return true;
}

bool Installer::uninstall(const std::string& moduleName, bool global) {
    std::string installDir = getInstallDir(global);
    std::string moduleDir = installDir + "/" + moduleName;
    
    struct stat st;
    if (stat(moduleDir.c_str(), &st) != 0) {
        std::cerr << "Module not installed: " << moduleName << std::endl;
        return false;
    }
    
    std::cout << "Uninstalling " << moduleName << "..." << std::endl;
    
#ifdef _WIN32
    std::string command = "rmdir /s /q \"" + moduleDir + "\"";
#else
    std::string command = "rm -rf \"" + moduleDir + "\"";
#endif
    
    if (system(command.c_str()) == 0) {
        std::cout << "✓ Successfully uninstalled " << moduleName << std::endl;
        return true;
    }
    
    std::cerr << "Failed to uninstall " << moduleName << std::endl;
    return false;
}

bool Installer::update(const std::string& moduleName, bool global) {
    std::cout << "Updating " << moduleName << "..." << std::endl;
    if (isInstalled(moduleName, global)) {
        uninstall(moduleName, global);
    }
    return install(moduleName, global);
}

std::vector<std::string> Installer::listInstalled(bool global) {
    std::vector<std::string> modules;
    return modules;
}

bool Installer::isInstalled(const std::string& moduleName, bool global) {
    std::string installDir = getInstallDir(global);
    std::string moduleDir = installDir + "/" + moduleName;
    struct stat st;
    return stat(moduleDir.c_str(), &st) == 0;
}

} // namespace box
