#include "types/value.h"
#include "types/array.h"
#include "types/obj_string.h"
#include "types/function.h"
#include "types/closure.h"
#include "types/upvalue.h"
#include "types/json_object.h"
#include "types/json_array.h"
#include "types/class.h"
#include "types/instance.h"
#include "types/bound_method.h"
#include "types/native_fn.h"
#include "modules/module.h"
#include "core/vm.h"
#include "runtime/error_handler.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdarg>

// ============================================================
// QBE AOT Runtime Helpers
//
// These functions are called from QBE-compiled Neutron code.
// Their C symbol names match QBE's $rt_* convention (QBE strips
// the $ prefix, so $rt_call → asm rt_call).
// ============================================================

// The VM context — set once at module init
static neutron::VM* g_vm = nullptr;
static std::unordered_map<std::string, neutron::Class*> g_class_map;

extern "C" void rt_init(neutron::VM* vm) {
    g_vm = vm;
    // QBE AOT mode does not currently have stack maps for precise garbage collection.
    // To prevent the GC from sweeping live objects referenced only by QBE stack/registers,
    // we effectively disable GC for AOT mode by setting the threshold to the maximum value.
    if (g_vm) {
        g_vm->nextGC = static_cast<size_t>(-1);
    }
}

// Return value buffer — QBE code reads from here after runtime calls
// This is a SHARED buffer, not per-module. Defined here, referenced by QBE code.
static uint64_t g_ret_tag = 0;
static uint64_t g_ret_data = 0;

// The QBE codegen uses loadw/loadl from $rt_ret, which the assembler
// resolves to our exported symbols. We define these with C linkage
// matching the QBE naming: $rt_ret → asm rt_ret.
extern "C" uint64_t rt_ret[2] = {1, 0}; // {tag, data}, default nil

// ============================================================
// Value conversion helpers (internal, not exported to QBE)
// ============================================================

// QBE tag → ValueType mapping:
//   1=NIL, 2=BOOL, 3=NUM, 4=STR, 5=ARRAY, 6=OBJ, 7=CLASS, 8=INST, 9=CALLABLE

static neutron::Value make_qbe_value(uint64_t tag, uint64_t data) {
    switch (tag) {
        case 1: return neutron::Value();
        case 2: return neutron::Value(static_cast<bool>(data));
        case 3: {
            double d;
            memcpy(&d, &data, sizeof(d));
            return neutron::Value(d);
        }
        case 4: {
            // data might be an ObjString* (from runtime) or const char* (from data section)
            neutron::Object* obj = reinterpret_cast<neutron::Object*>(data);
            if (obj && obj->obj_type == neutron::ObjType::OBJ_STRING) {
                return neutron::Value(static_cast<neutron::ObjString*>(obj));
            }
            const char* s = reinterpret_cast<const char*>(data);
            return neutron::Value(new neutron::ObjString(s ? s : ""));
        }
        case 5: {
            neutron::Array* arr = reinterpret_cast<neutron::Array*>(data);
            return arr ? neutron::Value(arr) : neutron::Value();
        }
        case 6: {
            neutron::Object* obj = reinterpret_cast<neutron::Object*>(data);
            return obj ? neutron::Value(obj) : neutron::Value();
        }
        case 7: {
            // AOT mode: data points to class name string in data section
            if (g_vm) {
                const char* name = reinterpret_cast<const char*>(data);
                if (name) {
                    auto it = g_class_map.find(name);
                    if (it != g_class_map.end()) {
                        return neutron::Value(it->second);
                    }
                    // Class not yet in map — create bare class
                    auto* klass = g_vm->allocate<neutron::Class>(std::string(name));
                    g_class_map[name] = klass;
                    return neutron::Value(klass);
                }
            }
            // Runtime mode: data is a Class* pointer
            neutron::Class* klass = reinterpret_cast<neutron::Class*>(data);
            return klass ? neutron::Value(klass) : neutron::Value();
        }
        case 8: {
            neutron::Instance* inst = reinterpret_cast<neutron::Instance*>(data);
            return inst ? neutron::Value(inst) : neutron::Value();
        }
        case 9: {
            neutron::Callable* callable = reinterpret_cast<neutron::Callable*>(data);
            return callable ? neutron::Value(callable) : neutron::Value();
        }
        default:
            return neutron::Value();
    }
}

static void store_result(const neutron::Value& v) {
    // Update the local (static) ret values
    switch (v.type) {
        case neutron::ValueType::NIL:
            g_ret_tag = 1;
            g_ret_data = 0;
            break;
        case neutron::ValueType::BOOLEAN:
            g_ret_tag = 2;
            g_ret_data = v.as.boolean ? 1 : 0;
            break;
        case neutron::ValueType::NUMBER:
            g_ret_tag = 3;
            memcpy(&g_ret_data, &v.as.number, sizeof(double));
            break;
        case neutron::ValueType::OBJ_STRING:
            g_ret_tag = 4;
            g_ret_data = reinterpret_cast<uint64_t>(v.as.obj_string);
            break;
        case neutron::ValueType::ARRAY:
            g_ret_tag = 5;
            g_ret_data = reinterpret_cast<uint64_t>(v.as.array);
            break;
        case neutron::ValueType::OBJECT:
            g_ret_tag = 6;
            g_ret_data = reinterpret_cast<uint64_t>(v.as.object);
            break;
        case neutron::ValueType::CLASS:
            g_ret_tag = 7;
            g_ret_data = reinterpret_cast<uint64_t>(v.as.klass);
            break;
        case neutron::ValueType::INSTANCE:
            g_ret_tag = 8;
            g_ret_data = reinterpret_cast<uint64_t>(v.as.instance);
            break;
        case neutron::ValueType::CALLABLE:
            g_ret_tag = 9;
            g_ret_data = reinterpret_cast<uint64_t>(v.as.callable);
            break;
        case neutron::ValueType::MODULE:
            g_ret_tag = 6;
            g_ret_data = reinterpret_cast<uint64_t>(v.as.object);
            break;
        default:
            g_ret_tag = 1;
            g_ret_data = 0;
            break;
    }
    // Sync to the externally visible buffer (for QBE to read)
    rt_ret[0] = g_ret_tag;
    rt_ret[1] = g_ret_data;
}

