/*
 * Neutron Programming Language - Project Builder Implementation
 * Copyright (c) 2026 yasakei
 */

#include "project/project_builder.h"
#include "compiler/scanner.h"
#include "compiler/parser.h"
#include "compiler/compiler.h"
#include "aot/aot_compiler.h"
#include "core/vm.h"
#include "types/value.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <regex>
#include <set>

// Forward declare getUsedModules from module_utils
namespace neutron {
    std::vector<std::string> getUsedModules(const std::string& source);
}

namespace neutron {

bool ProjectBuilder::fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

std::string ProjectBuilder::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return content;
}

std::vector<std::string> ProjectBuilder::findProjectModules(const std::string& projectRoot) {
    std::vector<std::string> modules;
    std::string boxModulesDir = projectRoot + "/.box/modules";
    
    if (!std::filesystem::exists(boxModulesDir)) {
        return modules;
    }
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(boxModulesDir)) {
            if (entry.is_directory()) {
                modules.push_back(entry.path().filename().string());
            }
        }
    } catch (...) {
        // Ignore errors
    }
    
    return modules;
}

std::vector<std::string> ProjectBuilder::findBoxModules(const std::string& projectRoot) {
    std::vector<std::string> nativeModules;
    std::string boxModulesDir = projectRoot + "/.box/modules";
    
    if (!std::filesystem::exists(boxModulesDir)) {
        return nativeModules;
    }
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(boxModulesDir)) {
            if (entry.is_directory()) {
                std::string moduleName = entry.path().filename().string();
                
                // Look for shared library files
                std::string libSo = entry.path().string() + "/lib" + moduleName + ".so";      // Linux
                std::string libSoNoPrefix = entry.path().string() + "/" + moduleName + ".so"; // Linux (no prefix)
                std::string libDylib = entry.path().string() + "/lib" + moduleName + ".dylib"; // macOS
                std::string libDylibNoPrefix = entry.path().string() + "/" + moduleName + ".dylib"; // macOS (no prefix)
                std::string libDll = entry.path().string() + "/" + moduleName + ".dll";        // Windows
                
                if (fileExists(libSo) || fileExists(libSoNoPrefix) || 
                    fileExists(libDylib) || fileExists(libDylibNoPrefix) || 
                    fileExists(libDll)) {
                    nativeModules.push_back(moduleName);
                }
            }
        }
    } catch (...) {
        // Ignore errors
    }
    
    return nativeModules;
}

std::vector<std::string> ProjectBuilder::getRuntimeSources() {
    return {
        // Core system sources
        "src/core/vm.cpp",
        "src/core/checkpoint.cpp", 
        "src/core/capi.cpp",
        "src/core/formatter.cpp",
        "src/token.cpp",
        
        // Compiler sources
        "src/compiler/scanner.cpp",
        "src/compiler/parser.cpp",
        "src/compiler/compiler.cpp",
        "src/compiler/bytecode.cpp",
        
        // Type system sources
        "src/types/value.cpp",
        "src/types/array.cpp",
        "src/types/buffer.cpp",
        "src/types/json_object.cpp",
        "src/types/json_array.cpp",
        "src/types/return.cpp",
        "src/types/version.cpp",
        "src/types/obj_string.cpp",
        "src/types/string_method_registry.cpp",
        "src/types/unicode_handler.cpp",
        "src/types/string_formatter.cpp",
        "src/types/string_error.cpp",
        "src/types/string_search_methods.cpp",
        
        // Runtime sources
        "src/runtime/environment.cpp",
        "src/runtime/error_handler.cpp",
        "src/runtime/runtime.cpp",
        "src/runtime/debug.cpp",
        "src/runtime/process.cpp",
        
        // Module system sources
        "src/modules/module.cpp",
        "src/modules/module_loader.cpp",
        "src/modules/module_registry.cpp",
        "src/modules/module_utils.cpp",
        
        // Project system sources
        "src/project/project_config.cpp",
        "src/project/project_manager.cpp",
        "src/project/project_builder.cpp",
        
        // Platform sources
        "src/platform/platform.cpp",
        "src/utils/component_interface.cpp"
    };
}

std::vector<std::string> ProjectBuilder::getBuiltinModuleSources() {
    return {
        "libs/sys/native.cpp",
        "libs/fmt/native.cpp",
        "libs/json/native.cpp",
        "libs/math/native.cpp",
        "libs/http/native.cpp",
        "libs/time/native.cpp",
        "libs/arrays/native.cpp",
        "libs/async/native.cpp",
        "libs/regex/native.cpp",
        "libs/process/native.cpp",
        "libs/crypto/native.cpp",
        "libs/path/native.cpp"
    };
}

