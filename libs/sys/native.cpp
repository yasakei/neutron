#include "libs/sys/native.h"
#include "vm.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>

namespace fs = std::filesystem;

using namespace neutron;

// File operations

// Copy file
Value sys_cp(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for sys.cp().");
    }
    
    if (arguments[0].type != ValueType::STRING || arguments[1].type != ValueType::STRING) {
        throw std::runtime_error("Arguments for sys.cp() must be strings.");
    }
    
    std::string source = std::get<std::string>(arguments[0].as);
    std::string destination = std::get<std::string>(arguments[1].as);
    
    try {
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
        return Value(true);
    } catch (const fs::filesystem_error& ex) {
        throw std::runtime_error("Failed to copy file: " + std::string(ex.what()));
    }
}

// Move file
Value sys_mv(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for sys.mv().");
    }
    
    if (arguments[0].type != ValueType::STRING || arguments[1].type != ValueType::STRING) {
        throw std::runtime_error("Arguments for sys.mv() must be strings.");
    }
    
    std::string source = std::get<std::string>(arguments[0].as);
    std::string destination = std::get<std::string>(arguments[1].as);
    
    try {
        fs::rename(source, destination);
        return Value(true);
    } catch (const fs::filesystem_error& ex) {
        throw std::runtime_error("Failed to move file: " + std::string(ex.what()));
    }
}

// Remove file
Value sys_rm(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for sys.rm().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Argument for sys.rm() must be a string.");
    }
    
    std::string path = std::get<std::string>(arguments[0].as);
    
    try {
        bool result = fs::remove(path);
        return Value(result);
    } catch (const fs::filesystem_error& ex) {
        throw std::runtime_error("Failed to remove file: " + std::string(ex.what()));
    }
}

// Create directory
Value sys_mkdir(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for sys.mkdir().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Argument for sys.mkdir() must be a string.");
    }
    
    std::string path = std::get<std::string>(arguments[0].as);
    
    try {
        bool result = fs::create_directory(path);
        return Value(result);
    } catch (const fs::filesystem_error& ex) {
        throw std::runtime_error("Failed to create directory: " + std::string(ex.what()));
    }
}

// Remove directory
Value sys_rmdir(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for sys.rmdir().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Argument for sys.rmdir() must be a string.");
    }
    
    std::string path = std::get<std::string>(arguments[0].as);
    
    try {
        bool result = fs::remove(path);
        return Value(result);
    } catch (const fs::filesystem_error& ex) {
        throw std::runtime_error("Failed to remove directory: " + std::string(ex.what()));
    }
}

// Check if file/directory exists
Value sys_exists(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for sys.exists().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Argument for sys.exists() must be a string.");
    }
    
    std::string path = std::get<std::string>(arguments[0].as);
    
    try {
        bool result = fs::exists(path);
        return Value(result);
    } catch (const fs::filesystem_error& ex) {
        return Value(false);
    }
}

// Read file
Value sys_read(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for sys.read().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Argument for sys.read() must be a string.");
    }
    
    std::string path = std::get<std::string>(arguments[0].as);
    
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for reading.");
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        return Value(buffer.str());
    } catch (const std::exception& ex) {
        throw std::runtime_error("Failed to read file: " + std::string(ex.what()));
    }
}

// Write file
Value sys_write(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for sys.write().");
    }
    
    if (arguments[0].type != ValueType::STRING || arguments[1].type != ValueType::STRING) {
        throw std::runtime_error("Arguments for sys.write() must be strings.");
    }
    
    std::string path = std::get<std::string>(arguments[0].as);
    std::string content = std::get<std::string>(arguments[1].as);
    
    try {
        std::ofstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for writing.");
        }
        
        file << content;
        return Value(true);
    } catch (const std::exception& ex) {
        throw std::runtime_error("Failed to write file: " + std::string(ex.what()));
    }
}

// Append to file
Value sys_append(std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for sys.append().");
    }
    
    if (arguments[0].type != ValueType::STRING || arguments[1].type != ValueType::STRING) {
        throw std::runtime_error("Arguments for sys.append() must be strings.");
    }
    
    std::string path = std::get<std::string>(arguments[0].as);
    std::string content = std::get<std::string>(arguments[1].as);
    
    try {
        std::ofstream file(path, std::ios::app);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for appending.");
        }
        
        file << content;
        return Value(true);
    } catch (const std::exception& ex) {
        throw std::runtime_error("Failed to append to file: " + std::string(ex.what()));
    }
}

// System info

// Get current working directory
Value sys_cwd(std::vector<Value> arguments) {
    if (arguments.size() != 0) {
        throw std::runtime_error("Expected 0 arguments for sys.cwd().");
    }
    
    try {
        std::string cwd = fs::current_path().string();
        return Value(cwd);
    } catch (const fs::filesystem_error& ex) {
        throw std::runtime_error("Failed to get current working directory: " + std::string(ex.what()));
    }
}

