/*
 * Neutron Programming Language - AOT Native Module Interface
 * Copyright (c) 2026 yasakei
 * 
 * This header provides the interface for AOT-compatible native modules.
 * Native modules compiled with this interface can be called from AOT-compiled code.
 */

#ifndef NEUTRON_AOT_MODULE_H
#define NEUTRON_AOT_MODULE_H

#include <cstdint>
#include <cmath>
#include <string>

namespace neutron {
namespace aot {

// Minimal Value struct matching AOT compiler output
struct Value {
    enum Type { NIL, BOOLEAN, NUMBER, OBJ_STRING } type;
    union { bool boolean; double number; const char* string; } as;
    
    Value() : type(NIL) { as.number = 0; }
    Value(bool b) : type(BOOLEAN) { as.boolean = b; }
    Value(double n) : type(NUMBER) { as.number = n; }
    Value(const char* s) : type(OBJ_STRING) { as.string = s; }
    
    std::string toString() const {
        switch (type) {
            case NIL: return "nil";
            case BOOLEAN: return as.boolean ? "true" : "false";
            case NUMBER: {
                double n = as.number;
                if (n == (long long)n) return std::to_string((long long)n);
                return std::to_string(n);
            }
            case OBJ_STRING: return std::string(as.string);
            default: return "unknown";
        }
    }
};

// Native function signature for AOT modules
typedef Value (*AotNativeFunction)(Value* args, int argCount);

// Module registration macro
#define AOT_MODULE_EXPORT(name) \
    extern "C" Value aot_##name(Value* args, int argCount)

// Math module - AOT compatible functions
namespace math {
    AOT_MODULE_EXPORT(sin) {
        if (argCount >= 1 && args[0].type == Value::NUMBER) {
            return Value(std::sin(args[0].as.number));
        }
        return Value(0.0);
    }
    
    AOT_MODULE_EXPORT(cos) {
        if (argCount >= 1 && args[0].type == Value::NUMBER) {
            return Value(std::cos(args[0].as.number));
        }
        return Value(0.0);
    }
    
    AOT_MODULE_EXPORT(tan) {
        if (argCount >= 1 && args[0].type == Value::NUMBER) {
            return Value(std::tan(args[0].as.number));
        }
        return Value(0.0);
    }
    
    AOT_MODULE_EXPORT(sqrt) {
        if (argCount >= 1 && args[0].type == Value::NUMBER) {
            return Value(std::sqrt(args[0].as.number));
        }
        return Value(0.0);
    }
    
    AOT_MODULE_EXPORT(abs) {
        if (argCount >= 1 && args[0].type == Value::NUMBER) {
            return Value(std::abs(args[0].as.number));
        }
        return Value(0.0);
    }
    
    AOT_MODULE_EXPORT(floor) {
        if (argCount >= 1 && args[0].type == Value::NUMBER) {
            return Value(std::floor(args[0].as.number));
        }
        return Value(0.0);
    }
    
    AOT_MODULE_EXPORT(ceil) {
        if (argCount >= 1 && args[0].type == Value::NUMBER) {
            return Value(std::ceil(args[0].as.number));
        }
        return Value(0.0);
    }
    
    AOT_MODULE_EXPORT(round) {
        if (argCount >= 1 && args[0].type == Value::NUMBER) {
            return Value(std::round(args[0].as.number));
        }
        return Value(0.0);
    }
    
    AOT_MODULE_EXPORT(pow) {
        if (argCount >= 2 && args[0].type == Value::NUMBER && args[1].type == Value::NUMBER) {
            return Value(std::pow(args[0].as.number, args[1].as.number));
        }
        return Value(0.0);
    }
    
    AOT_MODULE_EXPORT(log) {
        if (argCount >= 1 && args[0].type == Value::NUMBER) {
            return Value(std::log(args[0].as.number));
        }
        return Value(0.0);
    }
    
    AOT_MODULE_EXPORT(log10) {
        if (argCount >= 1 && args[0].type == Value::NUMBER) {
            return Value(std::log10(args[0].as.number));
        }
        return Value(0.0);
    }
} // namespace math

// Random module - AOT compatible functions
namespace random_impl {
    static unsigned int randomSeed = 12345;
    
    static unsigned int nextRandom() {
        randomSeed = randomSeed * 1103515245 + 12345;
        return (randomSeed / 65536) % 32768;
    }
}

namespace random {
    AOT_MODULE_EXPORT(seed) {
        if (argCount >= 1 && args[0].type == Value::NUMBER) {
            random_impl::randomSeed = static_cast<unsigned int>(args[0].as.number);
        }
        return Value();
    }
    
    AOT_MODULE_EXPORT(random) {
        return Value(static_cast<double>(random_impl::nextRandom()) / 32768.0);
    }
    
    AOT_MODULE_EXPORT(range) {
        if (argCount >= 2 && args[0].type == Value::NUMBER && args[1].type == Value::NUMBER) {
            int minVal = static_cast<int>(args[0].as.number);
            int maxVal = static_cast<int>(args[1].as.number);
            int range = maxVal - minVal + 1;
            return Value(static_cast<double>(minVal + (random_impl::nextRandom() % range)));
        }
        return Value(0.0);
    }
} // namespace random

// Fmt module - AOT compatible functions
namespace fmt {
    AOT_MODULE_EXPORT(to_string) {
        if (argCount >= 1) {
            std::string s = args[0].toString();
            // Return a heap-allocated string (simplified - in production use proper memory management)
            char* str = new char[s.length() + 1];
            strcpy(str, s.c_str());
            return Value(str);
        }
        return Value("");
    }
    
    AOT_MODULE_EXPORT(to_number) {
        if (argCount >= 1 && args[0].type == Value::OBJ_STRING) {
            return Value(std::stod(args[0].as.string));
        }
        return Value(0.0);
    }
} // namespace fmt

// Path module - AOT compatible functions (basic string operations)
namespace path_impl {
    static std::string lastPathResult;
}

namespace path {
    AOT_MODULE_EXPORT(join) {
        if (argCount >= 2 && args[0].type == Value::OBJ_STRING && args[1].type == Value::OBJ_STRING) {
            path_impl::lastPathResult = std::string(args[0].as.string) + "/" + args[1].as.string;
            return Value(path_impl::lastPathResult.c_str());
        }
        return Value("");
    }
    
    AOT_MODULE_EXPORT(basename) {
        if (argCount >= 1 && args[0].type == Value::OBJ_STRING) {
            std::string p = args[0].as.string;
            size_t pos = p.find_last_of("/\\");
            if (pos != std::string::npos) {
                path_impl::lastPathResult = p.substr(pos + 1);
            } else {
                path_impl::lastPathResult = p;
            }
            return Value(path_impl::lastPathResult.c_str());
        }
        return Value("");
    }
    
    AOT_MODULE_EXPORT(dirname) {
        if (argCount >= 1 && args[0].type == Value::OBJ_STRING) {
            std::string p = args[0].as.string;
            size_t pos = p.find_last_of("/\\");
            if (pos != std::string::npos) {
                path_impl::lastPathResult = p.substr(0, pos);
            } else {
                path_impl::lastPathResult = ".";
            }
            return Value(path_impl::lastPathResult.c_str());
        }
        return Value("");
    }
} // namespace path

} // namespace aot
} // namespace neutron

#endif // NEUTRON_AOT_MODULE_H