// ============================================================
// Simple runtime helpers — called with direct w/l args from QBE
// ============================================================

extern "C" void rt_add(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        double a, b;
        memcpy(&a, &data_a, sizeof(double));
        memcpy(&b, &data_b, sizeof(double));
        store_result(neutron::Value(a + b));
    } else {
        neutron::Value va = make_qbe_value(tag_a, data_a);
        neutron::Value vb = make_qbe_value(tag_b, data_b);
        if (va.type == neutron::ValueType::OBJ_STRING && vb.type == neutron::ValueType::OBJ_STRING) {
            neutron::ObjString* strA = va.as.obj_string;
            neutron::ObjString* strB = vb.as.obj_string;
            if (!strA->isInterned) {
                strA->chars.append(strB->chars);
                strA->hashComputed = false;
                store_result(va);
            } else {
                std::string result;
                result.reserve(strA->chars.size() + strB->chars.size());
                result.append(strA->chars);
                result.append(strB->chars);
                store_result(neutron::Value(g_vm->makeString(std::move(result))));
            }
        } else if (va.type == neutron::ValueType::OBJ_STRING || vb.type == neutron::ValueType::OBJ_STRING) {
            if (va.type == neutron::ValueType::OBJ_STRING && !va.as.obj_string->isInterned) {
                va.as.obj_string->chars.append(vb.toString());
                va.as.obj_string->hashComputed = false;
                store_result(va);
            } else {
                std::string result = va.toString() + vb.toString();
                store_result(neutron::Value(g_vm->makeString(std::move(result))));
            }
        } else {
            // Type error or unsupported — return nil
            store_result(neutron::Value());
        }
    }
}

extern "C" void rt_sub(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        double a, b;
        memcpy(&a, &data_a, sizeof(double));
        memcpy(&b, &data_b, sizeof(double));
        store_result(neutron::Value(a - b));
    } else {
        store_result(neutron::Value());
    }
}

extern "C" void rt_mul(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        double a, b;
        memcpy(&a, &data_a, sizeof(double));
        memcpy(&b, &data_b, sizeof(double));
        store_result(neutron::Value(a * b));
    } else {
        store_result(neutron::Value());
    }
}

extern "C" void rt_div(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        double a, b;
        memcpy(&a, &data_a, sizeof(double));
        memcpy(&b, &data_b, sizeof(double));
        if (b == 0.0) {
            store_result(neutron::Value());
        } else {
            store_result(neutron::Value(a / b));
        }
    } else {
        store_result(neutron::Value());
    }
}

extern "C" void rt_mod(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        double a, b;
        memcpy(&a, &data_a, sizeof(double));
        memcpy(&b, &data_b, sizeof(double));
        store_result(neutron::Value(std::fmod(a, b)));
    } else {
        store_result(neutron::Value());
    }
}

extern "C" void rt_neg(uint64_t tag, uint64_t data) {
    if (tag == 3) {
        double d;
        memcpy(&d, &data, sizeof(double));
        store_result(neutron::Value(-d));
    } else {
        store_result(neutron::Value());
    }
}

// Comparison helpers
extern "C" void rt_eq(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        double a, b;
        memcpy(&a, &data_a, sizeof(double));
        memcpy(&b, &data_b, sizeof(double));
        store_result(neutron::Value(a == b));
    } else if (tag_a == 4 && tag_b == 4) {
        neutron::Value va = make_qbe_value(tag_a, data_a);
        neutron::Value vb = make_qbe_value(tag_b, data_b);
        store_result(neutron::Value(va.as.obj_string->chars == vb.as.obj_string->chars));
    } else if (tag_a == tag_b && data_a == data_b) {
        store_result(neutron::Value(true));
    } else {
        store_result(neutron::Value(false));
    }
}

extern "C" void rt_neq(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        double a, b;
        memcpy(&a, &data_a, sizeof(double));
        memcpy(&b, &data_b, sizeof(double));
        store_result(neutron::Value(a != b));
    } else if (tag_a == 4 && tag_b == 4) {
        neutron::Value va = make_qbe_value(tag_a, data_a);
        neutron::Value vb = make_qbe_value(tag_b, data_b);
        store_result(neutron::Value(va.as.obj_string->chars != vb.as.obj_string->chars));
    } else if (tag_a == tag_b && data_a == data_b) {
        store_result(neutron::Value(false));
    } else {
        store_result(neutron::Value(true));
    }
}

extern "C" void rt_lt(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        double a, b;
        memcpy(&a, &data_a, sizeof(double));
        memcpy(&b, &data_b, sizeof(double));
        store_result(neutron::Value(a < b));
    } else {
        store_result(neutron::Value(false));
    }
}

extern "C" void rt_gt(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        double a, b;
        memcpy(&a, &data_a, sizeof(double));
        memcpy(&b, &data_b, sizeof(double));
        store_result(neutron::Value(a > b));
    } else {
        store_result(neutron::Value(false));
    }
}

// Bitwise helpers
static int64_t double_bits_to_int64(uint64_t bits) {
    double d;
    memcpy(&d, &bits, sizeof(double));
    return static_cast<int64_t>(d);
}

extern "C" void rt_band(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        int64_t a = double_bits_to_int64(data_a);
        int64_t b = double_bits_to_int64(data_b);
        store_result(neutron::Value(static_cast<double>(a & b)));
    } else {
        store_result(neutron::Value());
    }
}

extern "C" void rt_bor(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        int64_t a = double_bits_to_int64(data_a);
        int64_t b = double_bits_to_int64(data_b);
        store_result(neutron::Value(static_cast<double>(a | b)));
    } else {
        store_result(neutron::Value());
    }
}

extern "C" void rt_bxor(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        int64_t a = double_bits_to_int64(data_a);
        int64_t b = double_bits_to_int64(data_b);
        store_result(neutron::Value(static_cast<double>(a ^ b)));
    } else {
        store_result(neutron::Value());
    }
}

