#include "aot/aot_runtime.h"
#include "core/vm.h"
#include "compiler/compiler.h"
#include "types/json_object.h"
#include "types/array.h"
#include "types/instance.h"
#include "types/obj_string.h"
#include "types/value.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <setjmp.h>

namespace neutron {
namespace aot {

// Convert runtime Value to NaN-boxed uint64_t
static inline uint64_t valueToNan(const Value& val) {
    switch (val.type) {
        case ValueType::NIL:
            return AOT_NAN_BASE | (AOT_NAN_NIL << 47);
        case ValueType::BOOLEAN:
            return AOT_NAN_BASE | (AOT_NAN_BOOL << 47) | (val.as.boolean ? 1ULL : 0ULL);
        case ValueType::NUMBER: {
            uint64_t bits;
            memcpy(&bits, &val.as.number, sizeof(bits));
            return bits;
        }
        case ValueType::OBJ_STRING:
            return AOT_NAN_BASE | (AOT_NAN_STRING << 47) |
                   (reinterpret_cast<uint64_t>(val.as.obj_string) & AOT_NAN_PAYLOAD_MASK);
        case ValueType::ARRAY:
            return AOT_NAN_BASE | (AOT_NAN_ARRAY << 47) |
                   (reinterpret_cast<uint64_t>(val.as.array) & AOT_NAN_PAYLOAD_MASK);
        case ValueType::INSTANCE:
            return AOT_NAN_BASE | (AOT_NAN_INSTANCE << 47) |
                   (reinterpret_cast<uint64_t>(val.as.instance) & AOT_NAN_PAYLOAD_MASK);
        case ValueType::CALLABLE:
            return AOT_NAN_BASE | (AOT_NAN_CALLABLE << 47) |
                   (reinterpret_cast<uint64_t>(val.as.callable) & AOT_NAN_PAYLOAD_MASK);
        case ValueType::OBJECT:
            return AOT_NAN_BASE | (AOT_NAN_OBJECT << 47) |
                   (reinterpret_cast<uint64_t>(val.as.object) & AOT_NAN_PAYLOAD_MASK);
        default:
            return AOT_NAN_BASE | (AOT_NAN_NIL << 47);
    }
}

// Convert NaN-boxed uint64_t to runtime Value
static inline Value nanToValue(uint64_t nanVal) {
    if (!aot_isTagged(nanVal)) {
        double d;
        memcpy(&d, &nanVal, sizeof(d));
        return Value(d);
    }

    uint64_t tag = aot_getTag(nanVal);
    uint64_t payload = nanVal & AOT_NAN_PAYLOAD_MASK;

    switch (tag) {
        case AOT_NAN_NIL:
            return Value();
        case AOT_NAN_BOOL:
            return Value(static_cast<bool>(payload & 1));
        case AOT_NAN_STRING:
            return Value(reinterpret_cast<ObjString*>(payload));
        case AOT_NAN_ARRAY:
            return Value(reinterpret_cast<Array*>(payload));
        case AOT_NAN_INSTANCE:
            return Value(reinterpret_cast<Instance*>(payload));
        case AOT_NAN_CALLABLE:
            return Value(reinterpret_cast<Callable*>(payload));
        case AOT_NAN_OBJECT:
            return Value(reinterpret_cast<Object*>(payload));
        default:
            return Value();
    }
}

} // namespace aot
} // namespace neutron

using namespace neutron;
using namespace neutron::aot;

