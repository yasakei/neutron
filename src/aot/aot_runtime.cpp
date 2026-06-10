#include "aot/aot_runtime.h"
#include "core/vm.h"
#include "types/json_object.h"
#include "types/array.h"
#include "types/instance.h"
#include "types/obj_string.h"
#include "types/value.h"

#include <cstddef>
#include <cstring>
#include <string>

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

} // extern "C"
