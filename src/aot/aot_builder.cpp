#include "aot/aot_builder.h"
#include "proton.h"
#include "aot/proton_windows_bridge.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <filesystem>

namespace neutron {
namespace aot {

std::string AotBuilder::generate_qbe_ir(const Chunk* chunk, const std::string& func_name) {
    QbeCodegen codegen(chunk);
    return codegen.emit_function(func_name);
}

bool AotBuilder::write_qbe_ir(const Chunk* chunk, const std::string& func_name,
                               const std::string& ssa_path) {
    std::string ir = generate_qbe_ir(chunk, func_name);
    if (ir.empty()) return false;

    std::ofstream file(ssa_path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not write QBE IR to " << ssa_path << std::endl;
        return false;
    }
    file << ir;
    file.close();
    return true;
}

bool AotBuilder::compile_ssa(const std::string& ssa, const std::string& obj_path) {
    std::string asm_path = obj_path;
    size_t dot_pos = asm_path.rfind('.');
    if (dot_pos != std::string::npos) {
        asm_path = asm_path.substr(0, dot_pos) + ".s";
    } else {
        asm_path += ".s";
    }

    // Compile SSA to assembly using embedded Proton
    FILE *asf = fopen(asm_path.c_str(), "w");
    if (!asf) {
        std::cerr << "Error: Could not write assembly to " << asm_path << std::endl;
        return false;
    }

    int ret = proton_compile_ssa(ssa.c_str(), asf);
    fclose(asf);
    if (ret != 0) {
        std::cerr << "Error: Proton compilation failed" << std::endl;
        std::filesystem::remove(asm_path);
        return false;
    }

    // Assemble .s to object file using the platform bridge
    if (!ProtonWindowsBridge::assemble_object(asm_path, obj_path)) {
        std::cerr << "Error: assembler failed for " << asm_path << std::endl;
        std::filesystem::remove(asm_path);
        return false;
    }

    std::filesystem::remove(asm_path);
    return true;
}

bool AotBuilder::assemble(const std::string& ssa_path, const std::string& obj_path) {
    std::string asm_path = obj_path;
    size_t dot_pos = asm_path.rfind('.');
    if (dot_pos != std::string::npos) {
        asm_path = asm_path.substr(0, dot_pos) + ".s";
    } else {
        asm_path += ".s";
    }
    std::string cmd = "qbe -o \"" + asm_path + "\" \"" + ssa_path + "\"";
    int result = system(cmd.c_str());
    if (result != 0) {
        std::cerr << "Error: qbe assembly failed for " << ssa_path << std::endl;
        return false;
    }
    result = system(("as -o \"" + obj_path + "\" \"" + asm_path + "\"").c_str());
    if (result != 0) {
        std::cerr << "Error: system assembler failed for " << asm_path << std::endl;
        return false;
    }
    return true;
}

std::string AotBuilder::compile_to_object(const Chunk* chunk, const std::string& func_name,
                                           const std::string& output_dir) {
    std::string ir = generate_qbe_ir(chunk, func_name);
    if (ir.empty()) return "";

    std::string obj_path = output_dir + "/" + func_name + ProtonWindowsBridge::object_extension();

    if (!compile_ssa(ir, obj_path)) {
        return "";
    }

    return obj_path;
}

bool AotBuilder::link(const std::string& obj_path, const std::string& output_path,
                       const std::string& runtime_lib_path, bool is_windows) {
    std::string compiler = "g++";
    std::string link_flags;
#ifdef __APPLE__
    if (system("which clang++ > /dev/null 2>&1") == 0) {
        compiler = "clang++";
    }
    link_flags = "-lproton -lcurl -ljsoncpp -framework CoreFoundation";
    if (std::filesystem::exists("/opt/homebrew/lib")) {
        link_flags = "-L/opt/homebrew/lib " + link_flags;
    }
    if (std::filesystem::exists("/usr/local/lib")) {
        link_flags = "-L/usr/local/lib " + link_flags;
    }
#else
    link_flags = "-lproton -lcurl -ljsoncpp -ldl -lpthread";
#endif

    if (is_windows) {
#ifdef _WIN32
        // On MSVC Windows, try cl.exe first
        if (system("where cl >nul 2>&1") == 0) {
            compiler = "cl";
            link_flags = "/Fe:\"" + output_path + "\" /link /nologo "
                         "proton.lib libcurl.lib jsoncpp.lib ws2_32.lib advapi32.lib";
            std::string cmd = compiler + " \"" + obj_path + "\" \"" + runtime_lib_path + "\" " + link_flags;
            int result = system(cmd.c_str());
            if (result != 0) {
                std::cerr << "Error: MSVC linking failed for " << obj_path << std::endl;
                return false;
            }
            return true;
        }
#endif
        // Fallback to MinGW-style linking
        compiler = "g++";
        link_flags = "-lproton -lcurl -ljsoncpp -lws2_32 -lpthread";
    }

    std::string cmd = compiler + " -o \"" + output_path + "\" \"" + obj_path + "\" ";
    if (!is_windows) {
        cmd += "-Wl,--whole-archive \"" + runtime_lib_path + "\" -Wl,--no-whole-archive ";
    } else {
        cmd += "\"" + runtime_lib_path + "\" ";
    }
    cmd += link_flags;

    int result = system(cmd.c_str());
    if (result != 0) {
        std::cerr << "Error: linking failed for " << obj_path << std::endl;
        return false;
    }
    return true;
}

} // namespace aot
} // namespace neutron
