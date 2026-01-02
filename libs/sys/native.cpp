/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Public License 1.0.
 * For full license text, see LICENSE file in the root directory.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "native.h"
#include "vm.h"
#include "checkpoint.h"
#include "runtime/environment.h"
#include "types/value.h"
#include "types/obj_string.h"
#include "types/json_object.h"
#include "types/array.h"
#include "types/buffer.h"
#include "expr.h"
#include "platform/platform.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <array>
#include <memory>
#include <filesystem>
#include <chrono>
#include <thread>
#include <iomanip>

using namespace neutron;
namespace fs = std::filesystem;

// Checkpoint Operations
Value sys_checkpoint(VM& vm, std::vector<Value> args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.checkpoint() expects 1 argument (filepath)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.checkpoint() expects a string filepath");
    }
    
    std::string filepath = args[0].asString()->chars;
    // Pop 2 values (function + arg) and push true
    CheckpointManager::saveCheckpoint(vm, filepath, Value(true), 1);
    return Value(true);
}

// File Operations

Value sys_read(VM& vm, std::vector<Value> args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.read() expects 1 argument (filepath)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.read() expects a string filepath");
    }
    
    std::string filepath = args[0].asString()->chars;
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return Value(vm.internString(buffer.str()));
}

Value sys_write(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) {
        throw std::runtime_error("sys.write() expects 2 arguments (filepath, content)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.write() expects a string filepath");
    }
    if (args[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.write() expects string content");
    }
    
    std::string filepath = args[0].asString()->chars;
    std::string content = args[1].asString()->chars;
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filepath);
    }
    
    file << content;
    file.close();
    return Value(true);
}

Value sys_append(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) {
        throw std::runtime_error("sys.append() expects 2 arguments (filepath, content)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.append() expects a string filepath");
    }
    if (args[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.append() expects string content");
    }
    
    std::string filepath = args[0].asString()->chars;
    std::string content = args[1].asString()->chars;
    
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for appending: " + filepath);
    }
    
    file << content;
    file.close();
    return Value(true);
}

Value sys_cp(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) {
        throw std::runtime_error("sys.cp() expects 2 arguments (source, destination)");
    }
    if (args[0].type != ValueType::OBJ_STRING || args[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.cp() expects string arguments");
    }
    
    std::string source = args[0].asString()->chars;
    std::string destination = args[1].asString()->chars;
    
    if (!platform::copyFile(source, destination)) {
        throw std::runtime_error("Failed to copy file from " + source + " to " + destination);
    }
    
    return Value(true);
}

Value sys_mv(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) {
        throw std::runtime_error("sys.mv() expects 2 arguments (source, destination)");
    }
    if (args[0].type != ValueType::OBJ_STRING || args[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.mv() expects string arguments");
    }
    
    std::string source = args[0].asString()->chars;
    std::string destination = args[1].asString()->chars;
    
    if (!platform::moveFile(source, destination)) {
        throw std::runtime_error("Failed to move file from " + source + " to " + destination);
    }
    
    return Value(true);
}

Value sys_rm(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("sys.rm() expects 1 argument (filepath)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.rm() expects a string filepath");
    }
    
    std::string filepath = args[0].asString()->chars;
    
    if (!platform::removeFile(filepath)) {
        if (!platform::fileExists(filepath)) {
            return Value(false); // File doesn't exist
        }
        throw std::runtime_error("Failed to remove file: " + filepath);
    }
    
    return Value(true);
}

Value sys_exists(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("sys.exists() expects 1 argument (path)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.exists() expects a string path");
    }
    
    std::string path = args[0].asString()->chars;
    return Value(platform::fileExists(path));
}

// Directory Operations

Value sys_mkdir(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("sys.mkdir() expects 1 argument (path)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.mkdir() expects a string path");
    }
    
    std::string path = args[0].asString()->chars;
    
    if (!platform::createDirectory(path)) {
        throw std::runtime_error("Failed to create directory: " + path);
    }
    
    return Value(true);
}

Value sys_rmdir(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() < 1 || args.size() > 2) {
        throw std::runtime_error("sys.rmdir() expects 1 or 2 arguments (path, optional recursive)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.rmdir() expects a string path");
    }
    
    std::string pathStr = args[0].asString()->chars;
    bool recursive = false;
    
    // Check if recursive flag is provided
    if (args.size() == 2) {
        if (args[1].type != ValueType::BOOLEAN) {
            throw std::runtime_error("sys.rmdir() recursive flag must be a boolean");
        }
        recursive = args[1].as.boolean;
    }
    
    fs::path path(pathStr);
    fs::path cwd = fs::current_path();
    
    if (!fs::exists(path)) {
        throw std::runtime_error("Directory not found: " + pathStr);
    }
    
    if (!fs::is_directory(path)) {
        throw std::runtime_error("Not a directory: " + pathStr);
    }
    
    // Safety check: ensure the path is within or relative to current directory
    fs::path absPath = fs::absolute(path);
    fs::path absCwd = fs::absolute(cwd);
    
    // Check if trying to delete parent directory or outside cwd
    auto [root_end, nothing] = std::mismatch(absCwd.begin(), absCwd.end(), absPath.begin(), absPath.end());
    if (root_end != absCwd.end()) {
        throw std::runtime_error("Cannot delete directory outside current working directory: " + pathStr);
    }
    
    try {
        if (recursive) {
            // Remove directory and all contents recursively
            fs::remove_all(path);
        } else {
            // Remove only if empty
            if (!platform::removeDirectory(pathStr)) {
                throw std::runtime_error("Failed to remove directory (may not be empty): " + pathStr);
            }
        }
    } catch (const fs::filesystem_error& e) {
        throw std::runtime_error("Failed to remove directory: " + std::string(e.what()));
    }
    
    return Value(true);
}

