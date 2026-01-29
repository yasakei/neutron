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
#include "types/array.h"
#include "types/obj_string.h"
#include "types/native_fn.h"
#include "modules/module.h"
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cstdlib>

// Cross-platform includes
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define PATH_MAX MAX_PATH
    #define getcwd _getcwd
#else
    #include <unistd.h>
    #include <limits.h>
    #include <sys/stat.h>
#endif

using namespace neutron;

// Platform-specific path separators
#ifdef _WIN32
    const char PATH_SEP = '\\';
    const char ALT_PATH_SEP = '/';
    const std::string PATH_SEP_STR = "\\";
    const std::string PATH_DELIMITER = ";";
#else
    const char PATH_SEP = '/';
    const char ALT_PATH_SEP = '\\';
    const std::string PATH_SEP_STR = "/";
    const std::string PATH_DELIMITER = ":";
#endif

// Helper function to normalize path separators for current platform
std::string normalize_separators(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), ALT_PATH_SEP, PATH_SEP);
    return result;
}

// Helper function to split path into components
std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> components;
    std::string normalized = normalize_separators(path);
    std::stringstream ss(normalized);
    std::string component;
    
    while (std::getline(ss, component, PATH_SEP)) {
        if (!component.empty()) {
            components.push_back(component);
        }
    }
    
    return components;
}

// Helper function to check if path is absolute
bool is_absolute_path(const std::string& path) {
    if (path.empty()) return false;
    
#ifdef _WIN32
    // Windows: C:\ or \\server\share or \
    if (path.length() >= 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        return true;
    }
    if (path.length() >= 2 && (path[0] == '\\' || path[0] == '/')) {
        return true;
    }
#else
    // Unix: starts with /
    if (path[0] == '/') {
        return true;
    }
#endif
    
    return false;
}

// Helper function to get current working directory
std::string get_current_directory() {
    char buffer[PATH_MAX];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        return std::string(buffer);
    }
    return "";
}

// Path module functions

// path.join(...paths) - Join path components
Value path_join(VM& vm, std::vector<Value> arguments) {
    if (arguments.empty()) {
        return Value(vm.internString(""));
    }
    
    std::string result;
    
    for (size_t i = 0; i < arguments.size(); i++) {
        if (arguments[i].type != ValueType::OBJ_STRING) {
            throw std::runtime_error("All arguments to path.join() must be strings.");
        }
        
        std::string part = arguments[i].asString()->chars;
        if (part.empty()) continue;
        
        if (i == 0) {
            result = part;
        } else {
            // If this part is absolute, start over
            if (is_absolute_path(part)) {
                result = part;
            } else {
                // Add separator if needed
                if (!result.empty() && result.back() != PATH_SEP && result.back() != ALT_PATH_SEP) {
                    result += PATH_SEP;
                }
                result += part;
            }
        }
    }
    
    return Value(vm.internString(normalize_separators(result)));
}

// path.split(path) - Split into [dir, file]
Value path_split(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for path.split().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for path.split() must be a string.");
    }
    
    std::string path = normalize_separators(arguments[0].asString()->chars);
    
    size_t last_sep = path.find_last_of(PATH_SEP);
    
    auto result = vm.allocate<Array>();
    
    if (last_sep == std::string::npos) {
        // No separator found
        result->elements.push_back(Value(vm.internString("")));
        result->elements.push_back(Value(vm.internString(path)));
    } else {
        std::string dir = path.substr(0, last_sep);
        std::string file = path.substr(last_sep + 1);
        
        // Handle root directory case
        if (dir.empty()) {
            dir = PATH_SEP_STR;
        }
        
        result->elements.push_back(Value(vm.internString(dir)));
        result->elements.push_back(Value(vm.internString(file)));
    }
    
    return Value(result);
}

// path.dirname(path) - Get directory part
Value path_dirname(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for path.dirname().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for path.dirname() must be a string.");
    }
    
    std::string path = normalize_separators(arguments[0].asString()->chars);
    
    size_t last_sep = path.find_last_of(PATH_SEP);
    
    if (last_sep == std::string::npos) {
        return Value(vm.internString("."));
    }
    
    if (last_sep == 0) {
        return Value(vm.internString(PATH_SEP_STR));
    }
    
    return Value(vm.internString(path.substr(0, last_sep)));
}

