#include "native.h"
#include "vm.h"
#include <chrono>

using namespace neutron;

Value time_now(std::vector<Value> arguments) {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return Value(static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()));
}

void register_time_lib(std::shared_ptr<Environment> env) {
    std::shared_ptr<Environment> time_env = std::make_shared<Environment>();

    NativeFn* now_fn = new NativeFn(time_now, 0);
    time_env->define("now", Value(now_fn));

    Module* time_module = new Module("time", time_env, {});
    env->define("time", Value(time_module));
}
