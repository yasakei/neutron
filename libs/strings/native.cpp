/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 *
 * strings module — string manipulation utilities
 */
#include "native.h"
#include "vm.h"
#include "types/obj_string.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

using namespace neutron;

// ── helpers ──────────────────────────────────────────────────────────────────

static std::string getStr(const Value& v, const char* fn) {
    if (v.type != ValueType::OBJ_STRING)
        throw std::runtime_error(std::string(fn) + ": expected string argument.");
    return v.asString()->chars;
}

static double getNum(const Value& v, const char* fn) {
    if (v.type != ValueType::NUMBER)
        throw std::runtime_error(std::string(fn) + ": expected number argument.");
    return v.as.number;
}

// ── split(str, delimiter) → array ────────────────────────────────────────────
Value strings_split(VM& vm, std::vector<Value> args) {
    if (args.size() < 1 || args.size() > 2)
        throw std::runtime_error("strings.split(str, delim?) expects 1-2 arguments.");
    std::string s = getStr(args[0], "strings.split");
    std::string delim = (args.size() == 2) ? getStr(args[1], "strings.split") : " ";

    auto* arr = vm.allocate<Array>();
    if (delim.empty()) {
        // Split into individual characters
        for (char c : s)
            arr->push(Value(vm.internString(std::string(1, c))));
        return Value(arr);
    }
    size_t pos = 0, found;
    while ((found = s.find(delim, pos)) != std::string::npos) {
        arr->push(Value(vm.internString(s.substr(pos, found - pos))));
        pos = found + delim.size();
    }
    arr->push(Value(vm.internString(s.substr(pos))));
    return Value(arr);
}

// ── join(array, delimiter) → string ──────────────────────────────────────────
Value strings_join(VM& vm, std::vector<Value> args) {
    if (args.size() < 1 || args.size() > 2 || args[0].type != ValueType::ARRAY)
        throw std::runtime_error("strings.join(array, delim?) expects array and optional delimiter.");
    std::string delim = (args.size() == 2) ? getStr(args[1], "strings.join") : "";
    auto* arr = args[0].as.array;
    std::string result;
    for (size_t i = 0; i < arr->size(); i++) {
        if (i > 0) result += delim;
        result += arr->at(i).toString();
    }
    return Value(vm.internString(result));
}

// ── trim(str) → string ───────────────────────────────────────────────────────
Value strings_trim(VM& vm, std::vector<Value> args) {
    if (args.size() != 1) throw std::runtime_error("strings.trim() expects 1 argument.");
    std::string s = getStr(args[0], "strings.trim");
    size_t l = s.find_first_not_of(" \t\r\n");
    size_t r = s.find_last_not_of(" \t\r\n");
    if (l == std::string::npos) return Value(vm.internString(""));
    return Value(vm.internString(s.substr(l, r - l + 1)));
}

// ── trim_left(str) / trim_right(str) ─────────────────────────────────────────
Value strings_trim_left(VM& vm, std::vector<Value> args) {
    if (args.size() != 1) throw std::runtime_error("strings.trim_left() expects 1 argument.");
    std::string s = getStr(args[0], "strings.trim_left");
    size_t l = s.find_first_not_of(" \t\r\n");
    return Value(vm.internString(l == std::string::npos ? "" : s.substr(l)));
}

Value strings_trim_right(VM& vm, std::vector<Value> args) {
    if (args.size() != 1) throw std::runtime_error("strings.trim_right() expects 1 argument.");
    std::string s = getStr(args[0], "strings.trim_right");
    size_t r = s.find_last_not_of(" \t\r\n");
    return Value(vm.internString(r == std::string::npos ? "" : s.substr(0, r + 1)));
}

// ── upper(str) / lower(str) ───────────────────────────────────────────────────
Value strings_upper(VM& vm, std::vector<Value> args) {
    if (args.size() != 1) throw std::runtime_error("strings.upper() expects 1 argument.");
    std::string s = getStr(args[0], "strings.upper");
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return Value(vm.internString(s));
}