extern "C" void rt_bnot(uint64_t tag, uint64_t data) {
    if (tag == 3) {
        int64_t a = double_bits_to_int64(data);
        store_result(neutron::Value(static_cast<double>(~a)));
    } else {
        store_result(neutron::Value());
    }
}

extern "C" void rt_shl(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        int64_t a = double_bits_to_int64(data_a);
        int64_t b = double_bits_to_int64(data_b);
        store_result(neutron::Value(static_cast<double>(a << b)));
    } else {
        store_result(neutron::Value());
    }
}

extern "C" void rt_shr(uint64_t tag_a, uint64_t data_a, uint64_t tag_b, uint64_t data_b) {
    if (tag_a == 3 && tag_b == 3) {
        int64_t a = double_bits_to_int64(data_a);
        int64_t b = double_bits_to_int64(data_b);
        store_result(neutron::Value(static_cast<double>(a >> b)));
    } else {
        store_result(neutron::Value());
    }
}

// I/O
extern "C" void rt_say(uint64_t tag, uint64_t data) {
    neutron::Value v = make_qbe_value(tag, data);
    std::cout << v.toString() << std::endl;
}

// ============================================================
// Complex runtime helpers — called from QBE with variadic args
// ============================================================

// Global get: $rt_get_global(name_str_ptr)
// Looks up a global variable by name from g_vm->globals
extern "C" void rt_get_global(uint64_t name_str_ptr, uint64_t data_section_ptr) {
    // First check the QBE data section for user variables
    if (data_section_ptr) {
        uint64_t qtag = *reinterpret_cast<const uint64_t*>(data_section_ptr);
        // Check both tag 0 (rt_nil) and tag 1 (global nil) — both mean nil
        if (qtag > 1) {
            uint64_t qdata = *reinterpret_cast<const uint64_t*>(data_section_ptr + 8);
            g_ret_tag = qtag;
            g_ret_data = qdata;
            rt_ret[0] = qtag;
            rt_ret[1] = qdata;
            return;
        }
    }

    // Fall back to g_vm->globals for native module globals
    if (!g_vm) {
        store_result(neutron::Value());
        return;
    }
    const char* name = reinterpret_cast<const char*>(name_str_ptr);
    if (!name) {
        store_result(neutron::Value());
        return;
    }
    auto it = g_vm->globals.find(name);
    if (it != g_vm->globals.end()) {
        store_result(it->second);
        // Cache back to the QBE data section for fast future access
        if (data_section_ptr) {
            *reinterpret_cast<uint64_t*>(data_section_ptr) = g_ret_tag;
            *reinterpret_cast<uint64_t*>(data_section_ptr + 8) = g_ret_data;
        }
    } else {
        store_result(neutron::Value());
    }
}

// Call dispatch: $rt_call(callee_tag, callee_data, arg_count, tag_0, data_0, ...)
extern "C" void rt_call(uint64_t tag_callee, uint64_t data_callee,
                         uint64_t arg_count, ...) {
    if (!g_vm) {
        store_result(neutron::Value());
        return;
    }

    neutron::Value callee = make_qbe_value(tag_callee, data_callee);

    // Handle CLASS type for class instantiation
    if (callee.type == neutron::ValueType::CLASS) {
        neutron::Class* klass = callee.as.klass;
        va_list va;
        va_start(va, arg_count);
        std::vector<neutron::Value> args;
        args.reserve(arg_count);
        for (uint64_t i = 0; i < arg_count; i++) {
            uint64_t t = va_arg(va, uint64_t);
            uint64_t d = va_arg(va, uint64_t);
            args.push_back(make_qbe_value(t, d));
        }
        va_end(va);
        try {
            // Push class value and args onto VM stack; callValue will allocate the instance
            g_vm->push(neutron::Value(klass));
            for (const auto& arg : args) {
                g_vm->push(arg);
            }
            if (g_vm->callValuePublic(neutron::Value(klass), static_cast<int>(arg_count))) {
                if (klass->initializer) {
                    g_vm->runPublic(g_vm->frames.size() - 1);
                }
                // Instance is now on top of VM stack; store it as the result
                if (!g_vm->stack.empty()) {
                    store_result(g_vm->stack.back());
                    g_vm->pop();
                } else {
                    store_result(neutron::Value());
                }
            } else {
                store_result(neutron::Value());
            }
        } catch (const std::exception& e) {
            std::cerr << "Runtime error in class instantiation: " << e.what() << std::endl;
            store_result(neutron::Value());
        }
        return;
    }

    if (callee.type != neutron::ValueType::CALLABLE) {
        store_result(neutron::Value());
        return;
    }

    va_list va;
    va_start(va, arg_count);

    std::vector<neutron::Value> args;
    args.reserve(arg_count);
    for (uint64_t i = 0; i < arg_count; i++) {
        uint64_t t = va_arg(va, uint64_t);
        uint64_t d = va_arg(va, uint64_t);
        args.push_back(make_qbe_value(t, d));
    }

    va_end(va);

    try {
        neutron::Value result = g_vm->call(callee, args);
        store_result(result);
    } catch (const std::exception& e) {
        std::cerr << "Runtime error in call: " << e.what() << std::endl;
        store_result(neutron::Value());
    }
}

// Array creation: $rt_array(size, tag_0, data_0, tag_1, data_1, ...)
extern "C" void rt_array(uint64_t size, ...) {
    if (!g_vm) {
        store_result(neutron::Value());
        return;
    }

    va_list va;
    va_start(va, size);

    auto* arr = g_vm->allocate<neutron::Array>();
    arr->elements.reserve(size);
    for (uint64_t i = 0; i < size; i++) {
        uint64_t t = va_arg(va, uint64_t);
        uint64_t d = va_arg(va, uint64_t);
        arr->elements.push_back(make_qbe_value(t, d));
    }

    va_end(va);
    store_result(neutron::Value(arr));
}

