/*
 * Neutron Programming Language
 * Copyright (c) 2025 yasakei
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
#include "runtime/environment.h"
#include "types/value.h"
#include "types/json_object.h"
#include "types/array.h"
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
#include <iomanip>

using namespace neutron;
namespace fs = std::filesystem;

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
    
    if (!platform::copyFile(source, destination)) {
        throw std::runtime_error("Failed to copy file from " + source + " to " + destination);
    }
    
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
    
    if (!platform::moveFile(source, destination)) {
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
    
    if (!platform::removeFile(filepath)) {
        if (!platform::fileExists(filepath)) {
            return Value(false); // File doesn't exist
        }
        throw std::runtime_error("Failed to remove file: " + filepath);
    }
    
    return Value(true);
}

static Value sys_exists(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.exists() expects 1 argument (path)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.exists() expects a string path");
    }
    
    std::string path = std::get<std::string>(args[0].as);
    return Value(platform::fileExists(path));
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
    
    if (!platform::createDirectory(path)) {
        throw std::runtime_error("Failed to create directory: " + path);
    }
    
    return Value(true);
}

static Value sys_rmdir(const std::vector<Value>& args) {
    if (args.size() < 1 || args.size() > 2) {
        throw std::runtime_error("sys.rmdir() expects 1 or 2 arguments (path, optional recursive)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.rmdir() expects a string path");
    }
    
    std::string pathStr = std::get<std::string>(args[0].as);
    bool recursive = false;
    
    // Check if recursive flag is provided
    if (args.size() == 2) {
        if (args[1].type != ValueType::BOOLEAN) {
            throw std::runtime_error("sys.rmdir() recursive flag must be a boolean");
        }
        recursive = std::get<bool>(args[1].as);
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

static Value sys_listdir(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.listdir() expects 1 argument (path)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.listdir() expects a string path");
    }
    
    std::string path = std::get<std::string>(args[0].as);
    std::vector<std::string> files = platform::listDirectory(path);
    
    auto array = new Array();
    for (const auto& file : files) {
        array->elements.push_back(Value(file));
    }
    return Value(array);
}

static Value sys_stat(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.stat() expects 1 argument (path)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.stat() expects a string path");
    }
    
    std::string pathStr = std::get<std::string>(args[0].as);
    fs::path path(pathStr);
    
    if (!fs::exists(path)) {
        throw std::runtime_error("File not found: " + pathStr);
    }
    
    auto info = new JsonObject();
    info->properties["size"] = Value(static_cast<double>(fs::file_size(path)));
    
    // Handle time conversion safely
    auto ftime = fs::last_write_time(path);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
    );
    std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&cftime), "%Y-%m-%d %H:%M:%S");
    info->properties["mtime"] = Value(ss.str());
    
    info->properties["is_directory"] = Value(fs::is_directory(path));
    info->properties["is_file"] = Value(fs::is_regular_file(path));
    
    return Value(info);
}

static Value sys_chmod(const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw std::runtime_error("sys.chmod() expects 2 arguments (path, mode)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.chmod() expects a string path");
    }
    if (args[1].type != ValueType::NUMBER) {
        throw std::runtime_error("sys.chmod() expects a number mode");
    }
    
    std::string pathStr = std::get<std::string>(args[0].as);
    int mode = static_cast<int>(std::get<double>(args[1].as));
    
    // Cast integer mode to permissions
    fs::permissions(pathStr, static_cast<fs::perms>(mode), fs::perm_options::replace);
    
    return Value(true);
}

static Value sys_tmpfile(const std::vector<Value>& args) {
    if (args.size() != 0) {
        throw std::runtime_error("sys.tmpfile() expects 0 arguments");
    }
    
    auto tmp_dir = fs::temp_directory_path();
    
    // Generate a unique name using timestamp and random number
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    int rand_val = std::rand();
    std::string filename = "neutron_tmp_" + std::to_string(now) + "_" + std::to_string(rand_val);
    fs::path tmp_path = tmp_dir / filename;
    
    return Value(tmp_path.string());
}

// System Information

static Value sys_cwd(const std::vector<Value>& args) {
    if (args.size() != 0) {
        throw std::runtime_error("sys.cwd() expects 0 arguments");
    }
    
    std::string cwd = platform::getCwd();
    if (cwd.empty()) {
        throw std::runtime_error("Failed to get current working directory");
    }
    
    return Value(cwd);
}

static Value sys_chdir(const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw std::runtime_error("sys.chdir() expects 1 argument (path)");
    }
    if (args[0].type != ValueType::STRING) {
        throw std::runtime_error("sys.chdir() expects a string path");
    }
    
    std::string path = std::get<std::string>(args[0].as);
    
    if (!platform::setCwd(path)) {
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
    std::string value = platform::getEnv(varname);
    
    if (value.empty()) {
        return Value(); // Return nil
    }
    
    return Value(value);
}

static Value sys_args(VM& vm, const std::vector<Value>& args) {
    if (args.size() != 0) {
        throw std::runtime_error("sys.args() expects 0 arguments");
    }
    
    // Return command line arguments from VM
    auto array = new Array();
    for (const auto& arg : vm.commandLineArgs) {
        array->elements.push_back(Value(arg));
    }
    return Value(array);
}

static Value sys_info(const std::vector<Value>& args) {
    if (args.size() != 0) {
        throw std::runtime_error("sys.info() expects 0 arguments");
    }
    
    auto info = new JsonObject();
    
    // Use platform abstraction for system info
    info->properties["platform"] = Value(platform::getPlatform());
    info->properties["arch"] = Value(platform::getArch());
    info->properties["user"] = Value(platform::getUsername());
    info->properties["hostname"] = Value(platform::getHostname());
    info->properties["cwd"] = Value(platform::getCwd());
    
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
    
    platform::exitProcess(exit_code);
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
    int exitCode = platform::execute(command);
    
    return Value(static_cast<double>(exitCode));
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
        env->define("rmdir", Value(new NativeFn(sys_rmdir, -1))); // 1-2 arguments
        env->define("listdir", Value(new NativeFn(sys_listdir, 1)));
        
        // File Info & Permissions
        env->define("stat", Value(new NativeFn(sys_stat, 1)));
        env->define("chmod", Value(new NativeFn(sys_chmod, 2)));
        env->define("tmpfile", Value(new NativeFn(sys_tmpfile, 0)));
        
        // System Information
        env->define("cwd", Value(new NativeFn(sys_cwd, 0)));
        env->define("chdir", Value(new NativeFn(sys_chdir, 1)));
        env->define("env", Value(new NativeFn(sys_env, 1)));
        env->define("args", Value(new NativeFn(sys_args, 0, true)));  // true = needs VM
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