Value strings_lower(VM& vm, std::vector<Value> args) {
    if (args.size() != 1) throw std::runtime_error("strings.lower() expects 1 argument.");
    std::string s = getStr(args[0], "strings.lower");
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return Value(vm.internString(s));
}

// ── replace(str, from, to) → string ──────────────────────────────────────────
Value strings_replace(VM& vm, std::vector<Value> args) {
    if (args.size() != 3)
        throw std::runtime_error("strings.replace(str, from, to) expects 3 arguments.");
    std::string s    = getStr(args[0], "strings.replace");
    std::string from = getStr(args[1], "strings.replace");
    std::string to   = getStr(args[2], "strings.replace");
    if (from.empty()) return Value(vm.internString(s));
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return Value(vm.internString(s));
}

// ── replace_first(str, from, to) → string ────────────────────────────────────
Value strings_replace_first(VM& vm, std::vector<Value> args) {
    if (args.size() != 3)
        throw std::runtime_error("strings.replace_first(str, from, to) expects 3 arguments.");
    std::string s    = getStr(args[0], "strings.replace_first");
    std::string from = getStr(args[1], "strings.replace_first");
    std::string to   = getStr(args[2], "strings.replace_first");
    size_t pos = s.find(from);
    if (pos != std::string::npos) s.replace(pos, from.size(), to);
    return Value(vm.internString(s));
}

// ── contains(str, substr) → bool ─────────────────────────────────────────────
Value strings_contains(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) throw std::runtime_error("strings.contains(str, substr) expects 2 arguments.");
    std::string s = getStr(args[0], "strings.contains");
    std::string sub = getStr(args[1], "strings.contains");
    return Value(s.find(sub) != std::string::npos);
}

// ── starts_with(str, prefix) → bool ──────────────────────────────────────────
Value strings_starts_with(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) throw std::runtime_error("strings.starts_with(str, prefix) expects 2 arguments.");
    std::string s   = getStr(args[0], "strings.starts_with");
    std::string pre = getStr(args[1], "strings.starts_with");
    return Value(s.size() >= pre.size() && s.substr(0, pre.size()) == pre);
}

// ── ends_with(str, suffix) → bool ────────────────────────────────────────────
Value strings_ends_with(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) throw std::runtime_error("strings.ends_with(str, suffix) expects 2 arguments.");
    std::string s   = getStr(args[0], "strings.ends_with");
    std::string suf = getStr(args[1], "strings.ends_with");
    return Value(s.size() >= suf.size() && s.substr(s.size() - suf.size()) == suf);
}

// ── index_of(str, substr, start?) → number ───────────────────────────────────
Value strings_index_of(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() < 2 || args.size() > 3)
        throw std::runtime_error("strings.index_of(str, substr, start?) expects 2-3 arguments.");
    std::string s   = getStr(args[0], "strings.index_of");
    std::string sub = getStr(args[1], "strings.index_of");
    size_t start = (args.size() == 3) ? static_cast<size_t>(getNum(args[2], "strings.index_of")) : 0;
    size_t pos = s.find(sub, start);
    return Value(pos == std::string::npos ? -1.0 : static_cast<double>(pos));
}

// ── last_index_of(str, substr) → number ──────────────────────────────────────
Value strings_last_index_of(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2)
        throw std::runtime_error("strings.last_index_of(str, substr) expects 2 arguments.");
    std::string s   = getStr(args[0], "strings.last_index_of");
    std::string sub = getStr(args[1], "strings.last_index_of");
    size_t pos = s.rfind(sub);
    return Value(pos == std::string::npos ? -1.0 : static_cast<double>(pos));
}

// ── substring(str, start, end?) → string ─────────────────────────────────────
Value strings_substring(VM& vm, std::vector<Value> args) {
    if (args.size() < 2 || args.size() > 3)
        throw std::runtime_error("strings.substring(str, start, end?) expects 2-3 arguments.");
    std::string s = getStr(args[0], "strings.substring");
    int start = static_cast<int>(getNum(args[1], "strings.substring"));
    int end   = (args.size() == 3) ? static_cast<int>(getNum(args[2], "strings.substring"))
                                   : static_cast<int>(s.size());
    if (start < 0) start = 0;
    if (end > static_cast<int>(s.size())) end = static_cast<int>(s.size());
    if (start >= end) return Value(vm.internString(""));
    return Value(vm.internString(s.substr(start, end - start)));
}

