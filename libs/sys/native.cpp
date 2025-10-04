#include "native.h"
#include "vm.h"
#include "runtime/environment.h"
#include "types/value.h"
#include "types/json_object.h"
#include "types/array.h"
#include "expr.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <array>
#include <memory>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define getcwd _getcwd
#define chdir _chdir
#define mkdir(path, mode) _mkdir(path)
#define rmdir _rmdir
#define popen _popen
#define pclose _pclose
#else
#include <sys/types.h>
#include <dirent.h>
#endif

using namespace neutron;

// File Operations

static Value sys_read(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.read() expects 1 argument (filepath)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.read() expects a string filepath");
    }
    
    std::string filepath = std::get<std::string>(args[0].as);
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return Value(buffer.str());
}

static Value sys_write(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("sys.write() expects 2 arguments (filepath, content)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.write() expects a string filepath");
    }
    if (args[1].type != ValueType::STRING) {
        throw std::runtime_error("sys.write() expects string content");
    }
    
    std::string filepath = std::get<std::string>(args[0].as);
    std::string content = std::get<std::string>(args[1].as);
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filepath);
    }
    
    file << content;
    file.close();
    return Value(true);
}

static Value sys_append(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("sys.append() expects 2 arguments (filepath, content)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.append() expects a string filepath");
    }
    if (args[1].type != ValueType::STRING) {
        throw std::runtime_error("sys.append() expects string content");
    }
    
    std::string filepath = std::get<std::string>(args[0].as);
    std::string content = std::get<std::string>(args[1].as);
    
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for appending: " + filepath);
    }
    
    file << content;
    file.close();
    return Value(true);
}

static Value sys_cp(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("sys.cp() expects 2 arguments (source, destination)");
    }
    if (args[0].type != ValueType::STRING || args[1].type != ValueType::STRING) {
        throw std::runtime_error("sys.cp() expects string arguments");
    }
    
    std::string source = std::get<std::string>(args[0].as);
    std::string destination = std::get<std::string>(args[1].as);
    
    std::ifstream src(source, std::ios::binary);
    if (!src.is_open()) {
        throw std::runtime_error("Failed to open source file: " + source);
    }
    
    std::ofstream dst(destination, std::ios::binary);
    if (!dst.is_open()) {
        throw std::runtime_error("Failed to open destination file: " + destination);
    }
    
    dst << src.rdbuf();
    src.close();
    dst.close();
    
    return Value(true);
}

static Value sys_mv(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("sys.mv() expects 2 arguments (source, destination)");
    }
    if (args[0].type != ValueType::STRING || args[1].type != ValueType::STRING) {
        throw std::runtime_error("sys.mv() expects string arguments");
    }
    
    std::string source = std::get<std::string>(args[0].as);
    std::string destination = std::get<std::string>(args[1].as);
    
    if (std::rename(source.c_str(), destination.c_str()) != 0) {
        throw std::runtime_error("Failed to move file from " + source + " to " + destination);
    }
    
    return Value(true);
}

static Value sys_rm(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.rm() expects 1 argument (filepath)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.rm() expects a string filepath");
    }
    
    std::string filepath = std::get<std::string>(args[0].as);
    
    if (std::remove(filepath.c_str()) == 0) {
        return Value(true);
    } else {
        // Check if file doesn't exist
        std::ifstream file(filepath);
        if (!file.good()) {
            return Value(false); // File doesn't exist
        }
        throw std::runtime_error("Failed to remove file: " + filepath);
    }
}

static Value sys_exists(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.exists() expects 1 argument (path)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.exists() expects a string path");
    }
    
    std::string path = std::get<std::string>(args[0].as);
    
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path.c_str());
    return Value(attrs != INVALID_FILE_ATTRIBUTES);
#else
    struct stat buffer;
    return Value(stat(path.c_str(), &buffer) == 0);
#endif
}

// Directory Operations

static Value sys_mkdir(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.mkdir() expects 1 argument (path)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.mkdir() expects a string path");
    }
    
    std::string path = std::get<std::string>(args[0].as);
    
#ifdef _WIN32
    if (mkdir(path.c_str(), 0755) != 0) {
#else
    if (mkdir(path.c_str(), 0755) != 0) {
#endif
        throw std::runtime_error("Failed to create directory: " + path);
    }
    
    return Value(true);
}

static Value sys_rmdir(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.rmdir() expects 1 argument (path)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.rmdir() expects a string path");
    }
    
    std::string path = std::get<std::string>(args[0].as);
    
    if (rmdir(path.c_str()) != 0) {
        throw std::runtime_error("Failed to remove directory: " + path);
    }
    
    return Value(true);
}

// System Information

static Value sys_cwd(const std::vector<Value>& args) {
    if (args.size() != 0) {
        throw std::runtime_error("sys.cwd() expects 0 arguments");
    }
    
    char buffer[4096];
    if (getcwd(buffer, sizeof(buffer)) == nullptr) {
        throw std::runtime_error("Failed to get current working directory");
    }
    
    return Value(std::string(buffer));
}