Value sys_listdir(VM& vm, std::vector<Value> args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.listdir() expects 1 argument (path)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.listdir() expects a string path");
    }
    
    std::string path = args[0].asString()->chars;
    std::vector<std::string> files = platform::listDirectory(path);
    
    auto array = vm.allocate<Array>();
    for (const auto& file : files) {
        array->elements.push_back(Value(vm.internString(file)));
    }
    return Value(array);
}

Value sys_stat(VM& vm, std::vector<Value> args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.stat() expects 1 argument (path)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.stat() expects a string path");
    }
    
    std::string pathStr = args[0].asString()->chars;
    fs::path path(pathStr);
    
    if (!fs::exists(path)) {
        throw std::runtime_error("File not found: " + pathStr);
    }
    
    auto info = vm.allocate<JsonObject>();
    info->properties[vm.internString("size")] = Value(static_cast<double>(fs::file_size(path)));
    
    // Handle time conversion safely
    auto ftime = fs::last_write_time(path);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
    );
    std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&cftime), "%Y-%m-%d %H:%M:%S");
    info->properties[vm.internString("mtime")] = Value(vm.internString(ss.str()));
    
    info->properties[vm.internString("is_directory")] = Value(fs::is_directory(path));
    info->properties[vm.internString("is_file")] = Value(fs::is_regular_file(path));
    
    return Value(info);
}

Value sys_chmod(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) {
        throw std::runtime_error("sys.chmod() expects 2 arguments (path, mode)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.chmod() expects a string path");
    }
    if (args[1].type != ValueType::NUMBER) {
        throw std::runtime_error("sys.chmod() expects a number mode");
    }
    
    std::string pathStr = args[0].asString()->chars;
    int mode = static_cast<int>(args[1].as.number);
    
    // Cast integer mode to permissions
    fs::permissions(pathStr, static_cast<fs::perms>(mode), fs::perm_options::replace);
    
    return Value(true);
}

Value sys_tmpfile(VM& vm, std::vector<Value> args) {
    if (args.size() != 0) {
        throw std::runtime_error("sys.tmpfile() expects 0 arguments");
    }
    
    auto tmp_dir = fs::temp_directory_path();
    
    // Generate a unique name using timestamp and random number
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    int rand_val = std::rand();
    std::string filename = "neutron_tmp_" + std::to_string(now) + "_" + std::to_string(rand_val);
    fs::path tmp_path = tmp_dir / filename;
    
    return Value(vm.internString(tmp_path.string()));
}

// System Information

Value sys_cwd(VM& vm, std::vector<Value> args) {
    if (args.size() != 0) {
        throw std::runtime_error("sys.cwd() expects 0 arguments");
    }
    
    std::string cwd = platform::getCwd();
    if (cwd.empty()) {
        throw std::runtime_error("Failed to get current working directory");
    }
    
    return Value(vm.internString(cwd));
}

Value sys_chdir(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("sys.chdir() expects 1 argument (path)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.chdir() expects a string path");
    }
    
    std::string path = args[0].asString()->chars;
    
    if (!platform::setCwd(path)) {
        throw std::runtime_error("Failed to change directory to: " + path);
    }
    
    return Value(true);
}

Value sys_env(VM& vm, std::vector<Value> args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.env() expects 1 argument (variable name)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.env() expects a string variable name");
    }
    
    std::string varname = args[0].asString()->chars;
    std::string value = platform::getEnv(varname);
    
    if (value.empty()) {
        return Value(); // Return nil
    }
    
    return Value(vm.internString(value));
}

Value sys_args(VM& vm, std::vector<Value> args) {
    if (args.size() != 0) {
        throw std::runtime_error("sys.args() expects 0 arguments");
    }
    
    // Return command line arguments from VM
    auto array = vm.allocate<Array>();
    for (const auto& arg : vm.commandLineArgs) {
        array->elements.push_back(Value(vm.internString(arg)));
    }
    return Value(array);
}

Value sys_info(VM& vm, std::vector<Value> args) {
    if (args.size() != 0) {
        throw std::runtime_error("sys.info() expects 0 arguments");
    }
    
    auto info = vm.allocate<JsonObject>();
    
    // Use platform abstraction for system info
    info->properties[vm.internString("platform")] = Value(vm.internString(platform::getPlatform()));
    info->properties[vm.internString("arch")] = Value(vm.internString(platform::getArch()));
    info->properties[vm.internString("user")] = Value(vm.internString(platform::getUsername()));
    info->properties[vm.internString("hostname")] = Value(vm.internString(platform::getHostname()));
    info->properties[vm.internString("cwd")] = Value(vm.internString(platform::getCwd()));
    
    return Value(info);
}