bool ProjectBuilder::buildProjectExecutable(
    const std::string& projectRoot,
    const std::string& sourceCode,
    const std::string& /*sourcePath*/,
    const std::string& outputPath,
    const std::string& neutronExecutablePath,
    bool bundleLibs,
    bool aotCompile,
    const std::string& targetArch
) {
    std::cout << "\n[1/4] Preparing build..." << std::endl;

    // Platform detection
    bool isWindows = false;
    bool isMacOS = false;
    bool isMingw = false;
    bool isARM = false;
    bool isX64 = false;

    // If cross-compilation target specified, override auto-detection
    bool crossCompile = !targetArch.empty();
    std::string crossCompiler;
    std::string crossSysroot;
    
    if (crossCompile) {
        // Determine target platform and compiler from target triple
        if (targetArch.find("aarch64") != std::string::npos || targetArch.find("arm64") != std::string::npos || targetArch.find("armv8") != std::string::npos) {
            isARM = true;
            isX64 = false;
        } else if (targetArch.find("x86_64") != std::string::npos || targetArch.find("x64") != std::string::npos) {
            isARM = false;
            isX64 = true;
        }
        
        // Find cross-compiler
        std::vector<std::string> compilerCandidates = {
            targetArch + "-g++",
            targetArch + "-gcc"
        };
        
        for (const auto& cc : compilerCandidates) {
            std::string checkCmd = "which " + cc + " > /dev/null 2>&1";
            if (system(checkCmd.c_str()) == 0) {
                crossCompiler = cc;
                std::cout << "      Cross-compiler: " << crossCompiler << std::endl;
                break;
            }
        }
        
        if (crossCompiler.empty()) {
            std::cerr << "Warning: Cross-compiler not found for " << targetArch << std::endl;
            std::cerr << "         Install: sudo apt install " << targetArch << "-gcc" << std::endl;
        }
        
        // Check for sysroot
        std::string sysrootPath = "/usr/" + targetArch;
        if (targetArch.find("aarch64") != std::string::npos) {
            sysrootPath = "/usr/aarch64-linux-gnu";
        }
        if (std::filesystem::exists(sysrootPath)) {
            crossSysroot = sysrootPath;
            std::cout << "      Sysroot: " << crossSysroot << std::endl;
        }
    }

#ifdef _WIN32
    isWindows = true;
    #ifdef __MINGW32__
    isMingw = true;
    #endif
    #ifdef _M_ARM64
    isARM = true;
    #else
    isX64 = true;
    #endif
#elif __APPLE__
    isMacOS = true;
    #if defined(__arm64__) || defined(__aarch64__)
    isARM = true;
    #else
    isX64 = true;
    #endif
#elif defined(__aarch64__) || defined(__arm64__) || defined(__ARM_ARCH_ISA_A64)
    isARM = true;
#else
    isX64 = true;
#endif

    std::string archName = isARM ? "ARM64" : "x86-64";
    std::cout << "      Target architecture: " << archName << std::endl;

    std::string executableDir = std::filesystem::path(neutronExecutablePath).parent_path().string();
    if (executableDir.empty()) {
        executableDir = ".";
    }
    
    // Adjust output path extension
    std::string finalOutputPath = outputPath;
    if (isWindows && finalOutputPath.find(".exe") == std::string::npos) {
        finalOutputPath += ".exe";
    }
    
    // Create source file
    std::string tempSourcePath = finalOutputPath + "_main.cpp";
    std::ofstream srcFile(tempSourcePath);
    
    if (!srcFile.is_open()) {
        std::cerr << "Error: Could not create temporary source file" << std::endl;
        return false;
    }

    // Write includes
    srcFile << "// Auto-generated executable for project\n";
    srcFile << "#include <iostream>\n";
    srcFile << "#include <string>\n";
    srcFile << "#include <vector>\n";
    srcFile << "#include <cstdint>\n";
    srcFile << "#include <cmath>\n\n";

    // Find box modules
    auto boxModules = findBoxModules(projectRoot);

    // AOT compilation: only for pure Neutron code (no external/native modules)
    // OR when all external modules have static libraries (.a)
    bool aotSuccess = false;
    if (aotCompile) {
        std::cout << "[2/4] Analyzing dependencies..." << std::endl;
        
        std::vector<std::string> aotUsedModules = getUsedModules(sourceCode);
        auto boxModules = findBoxModules(projectRoot);

        // Modules that require interpreter (native code or external dependencies)
        // Note: math, random, fmt, path have AOT-compatible implementations in aot_module.h
        std::set<std::string> nonAotModules = {
            "http", "json", "sys", "time", "crypto", "process",
            "arrays", "async", "regex"
        };
        
        bool canAot = true;
        bool hasStaticLibs = true;
        for (const auto& mod : aotUsedModules) {
            // Check if it's a built-in native module
            if (nonAotModules.count(mod) > 0) {
                canAot = false;
                std::cout << "      Module '" << mod << "' requires interpreter" << std::endl;
            }
            // Check if it's an external box module
            else if (std::find(boxModules.begin(), boxModules.end(), mod) != boxModules.end()) {
                // Check if static library exists
                std::string libA = projectRoot + "/.box/modules/" + mod + "/lib" + mod + ".a";
                if (!fileExists(libA)) {
                    hasStaticLibs = false;
                    std::cout << "      Module '" << mod << "' requires interpreter (no static library)" << std::endl;
                }
            }
        }
        
        // Can use AOT if no non-AOT modules AND all box modules have static libs
        canAot = canAot && hasStaticLibs;
        
        if (canAot && aotUsedModules.empty()) {
            std::cout << "      No external dependencies found" << std::endl;
        }
        
        if (canAot) {
            std::cout << "      Compiling to native code..." << std::endl;
            
            neutron::VM aotVm;
            neutron::Scanner aotScanner(sourceCode);
            auto aotTokens = aotScanner.scanTokens();
            neutron::Parser aotParser(aotTokens);
            auto aotStatements = aotParser.parse();
            neutron::Compiler aotCompiler(aotVm);
            neutron::Function* mainFunc = aotCompiler.compile(aotStatements);
            
            if (mainFunc && mainFunc->chunk) {
                std::cout << "      Generated " << mainFunc->chunk->code.size() << " native instructions" << std::endl;
                
                srcFile << "// Auto-generated native code - no VM needed\n\n";
                neutron::aot::AotCompiler aotGen(mainFunc->chunk);
                srcFile << aotGen.generateCode("neutron_main");
                srcFile << "\nint main() { return neutron_main(); }\n";
                srcFile.close();
                aotSuccess = true;
            }
        }
    }
    
    // If AOT didn't succeed, embed source for interpreter mode
    if (!aotSuccess) {
        std::cout << "[2/4] Embedding source files..." << std::endl;
        std::vector<std::string> usedModules = getUsedModules(sourceCode);
        if (!usedModules.empty()) {
            std::cout << "      Found " << usedModules.size() << " module(s) to embed" << std::endl;
        }
    
    // Embed .nt module sources from project
    for (const auto& moduleName : usedModules) {
        // Check project .box/modules
        std::string boxModulePath = projectRoot + "/.box/modules/" + moduleName + "/" + moduleName + ".nt";
        if (fileExists(boxModulePath)) {
            std::string moduleSource = readFile(boxModulePath);
            srcFile << "const char* " << moduleName << "_source = R\"neutron_source(\n";
            srcFile << moduleSource;
            srcFile << "\n)neutron_source\";\n\n";
        }
        // Check standard lib
        else {
            std::string libPath = executableDir + "/../lib/" + moduleName + ".nt";
            if (!fileExists(libPath)) {
                // Try alternative path
                libPath = "lib/" + moduleName + ".nt";
            }
            if (fileExists(libPath)) {
                std::string moduleSource = readFile(libPath);
                srcFile << "const char* " << moduleName << "_source = R\"neutron_source(\n";
                srcFile << moduleSource;
                srcFile << "\n)neutron_source\";\n\n";
            }
        }
    }
    
    // Embed main source
    srcFile << "const char* embedded_source = R\"neutron_source(\n";
    srcFile << sourceCode;
    srcFile << "\n)neutron_source\";\n\n";

    // Embed native modules if bundling is enabled
    std::vector<std::string> bundledModules;

    if (bundleLibs && !boxModules.empty()) {
        std::cout << "      Bundling " << boxModules.size() << " native module(s): ";
        for (size_t i = 0; i < boxModules.size(); i++) {
            std::cout << boxModules[i];
            if (i < boxModules.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        
        srcFile << "#include <filesystem>\n";
        srcFile << "#include <fstream>\n";
        srcFile << "#include <cstdlib>\n\n";

        for (const auto& moduleName : boxModules) {
            std::string moduleDir = projectRoot + "/.box/modules/" + moduleName;
            std::string libFile;
            std::string libSo = moduleDir + "/lib" + moduleName + ".so";
            std::string libSoNoPrefix = moduleDir + "/" + moduleName + ".so";
            std::string libDylib = moduleDir + "/lib" + moduleName + ".dylib";
            std::string libDylibNoPrefix = moduleDir + "/" + moduleName + ".dylib";
            std::string libDll = moduleDir + "/" + moduleName + ".dll";
            std::string libA = moduleDir + "/lib" + moduleName + ".a";  // Static library for AOT

            // Prefer static library for AOT builds
            if (aotCompile && fileExists(libA)) {
                libFile = libA;
                std::cout << "        - " << moduleName << " (static)" << std::endl;
            } else if (fileExists(libSo)) libFile = libSo;
            else if (fileExists(libSoNoPrefix)) libFile = libSoNoPrefix;
            else if (fileExists(libDylib)) libFile = libDylib;
            else if (fileExists(libDylibNoPrefix)) libFile = libDylibNoPrefix;
            else if (fileExists(libDll)) libFile = libDll;

            if (!libFile.empty()) {
                std::ifstream binFile(libFile, std::ios::binary);
                if (binFile) {
                    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(binFile), {});

                    srcFile << "// Embedded native module: " << moduleName << "\n";
                    srcFile << "const unsigned char lib_" << moduleName << "_data[] = {";

                    for (size_t i = 0; i < buffer.size(); ++i) {
                        if (i % 16 == 0) srcFile << "\n    ";
                        srcFile << "0x" << std::hex << (int)buffer[i] << ", ";
                    }

                    srcFile << "\n};\n";
                    srcFile << "const size_t lib_" << moduleName << "_size = " << std::dec << buffer.size() << ";\n\n";
                    bundledModules.push_back(moduleName);
                }
            }
        }
    }
    
    // Helper lambda to find imported files
    auto findImports = [](const std::string& src) -> std::vector<std::string> {
        std::vector<std::string> imports;
        std::regex importRegex("using\\s+\"([^\"]+)\"\\s*;");
        std::sregex_iterator next(src.begin(), src.end(), importRegex);
        std::sregex_iterator end;
        while (next != end) {
            std::smatch match = *next;
            imports.push_back(match[1].str());
            next++;
        }
        return imports;
    };

    // Recursively find and embed imported files
    std::set<std::string> processedFiles;
    std::vector<std::string> processingQueue;
    
    // Initial scan of main source
    auto initialImports = findImports(sourceCode);
    for (const auto& imp : initialImports) {
        if (processedFiles.find(imp) == processedFiles.end()) {
            processingQueue.push_back(imp);
            processedFiles.insert(imp);
        }
    }

    // Process queue
    size_t queueIdx = 0;
    while (queueIdx < processingQueue.size()) {
        std::string currentFile = processingQueue[queueIdx++];
        
        // Try to find the file
        std::string filePath = currentFile;
        if (!fileExists(filePath)) {
            // Try relative to project root
            filePath = projectRoot + "/" + currentFile;
        }
        
        if (fileExists(filePath)) {
            std::string content = readFile(filePath);
            
            // Embed this file
            std::string safeName = currentFile;
            std::replace(safeName.begin(), safeName.end(), '.', '_');
            std::replace(safeName.begin(), safeName.end(), '/', '_');
            std::replace(safeName.begin(), safeName.end(), '-', '_');
            
            srcFile << "// Embedded file: " << currentFile << "\n";
            srcFile << "const char* file_" << safeName << " = R\"neutron_source(\n";
            srcFile << content;
            srcFile << "\n)neutron_source\";\n\n";
            
            // Scan for more imports
            auto newImports = findImports(content);
            for (const auto& imp : newImports) {
                if (processedFiles.find(imp) == processedFiles.end()) {
                    processingQueue.push_back(imp);
                    processedFiles.insert(imp);
                }
            }
        } else {
            std::cout << "Warning: Could not find imported file '" << currentFile << "' for embedding." << std::endl;
        }
    }
    
    // Write main function
    srcFile << "int main() {\n";
    srcFile << "    neutron::VM vm;\n\n";
    
    // Extract bundled native modules
    if (!bundledModules.empty()) {
        srcFile << "    // Extract bundled native modules\n";
        srcFile << "    std::string tempDir = std::filesystem::temp_directory_path().string() + \"/neutron_bundle_\" + std::to_string(std::rand());\n";
        srcFile << "    std::filesystem::create_directories(tempDir);\n";
        srcFile << "    vm.add_module_search_path(tempDir);\n\n";
        
        for (const auto& moduleName : bundledModules) {
            std::string filename = "lib" + moduleName + ".so"; // Default to Linux convention
            #ifdef _WIN32
            filename = moduleName + ".dll";
            #elif __APPLE__
            filename = "lib" + moduleName + ".dylib";
            #endif
            
            srcFile << "    {\n";
            srcFile << "        std::string libPath = tempDir + \"/" << filename << "\";\n";
            srcFile << "        std::ofstream libFile(libPath, std::ios::binary);\n";
            srcFile << "        if (libFile) {\n";
            srcFile << "            libFile.write((const char*)lib_" << moduleName << "_data, lib_" << moduleName << "_size);\n";
            srcFile << "            libFile.close();\n";
            srcFile << "        }\n";
            srcFile << "    }\n";
        }
        srcFile << "\n";
    }
    
    // Register embedded files
    for (const auto& file : processingQueue) {
        std::string safeName = file;
        std::replace(safeName.begin(), safeName.end(), '.', '_');
        std::replace(safeName.begin(), safeName.end(), '/', '_');
        std::replace(safeName.begin(), safeName.end(), '-', '_');
        
        srcFile << "    vm.addEmbeddedFile(\"" << file << "\", file_" << safeName << ");\n";
    }
    srcFile << "\n";
    
    // Load .nt modules
    for (const auto& moduleName : usedModules) {
        std::string boxModulePath = projectRoot + "/.box/modules/" + moduleName + "/" + moduleName + ".nt";
        std::string libPath = executableDir + "/../lib/" + moduleName + ".nt";
        
        if (fileExists(boxModulePath) || fileExists(libPath)) {
            srcFile << "    {\n";
            srcFile << "        neutron::Scanner scanner(" << moduleName << "_source);\n";
            srcFile << "        std::vector<neutron::Token> tokens = scanner.scanTokens();\n";
            srcFile << "        neutron::Parser parser(tokens);\n";
            srcFile << "        std::vector<std::unique_ptr<neutron::Stmt>> statements = parser.parse();\n";
            srcFile << "        neutron::Compiler compiler(vm);\n";
            srcFile << "        neutron::Function* function = compiler.compile(statements);\n";
            srcFile << "        vm.interpret(function);\n";
            srcFile << "    }\n";
        }
    }
    
    // Run main source
    srcFile << "    neutron::Scanner scanner(embedded_source);\n";
    srcFile << "    std::vector<neutron::Token> tokens = scanner.scanTokens();\n";
    srcFile << "    neutron::Parser parser(tokens);\n";
    srcFile << "    std::vector<std::unique_ptr<neutron::Stmt>> statements = parser.parse();\n";
    srcFile << "    neutron::Compiler compiler(vm);\n";
    srcFile << "    neutron::Function* function = compiler.compile(statements);\n";
    srcFile << "    vm.interpret(function);\n";
    srcFile << "    return 0;\n";
    srcFile << "}\n";

    srcFile.close();
    } // end if (!aotSuccess)
    
    // Determine compiler
    std::string compiler, linkFlags;
    std::string vcvarsPath; // Store vcvars path if found

    // Use cross-compiler if specified
    if (crossCompile && !crossCompiler.empty()) {
        compiler = crossCompiler;
        if (isARM) {
            linkFlags = "-lcurl -ljsoncpp -ldl -rdynamic";
        } else {
            linkFlags = "-lcurl -ljsoncpp -ldl -rdynamic";
        }
        
        // Add sysroot if available
        if (!crossSysroot.empty()) {
            linkFlags += " -Wl,--sysroot=" + crossSysroot;
        }
    } else if (isWindows && !isMingw) {
        compiler = "cl";
        // Add both executable dir and vcpkg lib path for linking
        std::string vcpkgLibPath = executableDir + "/vcpkg_installed/x64-windows/lib";
        // Include Windows system libraries required by the runtime (ws2_32 for sockets, advapi32 for registry/security)
        linkFlags = "/link /nologo /LIBPATH:\"" + executableDir + "\" /LIBPATH:\"" + vcpkgLibPath + "\" libcurl.lib jsoncpp.lib ws2_32.lib advapi32.lib";
        
        // Check if cl is in PATH, if not try to find vcvarsall.bat
        if (system("where cl > nul 2>&1") != 0) {
            // Try to find vcvarsall.bat to initialize MSVC environment (same paths as box uses)
            std::vector<std::string> vcvarsPaths = {
                "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat",
                "C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat",
                "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Auxiliary\\Build\\vcvarsall.bat",
                "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Auxiliary\\Build\\vcvarsall.bat",
                "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat",
                "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat",
                "C:\\Program Files\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat",
                "C:\\Program Files\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat"
            };
            
            for (const auto& path : vcvarsPaths) {
                if (std::filesystem::exists(path)) {
                    vcvarsPath = path;
                    break;
                }
            }
            
            // Don't fail here - we'll try to use vcvarsPath later when compiling
            // If it's still not found, the compile will fail with a better error message
        }
    } else if (isWindows && isMingw) {
        compiler = "g++";
        linkFlags = "-lcurl -ljsoncpp -lws2_32";
    } else {
        // Default to clang++ on macOS, g++ on Linux
        if (isMacOS) {
            // Check if clang++ is available (default on macOS)
            if (system("which clang++ > /dev/null 2>&1") == 0) {
                compiler = "clang++";
            } else {
                compiler = "g++";  // Fallback if gcc is installed via Homebrew
            }
            linkFlags = "-lcurl -ljsoncpp -framework CoreFoundation";
        } else {
            // Linux: default to g++
            compiler = "g++";
            // Linux: Add -rdynamic to export symbols for dlopen
            linkFlags = "-lcurl -ljsoncpp -ldl -rdynamic";
        }
    }
    
    // Determine Neutron source directory
    std::string neutronSrcDir;

    // 1. Check NEUTRON_HOME environment variable
    const char* neutronHomeEnv = std::getenv("NEUTRON_HOME");
    if (neutronHomeEnv) {
        neutronSrcDir = neutronHomeEnv;
    }

    // 2. Check if we are running from the root (dev mode)
    if (neutronSrcDir.empty()) {
        std::string checkPath = executableDir + "/include";
        if (std::filesystem::exists(checkPath)) {
            neutronSrcDir = executableDir;
        }
    }

    // 3. Check if we are running from bin/ (installed mode)
    if (neutronSrcDir.empty()) {
        std::string checkPath = executableDir + "/../include";
        if (std::filesystem::exists(checkPath)) {
            neutronSrcDir = (std::filesystem::path(executableDir) / "..").string();
        }
    }

    // 4. Check current working directory (for CI/test scenarios)
    if (neutronSrcDir.empty()) {
        std::string cwdCheck = "./include";
        if (std::filesystem::exists(cwdCheck)) {
            neutronSrcDir = ".";
        }
    }

    // 5. Fallback to current directory
    if (neutronSrcDir.empty()) {
        neutronSrcDir = ".";
    }
    
    // Normalize path
    try {
        neutronSrcDir = std::filesystem::absolute(neutronSrcDir).string();
    } catch (...) {
        // Keep as is if absolute fails
    }
    
    std::string includeDir = neutronSrcDir + "/include";
    std::string libsDir = neutronSrcDir + "/libs";
    
    // Check for system install layout (libs in share/neutron/libs)
    if (!std::filesystem::exists(libsDir)) {
        std::string systemLibs = neutronSrcDir + "/share/neutron/libs";
        if (std::filesystem::exists(systemLibs)) {
            libsDir = systemLibs;
        }
    }
    
    // Validate directories (silently continue if not found - will fail at compile time if needed)
    if (!std::filesystem::exists(includeDir) || !std::filesystem::exists(libsDir)) {
        // Don't spam warnings - the compile will fail with a clear error if headers are missing
    }
    
    // Try to find a static runtime archive
    std::string runtimeLibPath;
    std::vector<std::string> libCandidates = {
        // Windows paths - check installation directory first
        neutronSrcDir + "/neutron_runtime.lib",
        neutronSrcDir + "/lib/neutron_runtime.lib",
        neutronSrcDir + "/build/Release/neutron_runtime.lib",
        neutronSrcDir + "/build/neutron_runtime.lib",
        // Also check in the same directory as the neutron executable
        executableDir + "/neutron_runtime.lib",
        executableDir + "/lib/neutron_runtime.lib",
        // Unix paths
        neutronSrcDir + "/libneutron_runtime.a",
        neutronSrcDir + "/lib/libneutron_runtime.a",
        neutronSrcDir + "/build/libneutron_runtime.a"
    };
    
    // On Windows with MSVC, try to use static library first for better performance
    bool useStaticLib = true;

    // Try to find shared library first (faster linking)
    std::vector<std::string> sharedLibCandidates = {
        executableDir + "/libneutron_shared.so",
        neutronSrcDir + "/build/libneutron_shared.so",
        neutronSrcDir + "/libneutron_shared.so"
    };
    
    std::string sharedLibPath;
    for (const auto& path : sharedLibCandidates) {
        if (std::filesystem::exists(path)) {
            sharedLibPath = path;
            break;
        }
    }
    
    // Try to find static library on all platforms
    for (const auto& path : libCandidates) {
        if (std::filesystem::exists(path)) {
            runtimeLibPath = path;
            break;
        }
    }
    
    // Prefer shared library for faster builds (unless static is specifically needed)
    if (!sharedLibPath.empty()) {
        runtimeLibPath = sharedLibPath;
        useStaticLib = false;
    } else if (runtimeLibPath.empty()) {
        useStaticLib = false;
    }

    // Build compile command
    std::string compileCommand;
    std::vector<std::string> objectFiles; // Track object files for Windows
    
    // Add all necessary include paths
    std::string includePaths;
    if (isWindows && !isMingw) {
        includePaths = "/I\"" + includeDir + "\" /I\"" + includeDir + "/core\" /I\"" + includeDir + "/compiler\" /I\"" + includeDir + "/runtime\" /I\"" + includeDir + "/types\" /I\"" + includeDir + "/utils\" /I\"" + includeDir + "/cross-platfrom\" /I\"" + neutronSrcDir + "\" /I\"" + neutronSrcDir + "/src\" /I\"" + libsDir + "\"";
        
        // Add vcpkg headers if available (for curl, jsoncpp)
        std::string vcpkgInclude = neutronSrcDir + "/vcpkg_installed/x64-windows/include";
        if (std::filesystem::exists(vcpkgInclude)) {
            includePaths += " /I\"" + vcpkgInclude + "\"";
        }
        
        // On Windows MSVC, compile modules separately to avoid native.obj conflicts
        std::string objDir = std::filesystem::path(finalOutputPath).parent_path().string();
        if (objDir.empty()) objDir = ".";
        
        // First, compile each builtin module separately with unique object names
        // Skip this for AOT builds which are self-contained
        if ((!useStaticLib || runtimeLibPath.empty()) && !aotSuccess) {
            std::cout << "[2/4] Compiling modules..." << std::endl;
            auto builtinSources = getBuiltinModuleSources();
            for (const auto& src : builtinSources) {
                std::string fullPath = neutronSrcDir + "/" + src;
                if (fileExists(fullPath)) {
                    // Extract module name from path like "libs/sys/native.cpp" -> "sys"
                    size_t libsPos = src.find("libs/");
                    size_t nextSlash = src.find('/', libsPos + 5);
                    std::string moduleName = src.substr(libsPos + 5, nextSlash - libsPos - 5);
                    std::string objFile = objDir + "/" + moduleName + "_module.obj";

                    std::string moduleCmd = compiler + " /c /std:c++17 /EHsc /W3 /O2 /MT /D_CRT_SECURE_NO_WARNINGS /nologo /wd4267 /wd4244 /wd4100 /wd4458 /wd4273 /wd4101 " + includePaths + " \"" + fullPath + "\" /Fo\"" + objFile + "\"";
                    int result = system(moduleCmd.c_str());
                    if (result != 0) {
                        std::cerr << "Error compiling module: " << moduleName << std::endl;
                        return false;
                    }
                    objectFiles.push_back(objFile);
                }
            }
        }

        // Build the main compile command for Windows
        std::string archFlags = isARM ? " -march=armv8-a" : " -march=x86-64";
        std::string sysrootFlag = (!crossSysroot.empty()) ? " --sysroot=" + crossSysroot : "";
        compileCommand = compiler + " /std:c++17 /EHsc /W3 /O2 /MT /D_CRT_SECURE_NO_WARNINGS /nologo /wd4267 /wd4244 /wd4100 /wd4458 /wd4273 /wd4101" + archFlags + sysrootFlag + " " + includePaths + " \"" + tempSourcePath + "\" ";
    } else {
        includePaths = "-I\"" + includeDir + "\" -I\"" + includeDir + "/core\" -I\"" + includeDir + "/compiler\" -I\"" + includeDir + "/runtime\" -I\"" + includeDir + "/types\" -I\"" + includeDir + "/utils\" -I\"" + neutronSrcDir + "\" -I\"" + libsDir + "\"";
        std::string archFlags = isARM ? " -march=armv8-a" : " -march=x86-64";
        std::string sysrootFlag = (!crossSysroot.empty()) ? " --sysroot=" + crossSysroot : "";
        compileCommand = compiler + " -std=c++17 -Wall -Wextra -O2" + archFlags + sysrootFlag + " " + includePaths + " \"" + tempSourcePath + "\" ";
    }
    
    // If we found the runtime library and not on Windows MSVC, link it. Otherwise, compile sources.
    // Skip runtime linking for pure AOT builds (no external modules)
    if (aotSuccess) {
        std::cout << "      Linking standalone executable (no runtime)" << std::endl;
    } else if (!runtimeLibPath.empty()) {
        std::cout << "      Linking runtime library" << std::endl;
        
        // Check if it's a shared library
        bool isShared = (runtimeLibPath.find(".so") != std::string::npos || 
                        runtimeLibPath.find(".dylib") != std::string::npos ||
                        runtimeLibPath.find(".dll") != std::string::npos);
        
        if (isShared) {
            // Link shared library
            std::string libDir = std::filesystem::path(runtimeLibPath).parent_path().string();
            std::string libName = std::filesystem::path(runtimeLibPath).stem().string();
            if (libName.find("lib") == 0) {
                libName = libName.substr(3);
            }
            compileCommand += "-L\"" + libDir + "\" -l" + libName + " -Wl,-rpath,\"" + libDir + "\" ";
        } else if (useStaticLib) {
            // Link static library
            if (!isWindows && !isMacOS) {
                compileCommand += "-Wl,--whole-archive \"" + runtimeLibPath + "\" -Wl,--no-whole-archive ";
            } else {
                compileCommand += "\"" + runtimeLibPath + "\" ";
            }
        } else {
            // Fallback to static library linking
            if (!isWindows && !isMacOS) {
                compileCommand += "-Wl,--whole-archive \"" + runtimeLibPath + "\" -Wl,--no-whole-archive ";
            } else {
                compileCommand += "\"" + runtimeLibPath + "\" ";
            }
        }
        
        // On Windows, we need to compile dlfcn_compat_win.cpp to provide dlopen/dlsym/dlclose
        // These are Unix functions that the runtime uses, but Windows needs a compatibility shim
        if (isWindows && !isMingw) {
            std::string dlfcnCompat = neutronSrcDir + "/src/platform/dlfcn_compat_win.cpp";
            if (std::filesystem::exists(dlfcnCompat)) {
                compileCommand += "\"" + dlfcnCompat + "\" ";
            }
        }
    } else {
        std::cout << "[2/4] Compiling runtime..." << std::endl;
        // Add runtime sources
        auto runtimeSources = getRuntimeSources();
        for (const auto& src : runtimeSources) {
            std::string fullPath = neutronSrcDir + "/" + src;
            if (fileExists(fullPath)) {
                compileCommand += "\"" + fullPath + "\" ";
            }
        }
        
        // On Windows, add dlfcn compatibility shim
        if (isWindows && !isMingw) {
            std::string dlfcnCompat = neutronSrcDir + "/src/platform/dlfcn_compat_win.cpp";
            if (std::filesystem::exists(dlfcnCompat)) {
                compileCommand += "\"" + dlfcnCompat + "\" ";
            }
        }
    }

    // Add builtin modules (only if compiling from source, not when linking a library)
    // Skip this when we're linking either static or shared library, or for AOT builds
    if (runtimeLibPath.empty() && !(isWindows && !isMingw) && !aotSuccess) {
        auto builtinSources = getBuiltinModuleSources();
        for (const auto& src : builtinSources) {
            std::string fullPath = neutronSrcDir + "/" + src;
            if (fileExists(fullPath)) {
                compileCommand += "\"" + fullPath + "\" ";
            }
        }
    }
    
    // On Windows, add the pre-compiled module object files
    if (isWindows && !isMingw && !objectFiles.empty()) {
        for (const auto& objFile : objectFiles) {
            compileCommand += "\"" + objFile + "\" ";
        }
    }
    
    
    // Add Box native modules from project
    // Note: We already processed boxModules for embedding above if bundleLibs is true.
    // But we still need to handle static linking if bundleLibs is false.
    
    if (!boxModules.empty()) {
        if (bundleLibs) {
            std::cout << "  Bundling " << boxModules.size() << " module(s): ";
            for (size_t i = 0; i < boxModules.size(); i++) {
                std::cout << boxModules[i];
                if (i < boxModules.size() - 1) std::cout << ", ";
            }
            std::cout << std::endl;
            for (const auto& moduleName : boxModules) {
                (void)moduleName; // Suppress unused warning
            }
            // No need to add -L or -l flags or copy files, as we embedded them in the source
        } else {
            // Default: Static linking
            std::cout << "Linking " << boxModules.size() << " Box module(s) statically..." << std::endl;
            
            for (const auto& moduleName : boxModules) {
                std::string moduleDir = projectRoot + "/.box/modules/" + moduleName;
                std::string libA = moduleDir + "/lib" + moduleName + ".a";  // Static library
                
                if (fileExists(libA)) {
                    std::cout << "  - " << moduleName << " (static)" << std::endl;
                    compileCommand += "\"" + libA + "\" ";
                } else {
                    std::cerr << "Warning: No static library (.a) found for module: " << moduleName << std::endl;
                    std::cerr << "         Try 'neutron build --bundle' to use shared libraries instead." << std::endl;
                }
            }
        }
    }
    
    // Add output and link flags
    // For AOT builds with no external modules, skip unnecessary library links
    if (aotSuccess) {
        // Pure AOT build - no external libraries needed
        if (isWindows && !isMingw) {
            compileCommand += "/Fe:\"" + finalOutputPath + "\"";
        } else {
            compileCommand += "-o \"" + finalOutputPath + "\"";
        }
    } else if (isWindows && !isMingw) {
        compileCommand += "/Fe:\"" + finalOutputPath + "\" " + linkFlags;
    } else {
        compileCommand += linkFlags + " -o \"" + finalOutputPath + "\"";
    }
    
    // Check if compiler is available (skip for MSVC on Windows - we handle it later with vcvarsall)
    if (!(isWindows && compiler == "cl")) {
        std::string checkCmd;
        if (isWindows) {
            checkCmd = compiler + " --version > nul 2>&1";
        } else {
            checkCmd = "which " + compiler + " > /dev/null 2>&1";
        }
        
        if (system(checkCmd.c_str()) != 0) {
            std::cerr << "Error: Compiler '" << compiler << "' not found in PATH." << std::endl;
            if (isWindows && isMingw) {
                std::cerr << "Neutron requires a C++ compiler to build projects." << std::endl;
                std::cerr << "Please install MinGW-w64 and add 'bin' to your PATH." << std::endl;
                std::cerr << "Download: https://winlibs.com/ or via MSYS2" << std::endl;
            } else {
                std::cerr << "Please install g++ or clang++." << std::endl;
            }
            return false;
        }
    }

    std::cout << "[3/4] Compiling and linking..." << std::endl;
    // std::cout << "Command: " << compileCommand << std::endl;  // DEBUG: Commented out for clean output
    
    int result;
    
    // On Windows with MSVC, check if cl is in PATH and wrap with vcvarsall if needed
    #ifdef _WIN32
    if (compiler == "cl") {
        // Check if cl.exe is in PATH
        int checkResult = system("where cl >nul 2>&1");
        if (checkResult != 0) {
            // cl.exe not in PATH
            if (vcvarsPath.empty()) {
                // No vcvarsall.bat found either
                std::cerr << "\nError: Microsoft Visual C++ compiler not found." << std::endl;
                std::cerr << "\nNeutron requires MSVC to build projects on Windows." << std::endl;
                std::cerr << "\nTo install MSVC Build Tools:" << std::endl;
                std::cerr << "  1. Download: https://aka.ms/vs/17/release/vs_BuildTools.exe" << std::endl;
                std::cerr << "  2. Run the installer" << std::endl;
                std::cerr << "  3. Select 'Desktop development with C++' workload" << std::endl;
                std::cerr << "  4. Restart your terminal after installation" << std::endl;
                std::cerr << "\nAlternatively, run 'neutron build' from a Visual Studio Developer Command Prompt." << std::endl;
                return false;
            }
            
            // Found vcvarsall.bat, wrap command with it (suppress setup output)
            std::string wrappedCmd = "cmd /c \"\"" + vcvarsPath + "\" x64 >nul 2>&1 && " + compileCommand + "\"";
            result = system(wrappedCmd.c_str());
        } else {
            // cl is in PATH, use it directly
            result = system(compileCommand.c_str());
        }
    } else {
        result = system(compileCommand.c_str());
    }
    #else
    result = system(compileCommand.c_str());
    #endif

    // Clean up temp file
    std::filesystem::remove(tempSourcePath);

    if (result == 0) {
        std::cout << "[4/4] Build complete!" << std::endl;
        
        // Make executable on Unix
#ifndef _WIN32
        std::string chmodCmd = "chmod +x \"" + finalOutputPath + "\"";
        system(chmodCmd.c_str());
#endif
        return true;
    } else {
        std::cerr << "✗ Build failed!" << std::endl;
        return false;
    }
}

} // namespace neutron