// Object creation: $rt_obj(count, tag_k1, data_k1, tag_v1, data_v1, ...)
extern "C" void rt_obj(uint64_t count, ...) {
    if (!g_vm) {
        store_result(neutron::Value());
        return;
    }

    auto* obj = g_vm->allocate<neutron::JsonObject>();
    neutron::Value obj_val(static_cast<neutron::Object*>(obj));

    va_list va;
    va_start(va, count);
    for (uint64_t i = 0; i < count; i++) {
        uint64_t tag_k = va_arg(va, uint64_t);
        uint64_t data_k = va_arg(va, uint64_t);
        uint64_t tag_v = va_arg(va, uint64_t);
        uint64_t data_v = va_arg(va, uint64_t);
        neutron::Value key = make_qbe_value(tag_k, data_k);
        neutron::Value val = make_qbe_value(tag_v, data_v);
        if (key.type == neutron::ValueType::OBJ_STRING) {
            obj->properties[key.as.obj_string] = val;
        }
    }
    va_end(va);

    store_result(obj_val);
}

// Index get: $rt_idx_r(tag_obj, data_obj, tag_idx, data_idx)
extern "C" void rt_idx_r(uint64_t tag_obj, uint64_t data_obj,
                          uint64_t tag_idx, uint64_t data_idx) {
    neutron::Value obj = make_qbe_value(tag_obj, data_obj);
    neutron::Value idx = make_qbe_value(tag_idx, data_idx);

    if (obj.type == neutron::ValueType::ARRAY) {
        neutron::Array* arr = obj.as.array;
        if (idx.type == neutron::ValueType::NUMBER) {
            int64_t i = static_cast<int64_t>(idx.as.number);
            if (i >= 0 && static_cast<size_t>(i) < arr->size()) {
                store_result(arr->at(static_cast<size_t>(i)));
            } else {
                store_result(neutron::Value());
            }
        } else {
            store_result(neutron::Value());
        }
    } else if (obj.type == neutron::ValueType::OBJECT) {
        // Check if it's a JsonArray
        if (obj.as.object->obj_type == neutron::ObjType::OBJ_JSON_ARRAY) {
            auto* jarr = static_cast<neutron::JsonArray*>(obj.as.object);
            if (idx.type == neutron::ValueType::NUMBER) {
                int64_t i = static_cast<int64_t>(idx.as.number);
                if (i >= 0 && static_cast<size_t>(i) < jarr->elements.size()) {
                    store_result(jarr->elements[static_cast<size_t>(i)]);
                } else {
                    store_result(neutron::Value());
                }
            } else {
                store_result(neutron::Value());
            }
        } else if (obj.as.object->obj_type == neutron::ObjType::OBJ_JSON_OBJECT) {
            auto* jobj = static_cast<neutron::JsonObject*>(obj.as.object);
            if (idx.type == neutron::ValueType::OBJ_STRING) {
                neutron::ObjString* key = idx.as.obj_string;
                auto it = jobj->properties.find(key);
                if (it != jobj->properties.end()) {
                    store_result(it->second);
                } else {
                    store_result(neutron::Value());
                }
            } else {
                store_result(neutron::Value());
            }
        } else {
            store_result(neutron::Value());
        }
    } else if (obj.type == neutron::ValueType::OBJ_STRING) {
        // String indexing: get character at position
        if (idx.type == neutron::ValueType::NUMBER) {
            const std::string& s = obj.as.obj_string->chars;
            int64_t i = static_cast<int64_t>(idx.as.number);
            if (i >= 0 && static_cast<size_t>(i) < s.size()) {
                std::string ch(1, s[static_cast<size_t>(i)]);
                store_result(neutron::Value(g_vm->makeString(std::move(ch))));
            } else {
                store_result(neutron::Value());
            }
        } else {
            store_result(neutron::Value());
        }
    } else {
        store_result(neutron::Value());
    }
}

// Index set: $rt_idx_w(tag_obj, data_obj, tag_idx, data_idx, tag_val, data_val)
extern "C" void rt_idx_w(uint64_t tag_obj, uint64_t data_obj,
                          uint64_t tag_idx, uint64_t data_idx,
                          uint64_t tag_val, uint64_t data_val) {
    neutron::Value obj = make_qbe_value(tag_obj, data_obj);
    neutron::Value idx = make_qbe_value(tag_idx, data_idx);
    neutron::Value val = make_qbe_value(tag_val, data_val);

    if (obj.type == neutron::ValueType::ARRAY) {
        neutron::Array* arr = obj.as.array;
        if (idx.type == neutron::ValueType::NUMBER) {
            int64_t i = static_cast<int64_t>(idx.as.number);
            if (i >= 0 && static_cast<size_t>(i) < arr->size()) {
                arr->set(static_cast<size_t>(i), val);
            }
        }
    } else if (obj.type == neutron::ValueType::OBJECT) {
        if (obj.as.object->obj_type == neutron::ObjType::OBJ_JSON_ARRAY) {
            auto* jarr = static_cast<neutron::JsonArray*>(obj.as.object);
            if (idx.type == neutron::ValueType::NUMBER) {
                int64_t i = static_cast<int64_t>(idx.as.number);
                if (i >= 0 && static_cast<size_t>(i) < jarr->elements.size()) {
                    jarr->elements[static_cast<size_t>(i)] = val;
                }
            }
        } else if (obj.as.object->obj_type == neutron::ObjType::OBJ_JSON_OBJECT) {
            auto* jobj = static_cast<neutron::JsonObject*>(obj.as.object);
            if (idx.type == neutron::ValueType::OBJ_STRING) {
                neutron::ObjString* key = idx.as.obj_string;
                jobj->properties[key] = val;
            }
        }
    }
    // No result to store — value is already on the QBE stack
}