// path.basename(path) - Get filename part
Value path_basename(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for path.basename().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for path.basename() must be a string.");
    }
    
    std::string path = normalize_separators(arguments[0].asString()->chars);
    
    size_t last_sep = path.find_last_of(PATH_SEP);
    
    if (last_sep == std::string::npos) {
        return Value(vm.internString(path));
    }
    
    return Value(vm.internString(path.substr(last_sep + 1)));
}

// path.extname(path) - Get file extension
Value path_extname(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for path.extname().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for path.extname() must be a string.");
    }
    
    std::string path = arguments[0].asString()->chars;
    
    // Get basename first
    size_t last_sep = path.find_last_of(PATH_SEP);
    size_t last_sep_alt = path.find_last_of(ALT_PATH_SEP);
    
    size_t sep_pos = std::string::npos;
    if (last_sep != std::string::npos && last_sep_alt != std::string::npos) {
        sep_pos = std::max(last_sep, last_sep_alt);
    } else if (last_sep != std::string::npos) {
        sep_pos = last_sep;
    } else if (last_sep_alt != std::string::npos) {
        sep_pos = last_sep_alt;
    }
    
    std::string basename = (sep_pos == std::string::npos) ? path : path.substr(sep_pos + 1);
    
    // Find last dot in basename
    size_t last_dot = basename.find_last_of('.');
    
    if (last_dot == std::string::npos || last_dot == 0) {
        return Value(vm.internString(""));
    }
    
    return Value(vm.internString(basename.substr(last_dot)));
}

// path.isabs(path) - Check if path is absolute
Value path_isabs(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for path.isabs().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for path.isabs() must be a string.");
    }
    
    std::string path = arguments[0].asString()->chars;
    return Value(is_absolute_path(path));
}

// path.normalize(path) - Clean up path
Value path_normalize(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for path.normalize().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for path.normalize() must be a string.");
    }
    
    std::string path = normalize_separators(arguments[0].asString()->chars);
    
    if (path.empty()) {
        return Value(vm.internString("."));
    }
    
    bool is_abs = is_absolute_path(path);
    std::vector<std::string> components = split_path(path);
    std::vector<std::string> normalized;
    
    for (const std::string& component : components) {
        if (component == ".") {
            // Skip current directory references
            continue;
        } else if (component == "..") {
            // Parent directory
            if (!normalized.empty() && normalized.back() != "..") {
                normalized.pop_back();
            } else if (!is_abs) {
                normalized.push_back("..");
            }
        } else {
            normalized.push_back(component);
        }
    }
    
    std::string result;
    
    if (is_abs) {
        result = PATH_SEP_STR;
        for (size_t i = 0; i < normalized.size(); i++) {
            if (i > 0) {
                result += PATH_SEP;
            }
            result += normalized[i];
        }
    } else {
        for (size_t i = 0; i < normalized.size(); i++) {
            if (i > 0) {
                result += PATH_SEP;
            }
            result += normalized[i];
        }
    }
    
    if (result.empty()) {
        result = ".";
    }
    
    return Value(vm.internString(result));
}

// path.resolve(path) - Convert to absolute path
Value path_resolve(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for path.resolve().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for path.resolve() must be a string.");
    }
    
    std::string path = arguments[0].asString()->chars;
    
    if (is_absolute_path(path)) {
        // Already absolute, just normalize
        Value norm_args[] = { arguments[0] };
        std::vector<Value> norm_vec(norm_args, norm_args + 1);
        return path_normalize(vm, norm_vec);
    }
    
    // Make relative to current directory
    std::string cwd = get_current_directory();
    if (cwd.empty()) {
        throw std::runtime_error("Could not get current working directory.");
    }
    
    Value join_args[] = { 
        Value(vm.internString(cwd)), 
        arguments[0] 
    };
    std::vector<Value> join_vec(join_args, join_args + 2);
    Value joined = path_join(vm, join_vec);
    
    std::vector<Value> norm_vec = { joined };
    return path_normalize(vm, norm_vec);
}