// ── repeat(str, n) → string ──────────────────────────────────────────────────
Value strings_repeat(VM& vm, std::vector<Value> args) {
    if (args.size() != 2) throw std::runtime_error("strings.repeat(str, n) expects 2 arguments.");
    std::string s = getStr(args[0], "strings.repeat");
    int n = static_cast<int>(getNum(args[1], "strings.repeat"));
    if (n <= 0) return Value(vm.internString(""));
    std::string result;
    result.reserve(s.size() * n);
    for (int i = 0; i < n; i++) result += s;
    return Value(vm.internString(result));
}

// ── reverse(str) → string ────────────────────────────────────────────────────
Value strings_reverse(VM& vm, std::vector<Value> args) {
    if (args.size() != 1) throw std::runtime_error("strings.reverse() expects 1 argument.");
    std::string s = getStr(args[0], "strings.reverse");
    std::reverse(s.begin(), s.end());
    return Value(vm.internString(s));
}

// ── length(str) → number ─────────────────────────────────────────────────────
Value strings_length(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) throw std::runtime_error("strings.length() expects 1 argument.");
    std::string s = getStr(args[0], "strings.length");
    return Value(static_cast<double>(s.size()));
}

// ── count(str, substr) → number ──────────────────────────────────────────────
Value strings_count(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 2) throw std::runtime_error("strings.count(str, substr) expects 2 arguments.");
    std::string s   = getStr(args[0], "strings.count");
    std::string sub = getStr(args[1], "strings.count");
    if (sub.empty()) return Value(0.0);
    int count = 0;
    size_t pos = 0;
    while ((pos = s.find(sub, pos)) != std::string::npos) { count++; pos += sub.size(); }
    return Value(static_cast<double>(count));
}

// ── char_at(str, index) → string ─────────────────────────────────────────────
Value strings_char_at(VM& vm, std::vector<Value> args) {
    if (args.size() != 2) throw std::runtime_error("strings.char_at(str, index) expects 2 arguments.");
    std::string s = getStr(args[0], "strings.char_at");
    int idx = static_cast<int>(getNum(args[1], "strings.char_at"));
    if (idx < 0 || idx >= static_cast<int>(s.size()))
        throw std::runtime_error("strings.char_at: index out of bounds.");
    return Value(vm.internString(std::string(1, s[idx])));
}

// ── char_code(str, index?) → number ──────────────────────────────────────────
Value strings_char_code(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() < 1 || args.size() > 2)
        throw std::runtime_error("strings.char_code(str, index?) expects 1-2 arguments.");
    std::string s = getStr(args[0], "strings.char_code");
    int idx = (args.size() == 2) ? static_cast<int>(getNum(args[1], "strings.char_code")) : 0;
    if (s.empty() || idx < 0 || idx >= static_cast<int>(s.size()))
        throw std::runtime_error("strings.char_code: index out of bounds.");
    return Value(static_cast<double>(static_cast<unsigned char>(s[idx])));
}

// ── from_char_code(code) → string ────────────────────────────────────────────
Value strings_from_char_code(VM& vm, std::vector<Value> args) {
    if (args.size() != 1) throw std::runtime_error("strings.from_char_code(code) expects 1 argument.");
    int code = static_cast<int>(getNum(args[0], "strings.from_char_code"));
    if (code < 0 || code > 127)
        throw std::runtime_error("strings.from_char_code: code must be 0-127.");
    return Value(vm.internString(std::string(1, static_cast<char>(code))));
}

// ── is_empty(str) → bool ─────────────────────────────────────────────────────
Value strings_is_empty(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) throw std::runtime_error("strings.is_empty() expects 1 argument.");
    std::string s = getStr(args[0], "strings.is_empty");
    return Value(s.empty());
}

