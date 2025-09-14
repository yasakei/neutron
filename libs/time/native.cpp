#include "libs/time/native.h"
#include "libs/time/time_ops.h"
#include <string>
#include <vector>

namespace neutron {

// Get current timestamp
Value time_now(std::vector<Value> arguments) {
    if (arguments.size() != 0) {
        throw std::runtime_error("Expected 0 arguments for time.now().");
    }
    return Value(time_now_c());
}

// Format timestamp as string
Value time_format(std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::runtime_error("Expected 1-2 arguments for time.format().");
    }
    if (arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("First argument for time.format() must be a number (timestamp).");
    }
    const char* format = (arguments.size() == 2 && arguments[1].type == ValueType::STRING) ? arguments[1].as.string->c_str() : "%Y-%m-%d %H:%M:%S";
    char* formatted_time = time_format_c(arguments[0].as.number, format);
    Value result = Value(std::string(formatted_time));
    free(formatted_time);
    return result;
}

// Sleep for specified milliseconds
Value time_sleep(std::vector<Value> arguments) {
    if (arguments.size() != 1 || arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Expected 1 numeric argument for time.sleep().");
    }
    time_sleep_c((int)arguments[0].as.number);
    return Value(nullptr);
}

// Register time functions in the environment
void register_time_functions(std::shared_ptr<Environment> env) {
    auto timeEnv = std::make_shared<Environment>();
    timeEnv->define("now", Value(new NativeFn(time_now, 0)));
    timeEnv->define("format", Value(new NativeFn(time_format, -1)));
    timeEnv->define("sleep", Value(new NativeFn(time_sleep, 1)));
    auto timeModule = new Module("time", timeEnv, {});
    env->define("time", Value(timeModule));
}

} // namespace neutron