// Property get: $rt_getprop(data_obj, data_prop_name_sym)
// data_prop_name_sym points to a constant string (from QBE data section)
extern "C" void rt_getprop(uint64_t tag_obj, uint64_t data_obj, uint64_t data_prop_name_ptr) {
    const char* prop_name = reinterpret_cast<const char*>(data_prop_name_ptr);
    if (!prop_name) {
        store_result(neutron::Value());
        return;
    }

    neutron::Value obj = make_qbe_value(tag_obj, data_obj);

    if (obj.type == neutron::ValueType::ARRAY) {
        if (strcmp(prop_name, "length") == 0) {
            store_result(neutron::Value(static_cast<double>(obj.as.array->size())));
            return;
        }
    } else if (obj.type == neutron::ValueType::OBJ_STRING) {
        if (strcmp(prop_name, "length") == 0) {
            double len = static_cast<double>(obj.as.obj_string->chars.length());
            store_result(neutron::Value(len));
            return;
        }
    } else if (obj.type == neutron::ValueType::OBJECT) {
        auto* json_obj = dynamic_cast<neutron::JsonObject*>(obj.as.object);
        if (json_obj && g_vm) {
            // Use interned string for pointer-equality lookup
            neutron::ObjString* key_str = g_vm->internString(prop_name);
            auto it = json_obj->properties.find(key_str);
            if (it != json_obj->properties.end()) {
                store_result(it->second);
                return;
            }
        }
    } else if (obj.type == neutron::ValueType::INSTANCE) {
        neutron::Instance* inst = obj.as.instance;
        if (g_vm) {
            neutron::ObjString* key_str = g_vm->internString(prop_name);
            neutron::Value* field = inst->getField(key_str);
            if (field) {
                store_result(*field);
                return;
            }
        }
    } else if (obj.type == neutron::ValueType::MODULE) {
        neutron::Value prop = obj.as.module->get(prop_name);
        store_result(prop);
        return;
    }

    store_result(neutron::Value());
}

// Property set: $rt_setprop(data_obj, data_prop_name_ptr, tag_val, data_val)
extern "C" void rt_setprop(uint64_t tag_obj, uint64_t data_obj, uint64_t data_prop_name_ptr,
                            uint64_t tag_val, uint64_t data_val) {
    neutron::Value val = make_qbe_value(tag_val, data_val);
    const char* prop_name = reinterpret_cast<const char*>(data_prop_name_ptr);
    if (!prop_name) return;

    neutron::Value obj = make_qbe_value(tag_obj, data_obj);

    if (obj.type == neutron::ValueType::OBJECT) {
        auto* json_obj = dynamic_cast<neutron::JsonObject*>(obj.as.object);
        if (json_obj && g_vm) {
            neutron::ObjString* key = g_vm->internString(prop_name);
            json_obj->properties[key] = val;
        }
    } else if (obj.type == neutron::ValueType::INSTANCE) {
        neutron::Instance* inst = obj.as.instance;
        if (g_vm) {
            neutron::ObjString* key = g_vm->internString(prop_name);
            inst->setField(key, val);
        }
    }
}

