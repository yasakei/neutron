/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 *
 * coro module — cooperative fibers (lightweight coroutines).
 *
 * API:
 *   use coro;
 *   var fiber f = coro.create(func, ...args);  // CREATED
 *   var fiber f = coro.spawn(func, ...args);   // CREATED + first resume
 *   var any v = coro.resume(f, ...args);       // run until yield/return
 *   coro.yield(v);                             // yield from inside fiber
 *   var any r = coro.join(f);                  // run to completion
 *   var string s = coro.status(f);             // created|running|suspended|finished
 *   coro.sleep(ms);                            // cooperative sleep
 *   coro.schedule();                           // round-robin all fibers to completion
 *   var int n = coro.count();                  // number of fibers in VM
 *   var array results = coro.all([f1, f2]);    // join multiple, return results
 */
#include "native.h"
#include "vm.h"
#include "types/obj_string.h"
#include <chrono>
#include <thread>

using namespace neutron;

static Value coro_create(VM& vm, std::vector<Value> args) {
    if (args.empty()) {
        throw std::runtime_error("coro.create() requires a function argument");
    }
    if (args[0].type != ValueType::CALLABLE) {
        throw std::runtime_error("coro.create() first argument must be a function");
    }
    Function* func = dynamic_cast<Function*>(args[0].as.callable);
    if (!func) {
        throw std::runtime_error("coro.create() requires a user-defined function (not native)");
    }
    std::vector<Value> fargs(args.begin() + 1, args.end());
    std::string fname = func->name.empty() ? "<fiber>" : func->name;
    Fiber* fiber = vm.createFiber(func, fargs, fname);
    return Value(fiber);
}

static Value coro_spawn(VM& vm, std::vector<Value> args) {
    if (args.empty()) {
        throw std::runtime_error("coro.spawn() requires a function argument");
    }
    if (args[0].type != ValueType::CALLABLE) {
        throw std::runtime_error("coro.spawn() first argument must be a function");
    }
    Function* func = dynamic_cast<Function*>(args[0].as.callable);
    if (!func) {
        throw std::runtime_error("coro.spawn() requires a user-defined function (not native)");
    }
    std::vector<Value> fargs(args.begin() + 1, args.end());
    std::string fname = func->name.empty() ? "<fiber>" : func->name;
    Fiber* fiber = vm.createFiber(func, fargs, fname);
    vm.resumeFiber(fiber, fargs);
    return Value(fiber);
}

static Value coro_resume(VM& vm, std::vector<Value> args) {
    if (args.empty() || args[0].type != ValueType::FIBER) {
        throw std::runtime_error("coro.resume() requires a fiber argument");
    }
    Fiber* fiber = args[0].as.fiber;
    if (!fiber) throw std::runtime_error("coro.resume() got null fiber");
    std::vector<Value> fargs(args.begin() + 1, args.end());
    return vm.resumeFiber(fiber, fargs);
}

static Value coro_yield(VM& vm, std::vector<Value> args) {
    Value v = args.empty() ? Value() : args[0];
    if (!vm.currentFiber) {
        return v;
    }
    size_t toPop = args.size() + 1;
    for (size_t i = 0; i < toPop && !vm.stack.empty(); ++i) {
        vm.stack.pop_back();
    }
    vm.yieldFiber(v);
    return v; // unreachable
}

static Value coro_join(VM& vm, std::vector<Value> args) {
    if (args.empty() || args[0].type != ValueType::FIBER) {
        throw std::runtime_error("coro.join() requires a fiber argument");
    }
    Fiber* fiber = args[0].as.fiber;
    if (!fiber) throw std::runtime_error("coro.join() got null fiber");
    Value last;
    int guard = 0;
    while (fiber->state != Fiber::State::FINISHED && guard++ < 1000000) {
        last = vm.resumeFiber(fiber);
    }
    if (fiber->state == Fiber::State::FINISHED) return fiber->returnValue;
    return last;
}

static Value coro_status(VM& vm, std::vector<Value> args) {
    if (args.empty() || args[0].type != ValueType::FIBER) {
        throw std::runtime_error("coro.status() requires a fiber argument");
    }
    Fiber* fiber = args[0].as.fiber;
    if (!fiber) throw std::runtime_error("coro.status() got null fiber");
    return Value(vm.makeString(fiber->getStateString()));
}

static Value coro_sleep(VM& vm, std::vector<Value> args) {
    (void)vm;
    double ms = 0;
    if (!args.empty() && args[0].type == ValueType::NUMBER) {
        ms = args[0].as.number;
    }
    if (ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<uint32_t>(ms)));
    }
    return Value();
}

static Value coro_schedule(VM& vm, std::vector<Value> args) {
    (void)args;
    vm.runFiberScheduler();
    return Value();
}

static Value coro_count(VM& vm, std::vector<Value> args) {
    (void)args;
    return Value(static_cast<double>(vm.fibers.size()));
}

static Value coro_all(VM& vm, std::vector<Value> args) {
    if (args.empty() || args[0].type != ValueType::ARRAY) {
        throw std::runtime_error("coro.all([fibers]) requires an array of fibers");
    }
    Array* arr = args[0].as.array;
    Array* out = vm.allocate<Array>();
    for (const Value& v : arr->elements) {
        if (v.type != ValueType::FIBER || !v.as.fiber) {
            throw std::runtime_error("coro.all() array must contain only fibers");
        }
        Fiber* fiber = v.as.fiber;
        int guard = 0;
        while (fiber->state != Fiber::State::FINISHED && guard++ < 1000000) {
            vm.resumeFiber(fiber);
        }
        out->elements.push_back(fiber->returnValue);
    }
    return Value(out);
}

namespace neutron {
void register_coro_functions(VM& vm, std::shared_ptr<Environment> env) {
    env->define("create",   Value(vm.allocate<NativeFn>(coro_create,   -1, true)));
    env->define("spawn",    Value(vm.allocate<NativeFn>(coro_spawn,    -1, true)));
    env->define("resume",   Value(vm.allocate<NativeFn>(coro_resume,   -1, true)));
    env->define("yield",    Value(vm.allocate<NativeFn>(coro_yield,    -1, true)));
    env->define("join",     Value(vm.allocate<NativeFn>(coro_join,      1, true)));
    env->define("status",   Value(vm.allocate<NativeFn>(coro_status,    1, true)));
    env->define("sleep",    Value(vm.allocate<NativeFn>(coro_sleep,     1, true)));
    env->define("schedule", Value(vm.allocate<NativeFn>(coro_schedule,  0, true)));
    env->define("count",    Value(vm.allocate<NativeFn>(coro_count,     0, true)));
    env->define("all",      Value(vm.allocate<NativeFn>(coro_all,       1, true)));
}
}

extern "C" void neutron_init_coro_module(neutron::VM* vm) {
    auto it = vm->globals.find("coro");
    if (it != vm->globals.end() && it->second.type == neutron::ValueType::MODULE) {
        // Refresh existing module env (VM constructor pre-creates it)
        neutron::Module* mod = it->second.as.module;
        if (mod && mod->env) {
            neutron::register_coro_functions(*vm, mod->env);
            vm->loadedModuleCache["coro"] = true;
            return;
        }
    }
    auto env = std::make_shared<neutron::Environment>();
    neutron::register_coro_functions(*vm, env);
    auto mod = vm->allocate<neutron::Module>("coro", env);
    vm->define_module("coro", mod);
}