// ── is_alpha(str) / is_digit(str) / is_alnum(str) ────────────────────────────
Value strings_is_alpha(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) throw std::runtime_error("strings.is_alpha() expects 1 argument.");
    std::string s = getStr(args[0], "strings.is_alpha");
    if (s.empty()) return Value(false);
    return Value(std::all_of(s.begin(), s.end(), ::isalpha));
}

Value strings_is_digit(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) throw std::runtime_error("strings.is_digit() expects 1 argument.");
    std::string s = getStr(args[0], "strings.is_digit");
    if (s.empty()) return Value(false);
    return Value(std::all_of(s.begin(), s.end(), ::isdigit));
}

Value strings_is_alnum(VM& vm, std::vector<Value> args) {
    (void)vm;
    if (args.size() != 1) throw std::runtime_error("strings.is_alnum() expects 1 argument.");
    std::string s = getStr(args[0], "strings.is_alnum");
    if (s.empty()) return Value(false);
    return Value(std::all_of(s.begin(), s.end(), ::isalnum));
}

// ── registration ─────────────────────────────────────────────────────────────
namespace neutron {
    void register_strings_functions(VM& vm, std::shared_ptr<Environment> env) {
        env->define("split",          Value(vm.allocate<NativeFn>(strings_split,          -1, true)));
        env->define("join",           Value(vm.allocate<NativeFn>(strings_join,           -1, true)));
        env->define("trim",           Value(vm.allocate<NativeFn>(strings_trim,            1, true)));
        env->define("trim_left",      Value(vm.allocate<NativeFn>(strings_trim_left,       1, true)));
        env->define("trim_right",     Value(vm.allocate<NativeFn>(strings_trim_right,      1, true)));
        env->define("upper",          Value(vm.allocate<NativeFn>(strings_upper,           1, true)));
        env->define("lower",          Value(vm.allocate<NativeFn>(strings_lower,           1, true)));
        env->define("replace",        Value(vm.allocate<NativeFn>(strings_replace,         3, true)));
        env->define("replace_first",  Value(vm.allocate<NativeFn>(strings_replace_first,   3, true)));
        env->define("contains",       Value(vm.allocate<NativeFn>(strings_contains,        2, true)));
        env->define("starts_with",    Value(vm.allocate<NativeFn>(strings_starts_with,     2, true)));
        env->define("ends_with",      Value(vm.allocate<NativeFn>(strings_ends_with,       2, true)));
        env->define("index_of",       Value(vm.allocate<NativeFn>(strings_index_of,       -1, true)));
        env->define("last_index_of",  Value(vm.allocate<NativeFn>(strings_last_index_of,   2, true)));
        env->define("substring",      Value(vm.allocate<NativeFn>(strings_substring,      -1, true)));
        env->define("repeat",         Value(vm.allocate<NativeFn>(strings_repeat,          2, true)));
        env->define("reverse",        Value(vm.allocate<NativeFn>(strings_reverse,         1, true)));
        env->define("length",         Value(vm.allocate<NativeFn>(strings_length,          1, true)));
        env->define("count",          Value(vm.allocate<NativeFn>(strings_count,           2, true)));
        env->define("char_at",        Value(vm.allocate<NativeFn>(strings_char_at,         2, true)));
        env->define("char_code",      Value(vm.allocate<NativeFn>(strings_char_code,      -1, true)));
        env->define("from_char_code", Value(vm.allocate<NativeFn>(strings_from_char_code,  1, true)));
        env->define("is_empty",       Value(vm.allocate<NativeFn>(strings_is_empty,        1, true)));
        env->define("is_alpha",       Value(vm.allocate<NativeFn>(strings_is_alpha,        1, true)));
        env->define("is_digit",       Value(vm.allocate<NativeFn>(strings_is_digit,        1, true)));
        env->define("is_alnum",       Value(vm.allocate<NativeFn>(strings_is_alnum,        1, true)));
    }
}

extern "C" void neutron_init_strings_module(neutron::VM* vm) {
    auto env = std::make_shared<neutron::Environment>();
    neutron::register_strings_functions(*vm, env);
    auto mod = vm->allocate<neutron::Module>("strings", env);
    vm->define_module("strings", mod);
}