// Method invocation: $rt_invoke(data_obj, data_method_name_ptr, arg_count, tag_0, data_0, ...)
extern "C" void rt_invoke(uint64_t data_obj, uint64_t data_method_name_ptr,
                           uint64_t arg_count, ...) {
    if (!g_vm) {
        store_result(neutron::Value());
        return;
    }

    neutron::Value receiver = make_qbe_value(6, data_obj); // try OBJECT first
    // Actually, receiver could be any type that has methods (array, string, etc.)
    // Try to reconstruct from the data pointer:
    neutron::Object* obj_ptr = reinterpret_cast<neutron::Object*>(data_obj);
    if (obj_ptr) {
        switch (obj_ptr->obj_type) {
            case neutron::ObjType::OBJ_ARRAY:
                receiver = neutron::Value(static_cast<neutron::Array*>(obj_ptr));
                break;
            case neutron::ObjType::OBJ_JSON_ARRAY:
            case neutron::ObjType::OBJ_JSON_OBJECT:
                receiver = neutron::Value(static_cast<neutron::Object*>(obj_ptr));
                break;
            case neutron::ObjType::OBJ_STRING:
                receiver = neutron::Value(static_cast<neutron::ObjString*>(obj_ptr));
                break;
            case neutron::ObjType::OBJ_INSTANCE:
                receiver = neutron::Value(static_cast<neutron::Instance*>(obj_ptr));
                break;
            case neutron::ObjType::OBJ_MODULE:
                receiver = neutron::Value(static_cast<neutron::Module*>(obj_ptr));
                break;
            default:
                receiver = neutron::Value(static_cast<neutron::Object*>(obj_ptr));
                break;
        }
    }

    const char* method_name = reinterpret_cast<const char*>(data_method_name_ptr);
    if (!method_name) {
        store_result(neutron::Value());
        return;
    }

    va_list va;
    va_start(va, arg_count);

    std::vector<neutron::Value> args;
    args.reserve(arg_count);
    for (uint64_t i = 0; i < arg_count; i++) {
        uint64_t t = va_arg(va, uint64_t);
        uint64_t d = va_arg(va, uint64_t);
        args.push_back(make_qbe_value(t, d));
    }

    va_end(va);

    // Look up method by name
    std::string mn(method_name);

    // Handle array methods
    if (receiver.type == neutron::ValueType::ARRAY) {
        neutron::Array* arr = receiver.as.array;
        if (mn == "push" && arg_count == 1) {
            arr->push(args[0]);
            store_result(neutron::Value());
            return;
        } else if (mn == "pop" && arg_count == 0) {
            store_result(arr->pop());
            return;
        } else if (mn == "length" && arg_count == 0) {
            store_result(neutron::Value(static_cast<double>(arr->size())));
            return;
        } else if (mn == "slice" && arg_count == 2 &&
                   args[0].type == neutron::ValueType::NUMBER &&
                   args[1].type == neutron::ValueType::NUMBER) {
            int start = static_cast<int>(args[0].as.number);
            int end = static_cast<int>(args[1].as.number);
            if (start < 0) start = 0;
            if (end > static_cast<int>(arr->size())) end = static_cast<int>(arr->size());
            if (start > end) start = end;
            std::vector<neutron::Value> sliced;
            for (int i = start; i < end; i++)
                sliced.push_back(arr->at(i));
            store_result(neutron::Value(g_vm->allocate<neutron::Array>(sliced)));
            return;
        } else if (mn == "indexOf" && arg_count == 1) {
            int idx = -1;
            for (size_t i = 0; i < arr->size(); i++) {
                if (arr->at(i).toString() == args[0].toString()) {
                    idx = static_cast<int>(i);
                    break;
                }
            }
            store_result(neutron::Value(static_cast<double>(idx)));
            return;
        } else if (mn == "join" && arg_count == 1) {
            std::string sep = args[0].toString();
            std::string joined;
            for (size_t i = 0; i < arr->size(); i++) {
                if (i > 0) joined += sep;
                joined += arr->at(i).toString();
            }
            store_result(neutron::Value(joined));
            return;
        } else if (mn == "reverse" && arg_count == 0) {
            std::reverse(arr->elements.begin(), arr->elements.end());
            store_result(neutron::Value());
            return;
        } else if (mn == "sort" && arg_count == 0) {
            std::sort(arr->elements.begin(), arr->elements.end(),
                [](const neutron::Value& a, const neutron::Value& b) {
                    if (a.type == neutron::ValueType::NUMBER && b.type == neutron::ValueType::NUMBER)
                        return a.as.number < b.as.number;
                    if (a.type == neutron::ValueType::OBJ_STRING && b.type == neutron::ValueType::OBJ_STRING)
                        return a.toString() < b.toString();
                    return a.type == neutron::ValueType::NUMBER;
                });
            store_result(neutron::Value());
            return;
        }
    }

    // Handle string methods
    if (receiver.type == neutron::ValueType::OBJ_STRING && g_vm) {
        const std::string& str = receiver.as.obj_string->chars;
        if (mn == "length" && arg_count == 0) {
            store_result(neutron::Value(static_cast<double>(str.length())));
            return;
        } else if (mn == "find" && arg_count == 1) {
            size_t pos = str.find(args[0].toString());
            store_result(neutron::Value(static_cast<double>(pos != std::string::npos ? static_cast<int>(pos) : -1)));
            return;
        } else if (mn == "substr" && arg_count == 2 &&
                   args[0].type == neutron::ValueType::NUMBER &&
                   args[1].type == neutron::ValueType::NUMBER) {
            int start = static_cast<int>(args[0].as.number);
            int len = static_cast<int>(args[1].as.number);
            if (start < 0) start = 0;
            if (len < 0) len = 0;
            store_result(neutron::Value(str.substr(start, len)));
            return;
        } else if (mn == "contains" && arg_count == 1) {
            store_result(neutron::Value(str.find(args[0].toString()) != std::string::npos));
            return;
        } else if (mn == "upper" && arg_count == 0) {
            std::string upper;
            for (char c : str) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            store_result(neutron::Value(upper));
            return;
        } else if (mn == "lower" && arg_count == 0) {
            std::string lower;
            for (char c : str) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            store_result(neutron::Value(lower));
            return;
        } else if (mn == "trim" && arg_count == 0) {
            const std::string& s = str;
            size_t start = s.find_first_not_of(" \t\n\r");
            size_t end = s.find_last_not_of(" \t\n\r");
            if (start == std::string::npos) {
                store_result(neutron::Value(std::string()));
            } else {
                store_result(neutron::Value(s.substr(start, end - start + 1)));
            }
            return;
        } else if (mn == "split" && arg_count == 1) {
            std::string sep = args[0].toString();
            std::vector<neutron::Value> parts;
            size_t pos = 0, found;
            while ((found = str.find(sep, pos)) != std::string::npos) {
                parts.push_back(neutron::Value(str.substr(pos, found - pos)));
                pos = found + sep.length();
            }
            parts.push_back(neutron::Value(str.substr(pos)));
            store_result(neutron::Value(g_vm->allocate<neutron::Array>(parts)));
            return;
        } else if (mn == "replace" && arg_count == 2) {
            const std::string& from = args[0].toString();
            const std::string& to = args[1].toString();
            std::string result;
            size_t pos = 0, found;
            while ((found = str.find(from, pos)) != std::string::npos) {
                result.append(str, pos, found - pos);
                result += to;
                pos = found + from.length();
            }
            result.append(str, pos, str.length() - pos);
            store_result(neutron::Value(result));
            return;
        } else if (mn == "repeat" && arg_count == 1 &&
                   args[0].type == neutron::ValueType::NUMBER) {
            int count = static_cast<int>(args[0].as.number);
            std::string result;
            for (int i = 0; i < count; i++) result += str;
            store_result(neutron::Value(result));
            return;
        } else if (mn == "startsWith" && arg_count == 1) {
            store_result(neutron::Value(str.find(args[0].toString()) == 0));
            return;
        } else if (mn == "endsWith" && arg_count == 1) {
            const std::string& suffix = args[0].toString();
            store_result(neutron::Value(str.length() >= suffix.length() &&
                str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0));
            return;
        } else if (mn == "charCodeAt" && arg_count == 1 &&
                   args[0].type == neutron::ValueType::NUMBER) {
            int idx = static_cast<int>(args[0].as.number);
            if (idx >= 0 && static_cast<size_t>(idx) < str.length()) {
                store_result(neutron::Value(static_cast<double>(static_cast<unsigned char>(str[idx]))));
            } else {
                store_result(neutron::Value());
            }
            return;
        }
    }

    // Handle module methods (e.g. math.floor, strings.contains)
    if (receiver.type == neutron::ValueType::MODULE && g_vm) {
        neutron::Module* mod = receiver.as.module;
        neutron::Value func = mod->get(mn);
        if (func.type == neutron::ValueType::CALLABLE) {
            try {
                neutron::Value result = g_vm->call(func, args);
                store_result(result);
            } catch (...) {
                store_result(neutron::Value());
            }
            return;
        }
    }

    // For instances, check klass methods
    if (receiver.type == neutron::ValueType::INSTANCE) {
        neutron::Instance* inst = receiver.as.instance;

        // Check for methods in the class
        auto& methods = inst->klass->methods;
        neutron::ObjString* key_str = g_vm->internString(mn);
        auto it = methods.find(key_str);
        if (it != methods.end()) {
            // Found the method — set up frame manually (like OP_INVOKE)
            neutron::Value methodValue = it->second;
            if (methodValue.type == neutron::ValueType::CALLABLE) {
                neutron::Function* method = static_cast<neutron::Function*>(methodValue.as.callable);
                if (method->arity_val == arg_count) {
                    // Push receiver and args onto VM stack
                    g_vm->push(receiver);
                    for (const auto& arg : args) {
                        g_vm->push(arg);
                    }
                    // Set up call frame with slot_offset pointing to receiver
                    neutron::CallFrame* frame = &g_vm->frames.emplace_back();
                    frame->function = method;
                    frame->ip = method->chunk->code.empty() ? nullptr : method->chunk->code.data();
                    frame->slot_offset = g_vm->stack.size() - arg_count - 1;
                    frame->fileName = &g_vm->currentFileName;
                    frame->currentLine = -1;
                    frame->isBoundMethod = true;
                    // Execute the method
                    g_vm->runPublic(g_vm->frames.size() - 1);
                    if (!g_vm->stack.empty()) {
                        store_result(g_vm->stack.back());
                        g_vm->pop();
                    } else {
                        store_result(neutron::Value());
                    }
                    return;
                }
            }
            store_result(neutron::Value());
            return;
        }

        // Check for initializer
        if (mn == "init" && inst->klass->initializer) {
            args.insert(args.begin(), receiver);
            try {
                neutron::Value result = g_vm->call(
                    neutron::Value(inst->klass->initializer), args);
                store_result(result);
            } catch (...) {
                store_result(neutron::Value());
            }
            return;
        }
    }

    // Fallback: try property lookup then call via VM
    store_result(neutron::Value());
}