extern "C" {

uint64_t aot_getProperty(void* vm_ctx, uint64_t objVal, const char* propName) {
    VM* vm = static_cast<VM*>(vm_ctx);
    Value obj = nanToValue(objVal);
    ObjString* propStr = vm->internString(propName);

    if (obj.type == ValueType::INSTANCE) {
        Instance* inst = obj.as.instance;
        Value* field = inst->getField(propStr);
        if (field) return valueToNan(*field);
        auto methIt = inst->klass->methods.find(propStr);
        if (methIt != inst->klass->methods.end()) {
            Value methodValue = methIt->second;
            if (methodValue.type == ValueType::CALLABLE) {
                Callable* c = methodValue.as.callable;
                if (c->obj_type == ObjType::OBJ_FUNCTION) {
                    return valueToNan(Value(vm->allocate<BoundMethod>(obj, static_cast<Function*>(c))));
                }
            }
            return valueToNan(methodValue);
        }
        return valueToNan(Value());
    }

    if (obj.type == ValueType::OBJECT) {
        if (obj.as.object->obj_type == ObjType::OBJ_JSON_OBJECT) {
            JsonObject* jsonObj = static_cast<JsonObject*>(obj.as.object);
            auto it = jsonObj->properties.find(propStr);
            if (it != jsonObj->properties.end()) {
                return valueToNan(it->second);
            }
        }
        return valueToNan(Value());
    }

    if (obj.type == ValueType::ARRAY) {
        Array* arr = obj.as.array;
        std::string propStrName(propName);
        if (propStrName == "length") {
            return valueToNan(Value(static_cast<double>(arr->size())));
        }
        return valueToNan(Value());
    }

    if (obj.type == ValueType::OBJ_STRING) {
        std::string propStrName(propName);
        ObjString* strObj = obj.as.obj_string;
        if (propStrName == "length") {
            return valueToNan(Value(static_cast<double>(strObj->chars.length())));
        }
        return valueToNan(Value());
    }

    if (obj.type == ValueType::MODULE) {
        Module* mod = obj.as.module;
        try {
            Value prop = mod->get(propName);
            return valueToNan(prop);
        } catch (...) {
            return valueToNan(Value());
        }
    }

    return valueToNan(Value());
}

void aot_setProperty(void* vm_ctx, uint64_t objVal, const char* propName, uint64_t val) {
    VM* vm = static_cast<VM*>(vm_ctx);
    Value obj = nanToValue(objVal);
    Value propVal = nanToValue(val);
    ObjString* propStr = vm->internString(propName);

    if (obj.type == ValueType::INSTANCE) {
        obj.as.instance->setField(propStr, propVal);
    } else if (obj.type == ValueType::OBJECT) {
        if (obj.as.object->obj_type == ObjType::OBJ_JSON_OBJECT) {
            JsonObject* jsonObj = static_cast<JsonObject*>(obj.as.object);
            jsonObj->properties[propStr] = propVal;
        }
    }
}

// --- Cached property access (AOT inline cache) ---

uint64_t aot_getPropertyCached(void* vm_ctx, uint64_t objVal, const char* propName, void* cachePtr) {
    VM* vm = static_cast<VM*>(vm_ctx);
    AotPropCache* cache = static_cast<AotPropCache*>(cachePtr);
    Value obj = nanToValue(objVal);

    if (obj.type == ValueType::INSTANCE && cache->klass != nullptr) {
        Instance* inst = obj.as.instance;
        if (inst->klass == cache->klass) {
            // Cache hit — direct inline field access, no string lookup
            return valueToNan(inst->inlineFields[cache->inlineIndex].value);
        }
    }

    // Cache miss: do full lookup
    ObjString* propStr = vm->internString(propName);

    if (obj.type == ValueType::INSTANCE) {
        Instance* inst = obj.as.instance;
        Value* field = inst->getField(propStr);
        if (field) {
            // Check if the field is in inline storage and update cache
            uint8_t* fieldBase = reinterpret_cast<uint8_t*>(inst->inlineFields);
            uint8_t* fieldPtr = reinterpret_cast<uint8_t*>(field);
            ptrdiff_t byteOff = fieldPtr - fieldBase;
            if (byteOff >= 0 && byteOff < static_cast<ptrdiff_t>(INLINE_FIELD_COUNT * sizeof(InlineField)) &&
                (byteOff % sizeof(InlineField)) == offsetof(InlineField, value)) {
                cache->klass = inst->klass;
                cache->inlineIndex = static_cast<uint8_t>(byteOff / sizeof(InlineField));
            }
            return valueToNan(*field);
        }
        auto methIt = inst->klass->methods.find(propStr);
        if (methIt != inst->klass->methods.end()) {
            Value methodValue = methIt->second;
            if (methodValue.type == ValueType::CALLABLE) {
                Callable* c = methodValue.as.callable;
                if (c->obj_type == ObjType::OBJ_FUNCTION) {
                    return valueToNan(Value(vm->allocate<BoundMethod>(obj, static_cast<Function*>(c))));
                }
            }
            return valueToNan(methodValue);
        }
        return valueToNan(Value());
    }

    if (obj.type == ValueType::OBJECT) {
        if (obj.as.object->obj_type == ObjType::OBJ_JSON_OBJECT) {
            JsonObject* jsonObj = static_cast<JsonObject*>(obj.as.object);
            auto it = jsonObj->properties.find(propStr);
            if (it != jsonObj->properties.end()) {
                return valueToNan(it->second);
            }
        }
        return valueToNan(Value());
    }

    if (obj.type == ValueType::ARRAY) {
        Array* arr = obj.as.array;
        std::string propStrName(propName);
        if (propStrName == "length") {
            return valueToNan(Value(static_cast<double>(arr->size())));
        }
        return valueToNan(Value());
    }

    if (obj.type == ValueType::OBJ_STRING) {
        std::string propStrName(propName);
        ObjString* strObj = obj.as.obj_string;
        if (propStrName == "length") {
            return valueToNan(Value(static_cast<double>(strObj->chars.length())));
        }
        return valueToNan(Value());
    }

    if (obj.type == ValueType::MODULE) {
        Module* mod = obj.as.module;
        try {
            Value prop = mod->get(propName);
            return valueToNan(prop);
        } catch (...) {
            return valueToNan(Value());
        }
    }

    return valueToNan(Value());
}

void aot_setPropertyCached(void* vm_ctx, uint64_t objVal, const char* propName, uint64_t val, void* cachePtr) {
    VM* vm = static_cast<VM*>(vm_ctx);
    AotPropCache* cache = static_cast<AotPropCache*>(cachePtr);
    Value obj = nanToValue(objVal);
    Value propVal = nanToValue(val);
    ObjString* propStr = vm->internString(propName);

    if (obj.type == ValueType::INSTANCE) {
        Instance* inst = obj.as.instance;
        // Check cache first for fast set
        if (cache->klass == inst->klass) {
            inst->inlineFields[cache->inlineIndex].value = propVal;
            return;
        }
        // Cache miss: do full setField and try to update cache
        inst->setField(propStr, propVal);
        // Find the field and cache it if inline
        Value* field = inst->getField(propStr);
        if (field) {
            uint8_t* fieldBase = reinterpret_cast<uint8_t*>(inst->inlineFields);
            uint8_t* fieldPtr = reinterpret_cast<uint8_t*>(field);
            ptrdiff_t byteOff = fieldPtr - fieldBase;
            if (byteOff >= 0 && byteOff < static_cast<ptrdiff_t>(INLINE_FIELD_COUNT * sizeof(InlineField)) &&
                (byteOff % sizeof(InlineField)) == offsetof(InlineField, value)) {
                cache->klass = inst->klass;
                cache->inlineIndex = static_cast<uint8_t>(byteOff / sizeof(InlineField));
            }
        }
    } else if (obj.type == ValueType::OBJECT) {
        if (obj.as.object->obj_type == ObjType::OBJ_JSON_OBJECT) {
            JsonObject* jsonObj = static_cast<JsonObject*>(obj.as.object);
            jsonObj->properties[propStr] = propVal;
        }
    }
}

uint64_t aot_indexGet(void* vm_ctx, uint64_t objVal, uint64_t indexVal) {
    VM* vm = static_cast<VM*>(vm_ctx);
    (void)vm;
    Value obj = nanToValue(objVal);
    Value idx = nanToValue(indexVal);

    if (obj.type == ValueType::ARRAY) {
        Array* arr = obj.as.array;
        size_t i = static_cast<size_t>(idx.as.number);
        if (i < arr->size()) {
            return valueToNan(arr->at(i));
        }
    }

    if (obj.type == ValueType::OBJECT) {
        if (obj.as.object->obj_type == ObjType::OBJ_JSON_OBJECT) {
            JsonObject* jsonObj = static_cast<JsonObject*>(obj.as.object);
            if (idx.type == ValueType::OBJ_STRING) {
                auto it = jsonObj->properties.find(idx.as.obj_string);
                if (it != jsonObj->properties.end()) {
                    return valueToNan(it->second);
                }
            }
        }
    }

    if (obj.type == ValueType::OBJ_STRING) {
        ObjString* str = obj.as.obj_string;
        size_t i = static_cast<size_t>(idx.as.number);
        if (i < str->chars.length()) {
            std::string ch(1, str->chars[i]);
            return valueToNan(Value(vm->internString(ch)));
        }
    }

    return valueToNan(Value());
}

void aot_indexSet(void* vm_ctx, uint64_t objVal, uint64_t indexVal, uint64_t val) {
    VM* vm = static_cast<VM*>(vm_ctx);
    (void)vm;
    Value obj = nanToValue(objVal);
    Value idx = nanToValue(indexVal);
    Value newVal = nanToValue(val);

    if (obj.type == ValueType::ARRAY) {
        Array* arr = obj.as.array;
        size_t i = static_cast<size_t>(idx.as.number);
        if (i < arr->size()) {
            arr->set(i, newVal);
        }
    } else if (obj.type == ValueType::OBJECT) {
        if (obj.as.object->obj_type == ObjType::OBJ_JSON_OBJECT) {
            JsonObject* jsonObj = static_cast<JsonObject*>(obj.as.object);
            if (idx.type == ValueType::OBJ_STRING) {
                jsonObj->properties[idx.as.obj_string] = newVal;
            }
        }
    }
}

uint64_t aot_createArray(void* vm_ctx, const uint64_t* elements, uint8_t count) {
    VM* vm = static_cast<VM*>(vm_ctx);
    Array* arr = vm->allocate<Array>();
    for (uint8_t i = 0; i < count; i++) {
        arr->push(nanToValue(elements[i]));
    }
    return valueToNan(Value(arr));
}

uint64_t aot_createObject(void* vm_ctx, const uint64_t* keys, const uint64_t* values, uint8_t count) {
    VM* vm = static_cast<VM*>(vm_ctx);
    JsonObject* obj = vm->allocate<JsonObject>();
    for (uint8_t i = 0; i < count; i++) {
        Value key = nanToValue(keys[i]);
        Value val = nanToValue(values[i]);
        if (key.type == ValueType::OBJ_STRING) {
            obj->properties[key.as.obj_string] = val;
        }
    }
    return valueToNan(Value(obj));
}

uint64_t aot_call(void* vm_ctx, uint64_t callee, const uint64_t* args, uint8_t argCount) {
    VM* vm = static_cast<VM*>(vm_ctx);
    Value calleeVal = nanToValue(callee);

    std::vector<Value> argVec;
    argVec.reserve(argCount);
    for (uint8_t i = 0; i < argCount; i++) {
        argVec.push_back(nanToValue(args[i]));
    }

    try {
        Value result = vm->call(calleeVal, argVec);
        return valueToNan(result);
    } catch (const Return& ret) {
        return valueToNan(ret.value);
    } catch (...) {
        return valueToNan(Value());
    }
}

uint64_t aot_invoke(void* vm_ctx, uint64_t receiver, const char* methodName,
                    const uint64_t* args, uint8_t argCount) {
    VM* vm = static_cast<VM*>(vm_ctx);
    Value recv = nanToValue(receiver);
    ObjString* methodStr = vm->internString(methodName);

    std::vector<Value> argVec;
    argVec.reserve(argCount);
    for (uint8_t i = 0; i < argCount; i++) {
        argVec.push_back(nanToValue(args[i]));
    }

    if (recv.type == ValueType::INSTANCE) {
        Instance* inst = recv.as.instance;
        auto methIt = inst->klass->methods.find(methodStr);
        if (methIt != inst->klass->methods.end()) {
            Value callee = methIt->second;
            std::vector<Value> callArgs;
            callArgs.push_back(recv);
            for (auto& a : argVec) callArgs.push_back(a);
            try {
                Value result = vm->call(callee, callArgs);
                return valueToNan(result);
            } catch (const Return& ret) {
                return valueToNan(ret.value);
            } catch (...) {
                return valueToNan(Value());
            }
        }
    }

    if (recv.type == ValueType::ARRAY) {
        Array* arr = recv.as.array;
        std::string methodStrName(methodName);
        if (methodStrName == "push" && argCount == 1) {
            arr->push(nanToValue(args[0]));
            return valueToNan(Value(static_cast<double>(arr->size())));
        }
        if (methodStrName == "pop" && argCount == 0) {
            if (arr->size() > 0) {
                Value v = arr->pop();
                return valueToNan(v);
            }
            return valueToNan(Value());
        }
    }

    return valueToNan(Value());
}

uint64_t aot_getGlobal(void* vm_ctx, const char* name) {
    VM* vm = static_cast<VM*>(vm_ctx);
    auto it = vm->globals.find(name);
    if (it != vm->globals.end()) {
        return valueToNan(it->second);
    }
    return valueToNan(Value());
}

void aot_setGlobal(void* vm_ctx, const char* name, uint64_t val) {
    VM* vm = static_cast<VM*>(vm_ctx);
    vm->globals[name] = nanToValue(val);
}

uint64_t aot_internString(void* vm_ctx, const char* str) {
    VM* vm = static_cast<VM*>(vm_ctx);
    ObjString* s = vm->internString(str);
    return valueToNan(Value(s));
}

uint64_t aot_add(void* vm_ctx, uint64_t a, uint64_t b) {
    Value va = nanToValue(a);
    Value vb = nanToValue(b);
    if (va.type == ValueType::OBJ_STRING || vb.type == ValueType::OBJ_STRING) {
        VM* vm = static_cast<VM*>(vm_ctx);
        std::string result = va.toString() + vb.toString();
        return valueToNan(Value(vm->internString(result)));
    }
    double da, db;
    memcpy(&da, &a, sizeof(da));
    memcpy(&db, &b, sizeof(db));
    double sum = da + db;
    uint64_t result;
    memcpy(&result, &sum, sizeof(result));
    return result;
}

void aot_printValue(void* vm_ctx, uint64_t val) {
    (void)vm_ctx;
    static FILE* const out = stdout;

    if (!aot_isTagged(val)) {
        double d;
        memcpy(&d, &val, sizeof(d));
        // Check if it's an integer value
        int64_t truncated = static_cast<int64_t>(d);
        if (static_cast<double>(truncated) == d) {
            fprintf(out, "%lld\n", static_cast<long long>(truncated));
        } else {
            fprintf(out, "%g\n", d);
        }
        return;
    }

    uint64_t tag = aot_getTag(val);
    uint64_t payload = val & AOT_NAN_PAYLOAD_MASK;

    switch (tag) {
        case AOT_NAN_NIL:
            fprintf(out, "nil\n");
            break;
        case AOT_NAN_BOOL:
            fprintf(out, "%s\n", (payload & 1) ? "true" : "false");
            break;
        case AOT_NAN_STRING: {
            ObjString* str = reinterpret_cast<ObjString*>(payload);
            fprintf(out, "%s\n", str->chars.c_str());
            break;
        }
        case AOT_NAN_ARRAY:
        case AOT_NAN_OBJECT:
        case AOT_NAN_INSTANCE:
        case AOT_NAN_CALLABLE: {
            // Convert to runtime Value and use toString()
            Value v = nanToValue(val);
            std::string s = v.toString();
            fprintf(out, "%s\n", s.c_str());
            break;
        }
        default:
            fprintf(out, "nil\n");
            break;
    }
}

// --- For-in loop helpers ---

uint64_t aot_forInInit(void* vm_ctx, uint64_t iterableVal) {
    VM* vm = static_cast<VM*>(vm_ctx);
    Value iterable = nanToValue(iterableVal);
    Array* keys = vm->allocate<Array>();

    if (iterable.type == ValueType::OBJECT) {
        Object* obj = iterable.as.object;
        if (obj && obj->obj_type == ObjType::OBJ_JSON_OBJECT) {
            auto* jobj = static_cast<JsonObject*>(obj);
            for (const auto& kv : jobj->properties) {
                keys->push(Value(kv.first));
            }
        }
    } else if (iterable.type == ValueType::ARRAY) {
        Array* arr = iterable.as.array;
        for (size_t i = 0; i < arr->size(); i++) {
            keys->push(Value(static_cast<double>(i)));
        }
    }

    return valueToNan(Value(keys));
}

// --- Exception handling ---

// Per-VM exception frames for AOT
// Stores frame info so aot_throwError can print meaningful stack traces.
// Full exception recovery (catch blocks executing in AOT) requires
// setjmp/longjmp bridging back into LLVM IR — deferred but frame info is correct.

void aot_tryPush(void* vm_ctx, uint16_t tryEnd, uint16_t catchStart, uint16_t finallyStart) {
    VM* vm = static_cast<VM*>(vm_ctx);
    VM::ExceptionFrame frame;
    frame.tryStart = 0; // AOT codegen tracks IP separately
    frame.tryEnd = tryEnd;
    frame.catchStart = catchStart;
    frame.finallyStart = finallyStart;
    if (!vm->frames.empty()) {
        frame.frameBase = vm->frames.back().slot_offset;
    } else {
        frame.frameBase = 0;
    }
    vm->exceptionFrames.push_back(frame);
}

void aot_tryPop(void* vm_ctx) {
    VM* vm = static_cast<VM*>(vm_ctx);
    if (!vm->exceptionFrames.empty()) {
        vm->exceptionFrames.pop_back();
    }
}

static std::string aot_buildStackTrace(VM* vm) {
    std::string trace;
    for (auto it = vm->frames.rbegin(); it != vm->frames.rend(); ++it) {
        std::string funcName = "<script>";
        if (it->function) {
            funcName = it->function->name;
        }
        trace += "    at " + funcName + "\n";
    }
    return trace;
}

void aot_throwError(void* vm_ctx, uint64_t exceptionVal) {
    VM* vm = static_cast<VM*>(vm_ctx);
    Value exception = nanToValue(exceptionVal);
    std::string msg;
    if (exception.type == ValueType::OBJ_STRING) {
        msg = exception.as.obj_string->chars;
    } else {
        msg = exception.toString();
    }
    std::string trace = aot_buildStackTrace(vm);
    std::fprintf(stderr, "Error: %s\n%s", msg.c_str(), trace.c_str());
    std::exit(1);
}

void aot_runtimeError(void* vm_ctx, const char* message) {
    VM* vm = static_cast<VM*>(vm_ctx);
    std::string trace = aot_buildStackTrace(vm);
    std::fprintf(stderr, "Error: %s\n%s", message, trace.c_str());
    std::exit(1);
}

// --- Safe-mode validation helpers ---

void aot_validateSafeFunction(void* vm_ctx, uint64_t funcVal, int isSafeFile) {
    Value fv = nanToValue(funcVal);
    if (fv.type == ValueType::CALLABLE) {
        Function* function = dynamic_cast<Function*>(fv.as.callable);
        if (function && function->declaration) {
            for (const auto& param : function->declaration->params) {
                if (!param.typeAnnotation.has_value()) {
                    std::string msg = "Function parameter '" + param.name.lexeme + "' must have a type annotation" +
                        (isSafeFile ? " in .ntsc files (Neutron Safe Code)." : " inside a safe block.");
                    aot_runtimeError(vm_ctx, msg.c_str());
                }
            }
            if (!function->declaration->returnType.has_value()) {
                std::string msg = "Function '" + function->declaration->name.lexeme + "' must have a return type annotation" +
                    (isSafeFile ? " in .ntsc files (Neutron Safe Code)." : " inside a safe block.");
                aot_runtimeError(vm_ctx, msg.c_str());
            }
        }
    }
}

void aot_validateSafeFileFunction(void* vm_ctx, uint64_t funcVal) {
    Value fv = nanToValue(funcVal);
    if (fv.type == ValueType::CALLABLE) {
        Function* function = dynamic_cast<Function*>(fv.as.callable);
        if (function && function->declaration) {
            for (const auto& param : function->declaration->params) {
                if (!param.typeAnnotation.has_value()) {
                    std::string msg = "Function parameter '" + param.name.lexeme + "' must have a type annotation in safe file (.ntsc).";
                    aot_runtimeError(vm_ctx, msg.c_str());
                }
            }
            if (!function->declaration->returnType.has_value()) {
                std::string msg = "Function '" + function->declaration->name.lexeme + "' must have a return type annotation in safe file (.ntsc).";
                aot_runtimeError(vm_ctx, msg.c_str());
            }
        }
    }
}

void aot_validateSafeVariable(void* vm_ctx, const char* varName, int isSafeFile) {
    std::string msg = std::string("Variable '") + varName + "' must have a type annotation" +
        (isSafeFile ? " in .ntsc files (Neutron Safe Code)." : " inside a safe block.");
    aot_runtimeError(vm_ctx, msg.c_str());
}

void aot_validateSafeFileVariable(void* vm_ctx, const char* varName) {
    std::string msg = std::string("Variable '") + varName + "' must have a type annotation in safe file (.ntsc).";
    aot_runtimeError(vm_ctx, msg.c_str());
}

// --- Typed set helpers ---
// Type annotation byte values matching TokenType enum (token.h):
// TYPE_INT=77, TYPE_FLOAT=78, TYPE_STRING=79, TYPE_BOOL=80,
// TYPE_ARRAY=81, TYPE_OBJECT=82, TYPE_ANY=83.
// Using integer constants instead of TokenType for MSVC portability.

static bool aot_validateType(uint8_t expectedType, Value value) {
    switch (expectedType) {
        case 77: case 78: return value.type == ValueType::NUMBER;     // TYPE_INT, TYPE_FLOAT
        case 79: return value.type == ValueType::OBJ_STRING;          // TYPE_STRING
        case 80: return value.type == ValueType::BOOLEAN;             // TYPE_BOOL
        case 81: return value.type == ValueType::ARRAY;               // TYPE_ARRAY
        case 82: return value.type == ValueType::OBJECT;              // TYPE_OBJECT
        case 83: return true;                                         // TYPE_ANY
        default: return true;
    }
}

void aot_setLocalTyped(void* vm_ctx, uint64_t val, uint64_t slotVal, uint8_t expectedType) {
    (void)slotVal;
    Value value = nanToValue(val);
    if (!aot_validateType(expectedType, value)) {
        std::string actualName = value.type == ValueType::NIL ? "nil" :
                                  value.type == ValueType::BOOLEAN ? "boolean" :
                                  value.type == ValueType::NUMBER ? "number" :
                                  value.type == ValueType::OBJ_STRING ? "string" :
                                  value.type == ValueType::ARRAY ? "array" :
                                  value.type == ValueType::OBJECT ? "object" : "callable";
        std::string msg = "Type mismatch: Cannot assign value of type '" + actualName + "'.";
        aot_runtimeError(vm_ctx, msg.c_str());
    }
    // Actual store is done by the LLVM codegen (slotVal is the local index)
}

void aot_setGlobalTyped(void* vm_ctx, const char* name, uint64_t val) {
    VM* vm = static_cast<VM*>(vm_ctx);
    std::string varName(name);
    auto typeIt = vm->globalTypes.find(varName);
    if (typeIt != vm->globalTypes.end()) {
        Value value = nanToValue(val);
        if (!aot_validateType(static_cast<uint8_t>(typeIt->second), value)) {
            std::string actualName = value.type == ValueType::NIL ? "nil" :
                                      value.type == ValueType::BOOLEAN ? "boolean" :
                                      value.type == ValueType::NUMBER ? "number" :
                                      value.type == ValueType::OBJ_STRING ? "string" :
                                      value.type == ValueType::ARRAY ? "array" :
                                      value.type == ValueType::OBJECT ? "object" : "callable";
            std::string msg = "Type mismatch: Cannot assign value of type '" + actualName + "'.";
            aot_runtimeError(vm_ctx, msg.c_str());
        }
    }
    // Note: actual global value is stored by AOT codegen in LLVM global;
    // this helper only does type validation.
}

void aot_defineTypedGlobal(void* vm_ctx, const char* name, uint64_t val, uint8_t typeByte) {
    VM* vm = static_cast<VM*>(vm_ctx);
    std::string varName(name);
    vm->globals[varName] = nanToValue(val);
    vm->globalTypes[varName] = static_cast<decltype(vm->globalTypes)::mapped_type>(typeByte);
}

// ---- Focused helpers ----

uint64_t aot_tryGetCachedProp(void* inst, void* cache) {
    Instance* i = static_cast<Instance*>(inst);
    AotPropCache* c = static_cast<AotPropCache*>(cache);
    if (c->klass != nullptr && i->klass == c->klass) {
        return valueToNan(i->inlineFields[c->inlineIndex].value);
    }
    return AOT_SENTINEL;
}

uint8_t aot_trySetCachedProp(void* inst, void* cache, uint64_t val) {
    Instance* i = static_cast<Instance*>(inst);
    AotPropCache* c = static_cast<AotPropCache*>(cache);
    if (c->klass != nullptr && i->klass == c->klass) {
        i->inlineFields[c->inlineIndex].value = nanToValue(val);
        return 1;
    }
    return 0;
}

uint64_t aot_arrayGetCached(void* arr, uint64_t idxVal) {
    Array* a = static_cast<Array*>(arr);
    Value idx = nanToValue(idxVal);
    if (idx.type != ValueType::NUMBER) {
        // Tag dispatch: if index is a number, extract it
        // Otherwise return nil
        uint64_t bits;
        memcpy(&bits, &idx.as.number, sizeof(bits));
        (void)bits;
        return AOT_NAN_BASE | (AOT_NAN_NIL << 47);
    }
    size_t i = static_cast<size_t>(idx.as.number);
    if (i < a->size()) {
        return valueToNan(a->at(i));
    }
    return AOT_NAN_BASE | (AOT_NAN_NIL << 47);
}

void aot_arraySetCached(void* arr, uint64_t idxVal, uint64_t val) {
    Array* a = static_cast<Array*>(arr);
    Value idx = nanToValue(idxVal);
    if (idx.type == ValueType::NUMBER) {
        size_t i = static_cast<size_t>(idx.as.number);
        if (i < a->size()) {
            a->set(i, nanToValue(val));
        }
    }
}

// ---- Phase 6: Direct native function call support ----

static void* s_llvmFuncTable[256] = {nullptr};

void aot_registerLlvmFunc(int idx, void* funcPtr) {
    if (idx >= 0 && idx < 256) {
        s_llvmFuncTable[idx] = funcPtr;
    }
}

uint64_t aot_tryDirectCall(void* vm_ctx, uint64_t callee, const uint64_t* args, uint8_t argCount) {
    if (!aot_isTagged(callee)) return AOT_SENTINEL;
    if (aot_getTag(callee) != AOT_NAN_CALLABLE) return AOT_SENTINEL;

    void* ptr = reinterpret_cast<void*>(callee & AOT_NAN_PAYLOAD_MASK);
    Object* obj = static_cast<Object*>(ptr);
    if (obj->obj_type != ObjType::OBJ_FUNCTION) return AOT_SENTINEL;

    auto* fn = static_cast<Function*>(obj);
    int idx = fn->aotFuncIndex;
    if (idx < 0 || idx >= 256 || !s_llvmFuncTable[idx]) return AOT_SENTINEL;

    auto compiledFn = reinterpret_cast<uint64_t(*)(void*, const uint64_t*, uint8_t)>(s_llvmFuncTable[idx]);
    return compiledFn(vm_ctx, args, argCount);
}

uint64_t aot_stringCharAt(void* vm_ctx, uint64_t strVal, uint64_t idxVal) {
    ObjString* s = reinterpret_cast<ObjString*>(static_cast<uintptr_t>(strVal & AOT_NAN_PAYLOAD_MASK));
    Value idx = nanToValue(idxVal);
    if (idx.type == ValueType::NUMBER) {
        size_t i = static_cast<size_t>(idx.as.number);
        if (i < s->chars.length()) {
            VM* vm = static_cast<VM*>(vm_ctx);
            std::string ch(1, s->chars[i]);
            return valueToNan(Value(vm->internString(ch)));
        }
    }
    return AOT_NAN_BASE | (AOT_NAN_NIL << 47);
}

} // extern "C"
