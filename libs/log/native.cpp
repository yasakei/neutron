/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 *
 * log module — structured leveled logging
 *
 * Levels: DEBUG=0, INFO=1, WARN=2, ERROR=3
 * Default level: INFO (1)
 * Default output: stdout. Call log.set_file(path) to also write to a file.
 */
#include "native.h"
#include "vm.h"
#include "types/obj_string.h"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using namespace neutron;

// ── module state ──────────────────────────────────────────────────────────────
static int  g_level    = 1;          // INFO
static bool g_color    = true;
static bool g_timestamp = true;
static std::string g_logfile;        // empty = no file output

static const char* LEVEL_NAMES[] = { "DEBUG", "INFO", "WARN", "ERROR" };
static const char* LEVEL_COLORS[] = { "\033[36m", "\033[32m", "\033[33m", "\033[31m" };
static const char* RESET = "\033[0m";

// ── helpers ───────────────────────────────────────────────────────────────────
static std::string now_str() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

static void emit(int level, const std::string& msg) {
    if (level < g_level) return;

    std::string line;
    if (g_timestamp) line += "[" + now_str() + "] ";
    line += "[" + std::string(LEVEL_NAMES[level]) + "] ";
    line += msg;

    // stdout (with optional color)
    if (g_color && level >= 0 && level <= 3)
        std::cout << LEVEL_COLORS[level] << line << RESET << "\n";
    else
        std::cout << line << "\n";

    // file (no color codes)
    if (!g_logfile.empty()) {
        std::ofstream f(g_logfile, std::ios::app);
        if (f.is_open()) f << line << "\n";
    }
}

static std::string valToStr(const Value& v) {
    return v.toString();
}

// ── log.debug / info / warn / error ──────────────────────────────────────────
static Value log_at(int level, VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.empty()) throw std::runtime_error("log function expects at least 1 argument.");
    std::string msg;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) msg += " ";
        msg += valToStr(args[i]);
    }
    emit(level, msg);
    return Value();
}

Value log_debug(VM& vm, std::vector<Value> args) { return log_at(0, vm, args); }
Value log_info (VM& vm, std::vector<Value> args) { return log_at(1, vm, args); }
Value log_warn (VM& vm, std::vector<Value> args) { return log_at(2, vm, args); }
Value log_error(VM& vm, std::vector<Value> args) { return log_at(3, vm, args); }

// ── log.set_level(n) ─────────────────────────────────────────────────────────
Value log_set_level(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1 || args[0].type != ValueType::NUMBER)
        throw std::runtime_error("log.set_level(n) expects a number (0=DEBUG,1=INFO,2=WARN,3=ERROR).");
    int lvl = static_cast<int>(args[0].as.number);
    if (lvl < 0 || lvl > 3) throw std::runtime_error("log.set_level: level must be 0-3.");
    g_level = lvl;
    return Value();
}

// ── log.get_level() → number ──────────────────────────────────────────────────
Value log_get_level(VM& vm, std::vector<Value> args) {
    (void)vm; (void)args;
    return Value(static_cast<double>(g_level));
}

// ── log.set_file(path) ────────────────────────────────────────────────────────
Value log_set_file(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1 || args[0].type != ValueType::OBJ_STRING)
        throw std::runtime_error("log.set_file(path) expects a string path.");
    g_logfile = args[0].asString()->chars;
    return Value();
}

// ── log.set_color(bool) ───────────────────────────────────────────────────────
Value log_set_color(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1 || args[0].type != ValueType::BOOLEAN)
        throw std::runtime_error("log.set_color(bool) expects a boolean.");
    g_color = args[0].as.boolean;
    return Value();
}

// ── log.set_timestamp(bool) ───────────────────────────────────────────────────
Value log_set_timestamp(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1 || args[0].type != ValueType::BOOLEAN)
        throw std::runtime_error("log.set_timestamp(bool) expects a boolean.");
    g_timestamp = args[0].as.boolean;
    return Value();
}

// ── constants: log.DEBUG / INFO / WARN / ERROR ────────────────────────────────

// ── registration ──────────────────────────────────────────────────────────────
namespace neutron {
    void register_log_functions(VM& vm, std::shared_ptr<Environment> env) {
        env->define("debug",         Value(vm.allocate<NativeFn>(log_debug,         -1, true)));
        env->define("info",          Value(vm.allocate<NativeFn>(log_info,          -1, true)));
        env->define("warn",          Value(vm.allocate<NativeFn>(log_warn,          -1, true)));
        env->define("error",         Value(vm.allocate<NativeFn>(log_error,         -1, true)));
        env->define("set_level",     Value(vm.allocate<NativeFn>(log_set_level,      1, true)));
        env->define("get_level",     Value(vm.allocate<NativeFn>(log_get_level,      0, true)));
        env->define("set_file",      Value(vm.allocate<NativeFn>(log_set_file,       1, true)));
        env->define("set_color",     Value(vm.allocate<NativeFn>(log_set_color,      1, true)));
        env->define("set_timestamp", Value(vm.allocate<NativeFn>(log_set_timestamp,  1, true)));
        // Level constants
        env->define("DEBUG", Value(0.0));
        env->define("INFO",  Value(1.0));
        env->define("WARN",  Value(2.0));
        env->define("ERROR", Value(3.0));
    }
}

extern "C" void neutron_init_log_module(neutron::VM* vm) {
    auto env = std::make_shared<neutron::Environment>();
    neutron::register_log_functions(*vm, env);
    auto mod = vm->allocate<neutron::Module>("log", env);
    vm->define_module("log", mod);
}