// Exception
extern "C" void rt_throw(uint64_t tag, uint64_t data) {
    neutron::Value v = make_qbe_value(tag, data);
    if (g_vm) {
        neutron::ErrorHandler::reportRuntimeError(v.toString());
    } else {
        std::cerr << "Runtime error: " << v.toString() << std::endl;
    }
}

// Increment/Decrement (data is the double bits, return updated double bits)
extern "C" uint64_t rt_inc(uint64_t data) {
    double d;
    memcpy(&d, &data, sizeof(double));
    d += 1.0;
    uint64_t result;
    memcpy(&result, &d, sizeof(double));
    return result;
}

extern "C" uint64_t rt_dec(uint64_t data) {
    double d;
    memcpy(&d, &data, sizeof(double));
    d -= 1.0;
    uint64_t result;
    memcpy(&result, &d, sizeof(double));
    return result;
}

// For-in iterator init: $rt_forin_init(tag, data)
// For arrays, creates a keys array (indices). For objects, gets property names.
// Pushes: keys_array (or nil if unsupported), index (0)
extern "C" void rt_forin_init(uint64_t tag, uint64_t data) {
    if (!g_vm) {
        store_result(neutron::Value());
        return;
    }

    neutron::Value obj = make_qbe_value(tag, data);
    auto* keys = g_vm->allocate<neutron::Array>();

    if (obj.type == neutron::ValueType::ARRAY) {
        neutron::Array* arr = obj.as.array;
        for (size_t i = 0; i < arr->size(); i++) {
            keys->elements.push_back(neutron::Value(static_cast<double>(i)));
        }
    } else if (obj.type == neutron::ValueType::OBJECT) {
        auto* json_obj = dynamic_cast<neutron::JsonObject*>(obj.as.object);
        if (json_obj) {
            for (const auto& [key, _] : json_obj->properties) {
                keys->elements.push_back(neutron::Value(key->chars));
            }
        } else {
            auto* jarr = dynamic_cast<neutron::JsonArray*>(obj.as.object);
            if (jarr) {
                for (size_t i = 0; i < jarr->elements.size(); i++) {
                    keys->elements.push_back(neutron::Value(static_cast<double>(i)));
                }
            }
        }
    } else if (obj.type == neutron::ValueType::OBJ_STRING) {
        const std::string& s = obj.as.obj_string->chars;
        for (size_t i = 0; i < s.size(); i++) {
            keys->elements.push_back(neutron::Value(static_cast<double>(i)));
        }
    } else if (obj.type == neutron::ValueType::INSTANCE) {
        // Instance fields — iterate over inline + overflow
        neutron::Instance* inst = obj.as.instance;
        for (uint8_t i = 0; i < inst->inlineCount; i++) {
            if (inst->inlineFields[i].key) {
                keys->elements.push_back(neutron::Value(inst->inlineFields[i].key->chars));
            }
        }
        if (inst->overflowFields) {
            for (const auto& [key, _] : *inst->overflowFields) {
                keys->elements.push_back(neutron::Value(key->chars));
            }
        }
    }

    store_result(neutron::Value(keys));
}

// For-in next: $rt_forin_next(data_keys, data_index)
// Returns the next key (as a Value), or nil if done
extern "C" void rt_forin_next(uint64_t data_keys, uint64_t data_index) {
    auto* keys = reinterpret_cast<neutron::Array*>(data_keys);
    if (!keys) {
        store_result(neutron::Value());
        return;
    }

    double idx_d;
    memcpy(&idx_d, &data_index, sizeof(double));
    size_t idx = static_cast<size_t>(idx_d);

    if (idx < keys->size()) {
        store_result(keys->at(idx));
    } else {
        store_result(neutron::Value());
    }
}

// Spread: $rt_spread(data)
// The QBE codegen handles individual element push; this just validates.
extern "C" void rt_spread(uint64_t data) {
    neutron::Value arr_val = make_qbe_value(5, data); // tag 5 = ARRAY
    if (arr_val.type == neutron::ValueType::ARRAY && arr_val.as.array) {
        // QBE codegen pushes each element after this call
    }
}

// ============================================================
// Closure runtime helpers
// ============================================================

// Thread-local current closure context for upvalue access
// When a QBE-compiled function with upvalues is executing, this
// points to the closure whose upvalues are being accessed.
// Set by rt_closure, read by rt_get_upvalue / rt_set_upvalue.
static thread_local neutron::ObjClosure* g_current_closure = nullptr;