// Change directory
Value sys_chdir(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for sys.chdir().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Argument for sys.chdir() must be a string.");
    }
    
    std::string path = std::get<std::string>(arguments[0].as);
    
    try {
        fs::current_path(path);
        return Value(true);
    } catch (const fs::filesystem_error& ex) {
        throw std::runtime_error("Failed to change directory: " + std::string(ex.what()));
    }
}

// Get environment variable
Value sys_env(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for sys.env().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Argument for sys.env() must be a string.");
    }
    
    std::string name = std::get<std::string>(arguments[0].as);
    
    const char* value = std::getenv(name.c_str());
    if (value) {
        return Value(std::string(value));
    }
    return Value(nullptr);
}

// Get command line arguments (stub implementation)
Value sys_args(std::vector<Value> arguments) {
    if (arguments.size() != 0) {
        throw std::runtime_error("Expected 0 arguments for sys.args().");
    }
    
    // In a real implementation, this would return the actual command line arguments
    // For now, we'll return an empty array
    auto arr = new JsonArray();
    return Value(arr);
}

// Read input from stdin
Value sys_input(std::vector<Value> arguments) {
    std::string prompt = "";
    
    if (arguments.size() == 1) {
        if (arguments[0].type != ValueType::STRING) {
            throw std::runtime_error("Argument for sys.input() must be a string.");
        }
        prompt = std::get<std::string>(arguments[0].as);
    } else if (arguments.size() != 0) {
        throw std::runtime_error("Expected 0 or 1 arguments for sys.input().");
    }
    
    // Display the prompt
    std::cout << prompt;
    std::cout.flush();
    
    // Read the input
    std::string input;
    std::getline(std::cin, input);
    
    return Value(input);
}

// Get system info
Value sys_info(std::vector<Value> arguments) {
    if (arguments.size() != 0) {
        throw std::runtime_error("Expected 0 arguments for sys.info().");
    }
    
    auto info = new JsonObject();
    
    // Get some basic system info
    info->properties["cwd"] = Value(fs::current_path().string());
    
    // Get some basic system info
    info->properties["platform"] = Value(std::string("linux")); // Simplified implementation
    info->properties["arch"] = Value(std::string("x86_64"));    // Simplified implementation
    
    return Value(info);
}

// Process control

// Exit the program
Value sys_exit(std::vector<Value> arguments) {
    int code = 0;
    
    if (arguments.size() == 1) {
        if (arguments[0].type != ValueType::NUMBER) {
            throw std::runtime_error("Argument for sys.exit() must be a number.");
        }
        code = static_cast<int>(std::get<double>(arguments[0].as));
    } else if (arguments.size() != 0) {
        throw std::runtime_error("Expected 0 or 1 arguments for sys.exit().");
    }
    
    exit(code);
    return Value(); // This will never be reached
}

// Execute a command
Value sys_exec(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for sys.exec().");
    }
    
    if (arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Argument for sys.exec() must be a string.");
    }
    
    std::string command = std::get<std::string>(arguments[0].as);
    
    // Use popen to execute the command
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("Failed to execute command.");
    }
    
    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof buffer, pipe) != nullptr) {
        result += buffer;
    }
    
    int status = pclose(pipe);
    if (status == -1) {
        throw std::runtime_error("Failed to close pipe.");
    }
    
    // Remove trailing newline if present
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    return Value(result);
}

void register_sys_functions(std::shared_ptr<Environment> env) {
    // File operations
    env->define("cp", Value(new NativeFn(sys_cp, 2)));
    env->define("mv", Value(new NativeFn(sys_mv, 2)));
    env->define("rm", Value(new NativeFn(sys_rm, 1)));
    env->define("mkdir", Value(new NativeFn(sys_mkdir, 1)));
    env->define("rmdir", Value(new NativeFn(sys_rmdir, 1)));
    env->define("exists", Value(new NativeFn(sys_exists, 1)));
    env->define("read", Value(new NativeFn(sys_read, 1)));
    env->define("write", Value(new NativeFn(sys_write, 2)));
    env->define("append", Value(new NativeFn(sys_append, 2)));
    
    // System info
    env->define("cwd", Value(new NativeFn(sys_cwd, 0)));
    env->define("chdir", Value(new NativeFn(sys_chdir, 1)));
    env->define("env", Value(new NativeFn(sys_env, 1)));
    env->define("args", Value(new NativeFn(sys_args, 0)));
    env->define("info", Value(new NativeFn(sys_info, 0)));
    env->define("input", Value(new NativeFn(sys_input, -1))); // 0 or 1 arguments
    
    // Process control
    env->define("exit", Value(new NativeFn(sys_exit, -1))); // 0 or 1 arguments
    env->define("exec", Value(new NativeFn(sys_exec, 1)));
}

extern "C" void neutron_init_sys_module(VM* vm) {
    auto sys_env = std::make_shared<Environment>();
    register_sys_functions(sys_env);
    auto sys_module = new Module("sys", sys_env);
    vm->define_module("sys", sys_module);
}

} // namespace neutron

} // namespace neutron