// User Input

Value sys_input(VM& vm, std::vector<Value> args) {
    if (args.size() > 1) {
        throw std::runtime_error("sys.input() expects 0 or 1 argument (optional prompt)");
    }
    
    if (args.size() == 1) {
        if (args[0].type != ValueType::OBJ_STRING) {
            throw std::runtime_error("sys.input() prompt must be a string");
        }
        std::cout << args[0].asString()->chars;
    }
    
    std::string line;
    std::getline(std::cin, line);
    return Value(vm.internString(line));
}

Value sys_sleep(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("sys.sleep() expects 1 argument (milliseconds)");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("sys.sleep() expects a number (milliseconds)");
    }
    
    int ms = static_cast<int>(args[0].as.number);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    
    return Value(true);
}

// Process Control

Value sys_exit(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() > 1) {
        throw std::runtime_error("sys.exit() expects 0 or 1 argument (optional exit code)");
    }
    
    int exit_code = 0;
    if (args.size() == 1) {
        if (args[0].type != ValueType::NUMBER) {
            throw std::runtime_error("sys.exit() exit code must be a number");
        }
        exit_code = static_cast<int>(args[0].as.number);
    }
    
    platform::exitProcess(exit_code);
    return Value(); // Never reached
}

Value sys_exec(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) {
        throw std::runtime_error("sys.exec() expects 1 argument (command)");
    }
    if (args[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("sys.exec() expects a string command");
    }
    
    std::string command = args[0].asString()->chars;
    int exitCode = platform::execute(command);
    
    return Value(static_cast<double>(exitCode));
}

Value sys_alloc(VM& vm, std::vector<Value> args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.alloc() expects 1 argument (size)");
    }
    if (args[0].type != ValueType::NUMBER) {
        throw std::runtime_error("sys.alloc() expects a number size");
    }
    
    size_t size = static_cast<size_t>(args[0].as.number);
    return Value(vm.allocate<Buffer>(size));
}

namespace neutron {
    void register_sys_functions(VM& vm, std::shared_ptr<Environment> env) {
        // Checkpoint
        env->define("checkpoint", Value(vm.allocate<NativeFn>(sys_checkpoint, 1, true)));

        // Memory Operations
        env->define("alloc", Value(vm.allocate<NativeFn>(sys_alloc, 1, true)));

        // File Operations
        env->define("read", Value(vm.allocate<NativeFn>(sys_read, 1, true)));
        env->define("write", Value(vm.allocate<NativeFn>(sys_write, 2, true)));
        env->define("append", Value(vm.allocate<NativeFn>(sys_append, 2, true)));
        env->define("cp", Value(vm.allocate<NativeFn>(sys_cp, 2, true)));
        env->define("mv", Value(vm.allocate<NativeFn>(sys_mv, 2, true)));
        env->define("rm", Value(vm.allocate<NativeFn>(sys_rm, 1, true)));
        env->define("exists", Value(vm.allocate<NativeFn>(sys_exists, 1, true)));
        
        // Directory Operations
        env->define("mkdir", Value(vm.allocate<NativeFn>(sys_mkdir, 1, true)));
        env->define("rmdir", Value(vm.allocate<NativeFn>(sys_rmdir, -1, true))); // 1-2 arguments
        env->define("listdir", Value(vm.allocate<NativeFn>(sys_listdir, 1, true)));
        
        // File Info & Permissions
        env->define("stat", Value(vm.allocate<NativeFn>(sys_stat, 1, true)));
        env->define("chmod", Value(vm.allocate<NativeFn>(sys_chmod, 2, true)));
        env->define("tmpfile", Value(vm.allocate<NativeFn>(sys_tmpfile, 0, true)));
        
        // System Information
        env->define("cwd", Value(vm.allocate<NativeFn>(sys_cwd, 0, true)));
        env->define("chdir", Value(vm.allocate<NativeFn>(sys_chdir, 1, true)));
        env->define("env", Value(vm.allocate<NativeFn>(sys_env, 1, true)));
        env->define("args", Value(vm.allocate<NativeFn>(sys_args, 0, true)));  // true = needs VM
        env->define("info", Value(vm.allocate<NativeFn>(sys_info, 0, true)));
        
        // User Input
        env->define("input", Value(vm.allocate<NativeFn>(sys_input, -1, true)));
        env->define("sleep", Value(vm.allocate<NativeFn>(sys_sleep, 1, true)));
        
        // Process Control
        env->define("exit", Value(vm.allocate<NativeFn>(sys_exit, -1, true)));
        env->define("exec", Value(vm.allocate<NativeFn>(sys_exec, 1, true)));
    }
}

extern "C" void neutron_init_sys_module(VM* vm) {
    auto sys_env = std::make_shared<neutron::Environment>();
    neutron::register_sys_functions(*vm, sys_env);
    auto sys_module = vm->allocate<neutron::Module>("sys", sys_env);
    vm->define_module("sys", sys_module);
}