// Create a closure: $rt_closure(func_data, num_upvalues, tag_0, data_0, ...)
// Captures upvalues BY VALUE at closure creation time.
// This is a simplified approach — upvalues are closed immediately.
// Returns the closure as a tagged Value in rt_ret.
extern "C" void rt_closure(uint64_t func_data, uint64_t num_upvalues, ...) {
    if (!g_vm) {
        store_result(neutron::Value());
        return;
    }

    neutron::Function* fn = reinterpret_cast<neutron::Function*>(func_data);
    if (!fn) {
        store_result(neutron::Value());
        return;
    }

    neutron::ObjClosure* closure = g_vm->allocate<neutron::ObjClosure>(fn);

    va_list va;
    va_start(va, num_upvalues);

    for (uint64_t i = 0; i < num_upvalues; i++) {
        uint64_t t = va_arg(va, uint64_t);
        uint64_t d = va_arg(va, uint64_t);
        // Create a closed upvalue (nullptr location = already closed)
        neutron::UpValue* uv = g_vm->allocate<neutron::UpValue>(nullptr);
        uv->closed = make_qbe_value(t, d);
        closure->upvalues.push_back(uv);
    }

    va_end(va);

    // Set as current closure for nested upvalue access
    g_current_closure = closure;

    store_result(neutron::Value(closure));
}

// Get upvalue: $rt_get_upvalue(slot)
// Reads from the current closure's captured upvalues.
extern "C" void rt_get_upvalue(uint64_t slot) {
    if (!g_current_closure || slot >= g_current_closure->upvalues.size()) {
        store_result(neutron::Value());
        return;
    }
    neutron::UpValue* uv = g_current_closure->upvalues[static_cast<size_t>(slot)];
    if (uv) {
        // If the upvalue is still open (location != nullptr), read from the stack
        // Otherwise read from the closed value
        if (uv->location) {
            store_result(*uv->location);
        } else {
            store_result(uv->closed);
        }
    } else {
        store_result(neutron::Value());
    }
}

// Set upvalue: $rt_set_upvalue(slot, tag, data)
// Writes to the current closure's captured upvalues.
extern "C" void rt_set_upvalue(uint64_t slot, uint64_t tag, uint64_t data) {
    if (!g_current_closure || slot >= g_current_closure->upvalues.size()) {
        return;
    }
    neutron::UpValue* uv = g_current_closure->upvalues[static_cast<size_t>(slot)];
    if (uv) {
        neutron::Value val = make_qbe_value(tag, data);
        if (uv->location) {
            *uv->location = val;
        } else {
            uv->closed = val;
        }
    }
}

// Close upvalue: $rt_close_upvalue(slot)
// In the simplified by-value model, upvalues are already closed at creation.
// This is a no-op in AOT mode.
extern "C" void rt_close_upvalue(uint64_t slot) {
    (void)slot;
}

// Truthiness check
extern "C" int rt_istruthy(uint64_t tag, uint64_t data) {
    switch (tag) {
        case 1:  return 0; // nil = falsy
        case 2:  return data != 0; // bool
        case 3: { // number
            double d;
            memcpy(&d, &data, sizeof(double));
            return d != 0.0;
        }
        default: return 1; // everything else is truthy
    }
}

// Create integer value (convert uint64_t → double bits)
extern "C" uint64_t rt_int(uint64_t val) {
    uint64_t result;
    double d = static_cast<double>(val);
    memcpy(&result, &d, sizeof(double));
    return result;
}

// Read uint32_t from byte pointer (little-endian)
static uint32_t read_u32(const uint8_t*& p) {
    uint32_t v = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
    p += 4;
    return v;
}

// Deserialize a Value from byte stream
static neutron::Value read_value(const uint8_t*& p) {
    uint8_t tag = *p++;
    switch (tag) {
        case 0: return neutron::Value();
        case 1: { bool b = *p++; return neutron::Value(b); }
        case 2: {
            double d;
            memcpy(&d, p, sizeof(d));
            p += 8;
            return neutron::Value(d);
        }
        case 3: {
            uint32_t len = read_u32(p);
            std::string s(reinterpret_cast<const char*>(p), len);
            p += len;
            // Use internString so pointer equality works for property lookups
            return neutron::Value(g_vm->internString(s));
        }
        default: return neutron::Value();
    }
}

// Initialize class definitions from serialized data in the data section
// The data is structured as a series of class definitions:
//   For each class: name_len(4), name(N), method_count(4)
//     For each method: name_len(4), name(N), arity(4), up_count(4),
//                      bytecode_size(4), bytecode(N), const_count(4), constants...
//   Terminator: 0xFFFFFFFF
// This stores reconstructed classes in a static map.
extern "C" void rt_init_classes(uint64_t data_ptr) {
    if (!g_vm) return;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data_ptr);
    while (true) {
        uint32_t name_len = read_u32(p);
        if (name_len == 0xFFFFFFFF) break; // terminator
        std::string class_name(reinterpret_cast<const char*>(p), name_len);
        p += name_len;
        uint32_t method_count = read_u32(p);
        auto* klass = g_vm->allocate<neutron::Class>(class_name);
        for (uint32_t m = 0; m < method_count; m++) {
            uint32_t mname_len = read_u32(p);
            std::string mname(reinterpret_cast<const char*>(p), mname_len);
            p += mname_len;
            uint32_t arity = read_u32(p);
            auto* fn = new neutron::Function(mname, static_cast<int>(arity));
            fn->chunk->code.clear();
            fn->chunk->constants.clear();
            uint32_t bc_size = read_u32(p);
            for (uint32_t i = 0; i < bc_size; i++) {
                fn->chunk->code.push_back(*p++);
            }
            uint32_t const_count = read_u32(p);
            for (uint32_t i = 0; i < const_count; i++) {
                fn->chunk->constants.push_back(read_value(p));
            }
            klass->methods[g_vm->internString(mname)] = fn;
            if (mname == "initialize") {
                klass->initializer = fn;
            }
        }
        g_class_map[class_name] = klass;
    }
}

// Add local const fallback: $rt_add_local_const(tag, data, const_tag, const_data)
extern "C" void rt_add_local_const(uint64_t tag, uint64_t data,
                                    uint64_t const_tag, uint64_t const_data) {
    if (tag == 3 && const_tag == 3) {
        double a, b;
        memcpy(&a, &data, sizeof(double));
        memcpy(&b, &const_data, sizeof(double));
        store_result(neutron::Value(a + b));
    } else {
        store_result(neutron::Value());
    }
}