// path.relative(from, to) - Get relative path
Value path_relative(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 2) {
        throw std::runtime_error("Expected 2 arguments for path.relative().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING || arguments[1].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Both arguments for path.relative() must be strings.");
    }
    
    // Resolve both paths to absolute
    std::vector<Value> from_vec = { arguments[0] };
    std::vector<Value> to_vec = { arguments[1] };
    
    Value from_abs = path_resolve(vm, from_vec);
    Value to_abs = path_resolve(vm, to_vec);
    
    std::string from_path = from_abs.asString()->chars;
    std::string to_path = to_abs.asString()->chars;
    
    std::vector<std::string> from_parts = split_path(from_path);
    std::vector<std::string> to_parts = split_path(to_path);
    
    // Find common prefix
    size_t common = 0;
    size_t min_len = std::min(from_parts.size(), to_parts.size());
    
    for (size_t i = 0; i < min_len; i++) {
        if (from_parts[i] == to_parts[i]) {
            common++;
        } else {
            break;
        }
    }
    
    // Build relative path
    std::string result;
    
    // Add .. for each remaining part in from_path
    for (size_t i = common; i < from_parts.size(); i++) {
        if (!result.empty()) result += PATH_SEP;
        result += "..";
    }
    
    // Add remaining parts from to_path
    for (size_t i = common; i < to_parts.size(); i++) {
        if (!result.empty()) result += PATH_SEP;
        result += to_parts[i];
    }
    
    if (result.empty()) {
        result = ".";
    }
    
    return Value(vm.internString(result));
}

// path.sep - Platform path separator
Value path_sep(VM& vm, std::vector<Value> arguments) {
    (void)vm; // Unused
    (void)arguments; // Unused
    return Value(vm.internString(PATH_SEP_STR));
}

// path.delimiter - Platform path delimiter
Value path_delimiter(VM& vm, std::vector<Value> arguments) {
    (void)vm; // Unused
    (void)arguments; // Unused
    return Value(vm.internString(PATH_DELIMITER));
}

// path.toUnix(path) - Convert to Unix-style path
Value path_toUnix(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for path.toUnix().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for path.toUnix() must be a string.");
    }
    
    std::string path = arguments[0].asString()->chars;
    std::replace(path.begin(), path.end(), '\\', '/');
    
    return Value(vm.internString(path));
}

// path.toWindows(path) - Convert to Windows-style path
Value path_toWindows(VM& vm, std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for path.toWindows().");
    }
    
    if (arguments[0].type != ValueType::OBJ_STRING) {
        throw std::runtime_error("Argument for path.toWindows() must be a string.");
    }
    
    std::string path = arguments[0].asString()->chars;
    std::replace(path.begin(), path.end(), '/', '\\');
    
    return Value(vm.internString(path));
}

namespace neutron {
    void register_path_functions(VM& vm, std::shared_ptr<Environment> env) {
        // Core path operations
        env->define("join", Value(vm.allocate<NativeFn>(path_join, -1, true)));
        env->define("split", Value(vm.allocate<NativeFn>(path_split, 1, true)));
        env->define("dirname", Value(vm.allocate<NativeFn>(path_dirname, 1, true)));
        env->define("basename", Value(vm.allocate<NativeFn>(path_basename, 1, true)));
        env->define("extname", Value(vm.allocate<NativeFn>(path_extname, 1, true)));
        
        // Path analysis
        env->define("isabs", Value(vm.allocate<NativeFn>(path_isabs, 1, true)));
        env->define("normalize", Value(vm.allocate<NativeFn>(path_normalize, 1, true)));
        env->define("resolve", Value(vm.allocate<NativeFn>(path_resolve, 1, true)));
        env->define("relative", Value(vm.allocate<NativeFn>(path_relative, 2, true)));
        
        // Platform constants
        env->define("sep", Value(vm.internString(PATH_SEP_STR)));
        env->define("delimiter", Value(vm.internString(PATH_DELIMITER)));
        
        // Cross-platform utilities
        env->define("toUnix", Value(vm.allocate<NativeFn>(path_toUnix, 1, true)));
        env->define("toWindows", Value(vm.allocate<NativeFn>(path_toWindows, 1, true)));
    }
}

extern "C" void neutron_init_path_module(VM* vm) {
    auto path_env = std::make_shared<neutron::Environment>();
    neutron::register_path_functions(*vm, path_env);
    auto path_module = vm->allocate<neutron::Module>("path", path_env);
    vm->define_module("path", path_module);
}