static Value sys_chdir(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.chdir() expects 1 argument (path)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.chdir() expects a string path");
    }
    
    std::string path = std::get<std::string>(args[0].as);
    
    if (chdir(path.c_str()) != 0) {
        throw std::runtime_error("Failed to change directory to: " + path);
    }
    
    return Value(true);
}

static Value sys_env(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.env() expects 1 argument (variable name)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.env() expects a string variable name");
    }
    
    std::string varname = std::get<std::string>(args[0].as);
    const char* value = std::getenv(varname.c_str());
    
    if (value == nullptr) {
        return Value(); // Return nil
    }
    
    return Value(std::string(value));
}

static Value sys_args(const std::vector<Value>& args) {
    if (args.size() != 0) {
        throw std::runtime_error("sys.args() expects 0 arguments");
    }
    
    // For now, return an empty array
    // In a full implementation, you'd need to pass command line args to the VM
    auto array = new Array();
    return Value(array);
}

static Value sys_info(const std::vector<Value>& args) {
    if (args.size() != 0) {
        throw std::runtime_error("sys.info() expects 0 arguments");
    }
    
    auto info = new JsonObject();
    
    // Platform
#ifdef _WIN32
    info->properties["platform"] = Value(std::string("windows"));
#elif __APPLE__
    info->properties["platform"] = Value(std::string("macos"));
#elif __linux__
    info->properties["platform"] = Value(std::string("linux"));
#else
    info->properties["platform"] = Value(std::string("unknown"));
#endif
    
    // Architecture
#if defined(__x86_64__) || defined(_M_X64)
    info->properties["arch"] = Value(std::string("x64"));
#elif defined(__i386__) || defined(_M_IX86)
    info->properties["arch"] = Value(std::string("x86"));
#elif defined(__aarch64__) || defined(_M_ARM64)
    info->properties["arch"] = Value(std::string("arm64"));
#else
    info->properties["arch"] = Value(std::string("unknown"));
#endif
    
    // Current working directory
    char buffer[4096];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        info->properties["cwd"] = Value(std::string(buffer));
    }
    
    return Value(info);
}

// User Input

static Value sys_input(const std::vector<Value>& args) {
    if (args.size() > 1) {
        throw std::runtime_error("sys.input() expects 0 or 1 argument (optional prompt)");
    }
    
    if (args.size() == 1) {
        if (args[0].type != ValueType::STRING) {
            throw std::runtime_error("sys.input() prompt must be a string");
        }
        std::cout << std::get<std::string>(args[0].as);
    }
    
    std::string line;
    std::getline(std::cin, line);
    return Value(line);
}

// Process Control

static Value sys_exit(const std::vector<Value>& args) {
    if (args.size() > 1) {
        throw std::runtime_error("sys.exit() expects 0 or 1 argument (optional exit code)");
    }
    
    int exit_code = 0;
    if (args.size() == 1) {
        if (args[0].type != ValueType::NUMBER) {
            throw std::runtime_error("sys.exit() exit code must be a number");
        }
        exit_code = static_cast<int>(std::get<double>(args[0].as));
    }
    
    std::exit(exit_code);
    return Value(); // Never reached
}

static Value sys_exec(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.exec() expects 1 argument (command)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.exec() expects a string command");
    }
    
    std::string command = std::get<std::string>(args[0].as);
    std::array<char, 128> buffer;
    std::string result;
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to execute command: " + command);
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    
    pclose(pipe);
    return Value(result);
}

namespace neutron {
    void register_sys_functions(std::shared_ptr<Environment> env) {
        // File Operations
        env->define("read", Value(new NativeFn(sys_read, 1)));
        env->define("write", Value(new NativeFn(sys_write, 2)));
        env->define("append", Value(new NativeFn(sys_append, 2)));
        env->define("cp", Value(new NativeFn(sys_cp, 2)));
        env->define("mv", Value(new NativeFn(sys_mv, 2)));
        env->define("rm", Value(new NativeFn(sys_rm, 1)));
        env->define("exists", Value(new NativeFn(sys_exists, 1)));
        
        // Directory Operations
        env->define("mkdir", Value(new NativeFn(sys_mkdir, 1)));
        env->define("rmdir", Value(new NativeFn(sys_rmdir, 1)));
        
        // System Information
        env->define("cwd", Value(new NativeFn(sys_cwd, 0)));
        env->define("chdir", Value(new NativeFn(sys_chdir, 1)));
        env->define("env", Value(new NativeFn(sys_env, 1)));
        env->define("args", Value(new NativeFn(sys_args, 0)));
        env->define("info", Value(new NativeFn(sys_info, 0)));
        
        // User Input
        env->define("input", Value(new NativeFn(sys_input, -1)));
        
        // Process Control
        env->define("exit", Value(new NativeFn(sys_exit, -1)));
        env->define("exec", Value(new NativeFn(sys_exec, 1)));
    }
}

extern "C" void neutron_init_sys_module(VM* vm) {
    auto sys_env = std::make_shared<neutron::Environment>();
    neutron::register_sys_functions(sys_env);
    auto sys_module = new neutron::Module("sys", sys_env);
    vm->define_module("sys", sys_module);
}