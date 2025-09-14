#include "libs/sys/native.h"
#include "libs/sys/sys_ops.h"
#include <string>
#include <vector>

namespace neutron {

// File operations
Value sys_cp(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::STRING || arguments[1].type != ValueType::STRING) {
        throw std::runtime_error("Expected 2 string arguments for sys.cp().");
    }
    return Value(sys_cp_c(arguments[0].as.string->c_str(), arguments[1].as.string->c_str()));
}

Value sys_mv(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::STRING || arguments[1].type != ValueType::STRING) {
        throw std::runtime_error("Expected 2 string arguments for sys.mv().");
    }
    return Value(sys_mv_c(arguments[0].as.string->c_str(), arguments[1].as.string->c_str()));
}

Value sys_rm(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected 1 string argument for sys.rm().");
    }
    return Value(sys_rm_c(arguments[0].as.string->c_str()));
}

Value sys_mkdir(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected 1 string argument for sys.mkdir().");
    }
    return Value(sys_mkdir_c(arguments[0].as.string->c_str()));
}

Value sys_rmdir(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected 1 string argument for sys.rmdir().");
    }
    return Value(sys_rmdir_c(arguments[0].as.string->c_str()));
}

Value sys_exists(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected 1 string argument for sys.exists().");
    }
    return Value(sys_exists_c(arguments[0].as.string->c_str()));
}

Value sys_read(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected 1 string argument for sys.read().");
    }
    char* content = sys_read_c(arguments[0].as.string->c_str());
    if (content) {
        Value result = Value(std::string(content));
        free(content);
        return result;
    }
    return Value(nullptr);
}

Value sys_write(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::STRING || arguments[1].type != ValueType::STRING) {
        throw std::runtime_error("Expected 2 string arguments for sys.write().");
    }
    return Value(sys_write_c(arguments[0].as.string->c_str(), arguments[1].as.string->c_str()));
}

Value sys_append(std::vector<Value> arguments) {
    if (arguments.size() != 2 || arguments[0].type != ValueType::STRING || arguments[1].type != ValueType::STRING) {
        throw std::runtime_error("Expected 2 string arguments for sys.append().");
    }
    return Value(sys_append_c(arguments[0].as.string->c_str(), arguments[1].as.string->c_str()));
}

// System info
Value sys_cwd(std::vector<Value> arguments) {
    if (arguments.size() != 0) {
        throw std::runtime_error("Expected 0 arguments for sys.cwd().");
    }
    char* cwd = sys_cwd_c();
    if (cwd) {
        Value result = Value(std::string(cwd));
        free(cwd);
        return result;
    }
    return Value(nullptr);
}

Value sys_chdir(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected 1 string argument for sys.chdir().");
    }
    return Value(sys_chdir_c(arguments[0].as.string->c_str()));
}

Value sys_env(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected 1 string argument for sys.env().");
    }
    char* value = sys_env_c(arguments[0].as.string->c_str());
    if (value) {
        return Value(std::string(value));
    }
    return Value(nullptr);
}

Value sys_info(std::vector<Value> arguments) {
    if (arguments.size() != 0) {
        throw std::runtime_error("Expected 0 arguments for sys.info().");
    }
    char* info = sys_info_c();
    if (info) {
        Value result = Value(std::string(info));
        free(info);
        return result;
    }
    return Value(nullptr);
}

Value sys_input(std::vector<Value> arguments) {
    if (arguments.size() > 1 || (arguments.size() == 1 && arguments[0].type != ValueType::STRING)) {
        throw std::runtime_error("Expected 0 or 1 string argument for sys.input().");
    }
    const char* prompt = arguments.size() == 1 ? arguments[0].as.string->c_str() : "";
    char* line = sys_input_c(prompt);
    if (line) {
        Value result = Value(std::string(line));
        free(line);
        return result;
    }
    return Value(std::string(""));
}

// Process control
Value sys_exit(std::vector<Value> arguments) {
    if (arguments.size() > 1 || (arguments.size() == 1 && arguments[0].type != ValueType::NUMBER)) {
        throw std::runtime_error("Expected 0 or 1 numeric argument for sys.exit().");
    }
    int code = arguments.size() == 1 ? (int)arguments[0].as.number : 0;
    sys_exit_c(code);
    return Value(nullptr); // Should not be reached
}

Value sys_exec(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::STRING) {
        throw std::runtime_error("Expected 1 string argument for sys.exec().");
    }
    ExecResult* c_result = sys_exec_c(arguments[0].as.string->c_str());
    if (c_result) {
        auto obj = new JsonObject();
        obj->properties["output"] = Value(std::string(c_result->output));
        obj->properties["exit_code"] = Value((double)c_result->exit_code);
        free_exec_result(c_result);
        return Value(obj);
    }
    return Value(nullptr);
}

void register_sys_functions(std::shared_ptr<Environment> env) {
    auto sysEnv = std::make_shared<Environment>();
    sysEnv->define("cp", Value(new NativeFn(sys_cp, 2)));
    sysEnv->define("mv", Value(new NativeFn(sys_mv, 2)));
    sysEnv->define("rm", Value(new NativeFn(sys_rm, 1)));
    sysEnv->define("mkdir", Value(new NativeFn(sys_mkdir, 1)));
    sysEnv->define("rmdir", Value(new NativeFn(sys_rmdir, 1)));
    sysEnv->define("exists", Value(new NativeFn(sys_exists, 1)));
    sysEnv->define("read", Value(new NativeFn(sys_read, 1)));
    sysEnv->define("write", Value(new NativeFn(sys_write, 2)));
    sysEnv->define("append", Value(new NativeFn(sys_append, 2)));
    sysEnv->define("cwd", Value(new NativeFn(sys_cwd, 0)));
    sysEnv->define("chdir", Value(new NativeFn(sys_chdir, 1)));
    sysEnv->define("env", Value(new NativeFn(sys_env, 1)));
    sysEnv->define("info", Value(new NativeFn(sys_info, 0)));
    sysEnv->define("input", Value(new NativeFn(sys_input, -1)));
    sysEnv->define("exit", Value(new NativeFn(sys_exit, -1)));
    sysEnv->define("exec", Value(new NativeFn(sys_exec, 1)));
    auto sysModule = new Module("sys", sysEnv, {});
    env->define("sys", Value(sysModule));
}

} // namespace neutron
