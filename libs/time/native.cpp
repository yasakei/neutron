#include "libs/time/native.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

namespace neutron {

// Get current timestamp
Value time_now(std::vector<Value> arguments) {
    if (arguments.size() != 0) {
        throw std::runtime_error("Expected 0 arguments for time.now().");
    }
    
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return Value(static_cast<double>(now));
}

// Format timestamp as string
Value time_format(std::vector<Value> arguments) {
    if (arguments.size() < 1 || arguments.size() > 2) {
        throw std::runtime_error("Expected 1-2 arguments for time.format().");
    }
    
    if (arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("First argument for time.format() must be a number (timestamp).");
    }
    
    // Get the timestamp
    double timestamp = arguments[0].as.number;
    
    // Default format or custom format
    std::string format = "%Y-%m-%d %H:%M:%S";
    if (arguments.size() == 2) {
        if (arguments[1].type != ValueType::STRING) {
            throw std::runtime_error("Second argument for time.format() must be a string (format).");
        }
        format = *arguments[1].as.string;
    }
    
    // Convert timestamp to time_t
    std::time_t t = static_cast<std::time_t>(timestamp / 1000); // Assuming milliseconds
    
    // Format the time
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), format.c_str());
    
    return Value(oss.str());
}

// Sleep for specified milliseconds
Value time_sleep(std::vector<Value> arguments) {
    if (arguments.size() != 1) {
        throw std::runtime_error("Expected 1 argument for time.sleep().");
    }
    
    if (arguments[0].type != ValueType::NUMBER) {
        throw std::runtime_error("Argument for time.sleep() must be a number (milliseconds).");
    }
    
    int milliseconds = static_cast<int>(arguments[0].as.number);
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    
    return Value(nullptr);
}

// Register time functions in the environment
void register_time_functions(std::shared_ptr<Environment> env) {
    // Create a time module
    auto timeEnv = std::make_shared<Environment>();
    timeEnv->define("now", Value(new NativeFn(time_now, 0)));
    timeEnv->define("format", Value(new NativeFn(time_format, -1))); // -1 for variable arguments
    timeEnv->define("sleep", Value(new NativeFn(time_sleep, 1)));
    
    auto timeModule = new Module("time", timeEnv, std::vector<std::unique_ptr<Stmt>>());
    env->define("time", Value(timeModule));
}

} // namespace neutron