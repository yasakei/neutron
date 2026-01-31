/*
 * Neutron Programming Language
 * Copyright (c) 2026 yasakei
 * 
 * This software is distributed under the Neutron Permissive License (NPL) 1.1.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, for both open source and commercial purposes.
 * 
 * Conditions:
 * 
 * 1. The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 * 
 * 2. Attribution is appreciated but NOT required.
 *    Suggested (optional) credit:
 *    "Built using Neutron Programming Language (c) yasakei"
 * 
 * 3. The name "Neutron" and the name of the copyright holder may not be used
 *    to endorse or promote products derived from this Software without prior
 *    written permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "checkpoint.h"
#include "vm.h"
#include "types/value.h"
#include "types/obj_string.h"
#include "types/array.h"
#include "types/json_object.h"
#include "types/function.h"
#include "types/class.h"
#include "types/instance.h"
#include "types/native_fn.h"
#include "types/bound_method.h"
#include "compiler/compiler.h"
#include "compiler/scanner.h"
#include "compiler/parser.h"
#include "runtime/error_handler.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <set>

namespace neutron {

// Serializer Class
class Serializer {
public:
    std::ofstream& out;
    std::map<Object*, uint64_t> objToId;
    std::map<Environment*, uint64_t> envToId;
    uint64_t nextObjId = 1;
    uint64_t nextEnvId = 1;
    std::queue<Object*> objQueue;
    std::queue<Environment*> envQueue;

    Serializer(std::ofstream& out) : out(out) {}

    void writeByte(uint8_t byte) { out.write((char*)&byte, 1); }
    void writeInt(int val) { out.write((char*)&val, 4); }
    void writeLong(uint64_t val) { out.write((char*)&val, 8); }
    void writeDouble(double val) { out.write((char*)&val, 8); }
    void writeString(const std::string& str) {
        writeLong(str.length());
        out.write(str.c_str(), str.length());
    }

    uint64_t getObjectId(Object* obj) {
        if (!obj) return 0;
        if (objToId.find(obj) == objToId.end()) {
            objToId[obj] = nextObjId++;
            objQueue.push(obj);
        }
        return objToId[obj];
    }

    uint64_t getEnvId(Environment* env) {
        if (!env) return 0;
        if (envToId.find(env) == envToId.end()) {
            envToId[env] = nextEnvId++;
            envQueue.push(env);
        }
        return envToId[env];
    }

    void writeValue(const Value& val) {
        writeByte((uint8_t)val.type);
        switch (val.type) {
            case ValueType::NIL: break;
            case ValueType::BOOLEAN: writeByte(val.as.boolean); break;
            case ValueType::NUMBER: writeDouble(val.as.number); break;
            case ValueType::OBJ_STRING: writeLong(getObjectId(val.as.obj_string)); break;
            case ValueType::ARRAY: writeLong(getObjectId(val.as.array)); break;
            case ValueType::OBJECT: writeLong(getObjectId(val.as.object)); break;
            case ValueType::CALLABLE: writeLong(getObjectId(val.as.callable)); break;
            case ValueType::CLASS: writeLong(getObjectId(val.as.klass)); break;
            case ValueType::INSTANCE: writeLong(getObjectId(val.as.instance)); break;
            default: break;
        }
    }
};

void CheckpointManager::saveCheckpoint(VM& vm, const std::string& path, Value returnValue, int argsToPop) {
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open " + path);

    Serializer ser(file);
    ser.writeString("NTRN_CKPT");
    ser.writeInt(1);
    ser.writeString(vm.currentFileName);

    // 1. Discovery Phase
    std::queue<Value> valQueue;
    std::set<Object*> visitedObjs;
    std::set<Environment*> visitedEnvs;

    auto addObj = [&](Object* obj) {
        if (obj && visitedObjs.find(obj) == visitedObjs.end()) {
            visitedObjs.insert(obj);
            ser.getObjectId(obj);
            
            if (auto arr = dynamic_cast<Array*>(obj)) {
                for (const auto& v : arr->elements) valQueue.push(v);
            } else if (auto map = dynamic_cast<JsonObject*>(obj)) {
                for (const auto& p : map->properties) valQueue.push(p.second);
            } else if (auto fn = dynamic_cast<Function*>(obj)) {
                for (const auto& v : fn->chunk->constants) valQueue.push(v);
                if (fn->closure) ser.getEnvId(fn->closure.get());
            }
        }
    };

    // Roots
    // Stack (adjusted to simulate return)
    size_t stackSize = vm.stack.size();
    size_t itemsToRemove = argsToPop + 1; // args + function
    if (stackSize < itemsToRemove) itemsToRemove = 0; // Safety

    for (size_t i = 0; i < stackSize - itemsToRemove; i++) valQueue.push(vm.stack[i]);
    valQueue.push(returnValue);

    for (const auto& p : vm.globals) valQueue.push(p.second);
    for (const auto& f : vm.frames) addObj(f.function);

    auto getObjPtr = [](const Value& v) -> Object* {
        switch (v.type) {
            case ValueType::OBJ_STRING: return v.as.obj_string;
            case ValueType::ARRAY: return v.as.array;
            case ValueType::OBJECT: return v.as.object;
            case ValueType::CALLABLE: return v.as.callable;
            case ValueType::CLASS: return v.as.klass;
            case ValueType::INSTANCE: return v.as.instance;
            default: return nullptr;
        }
    };

    while (!valQueue.empty() || !ser.envQueue.empty()) {
        while (!valQueue.empty()) {
            Value v = valQueue.front(); valQueue.pop();
            if (v.type >= ValueType::OBJ_STRING) addObj(getObjPtr(v));
        }
        while (!ser.envQueue.empty()) {
            Environment* env = ser.envQueue.front(); ser.envQueue.pop();
            if (visitedEnvs.find(env) != visitedEnvs.end()) continue;
            visitedEnvs.insert(env);
            
            if (env->enclosing) ser.getEnvId(env->enclosing.get());
            for (const auto& p : env->values) valQueue.push(p.second);
        }
    }

    // 2. Write Definitions
    // Objects
    ser.writeLong(ser.objToId.size());
    std::vector<Object*> sortedObjs(ser.objToId.size());
    for (auto const& [obj, id] : ser.objToId) sortedObjs[id-1] = obj;

    for (Object* obj : sortedObjs) {
        ser.writeLong(ser.objToId[obj]); // ID
        if (auto s = dynamic_cast<ObjString*>(obj)) {
            ser.writeByte((uint8_t)ValueType::OBJ_STRING);
            ser.writeString(s->chars);
        } else if (dynamic_cast<Array*>(obj)) {
            ser.writeByte((uint8_t)ValueType::ARRAY);
        } else if (dynamic_cast<JsonObject*>(obj)) {
            ser.writeByte((uint8_t)ValueType::OBJECT);
        } else if (auto fn = dynamic_cast<Function*>(obj)) {
            ser.writeByte((uint8_t)ValueType::CALLABLE);
            ser.writeString(fn->name);
            ser.writeInt(fn->arity_val);
        } else {
            ser.writeByte((uint8_t)ValueType::NIL); // Unknown
        }
    }

    // Envs
    ser.writeLong(ser.envToId.size());
    std::vector<Environment*> sortedEnvs(ser.envToId.size());
    for (auto const& [env, id] : ser.envToId) sortedEnvs[id-1] = env;
    
    for (Environment* env : sortedEnvs) {
        ser.writeLong(ser.envToId[env]);
    }

    // 3. Write Data
    for (Object* obj : sortedObjs) {
        ser.writeLong(ser.objToId[obj]);
        if (auto arr = dynamic_cast<Array*>(obj)) {
            ser.writeLong(arr->elements.size());
            for (const auto& v : arr->elements) ser.writeValue(v);
        } else if (auto map = dynamic_cast<JsonObject*>(obj)) {
            ser.writeLong(map->properties.size());
            for (const auto& p : map->properties) {
                ser.writeString(p.first->chars);
                ser.writeValue(p.second);
            }
        } else if (auto fn = dynamic_cast<Function*>(obj)) {
            // Chunk
            ser.writeLong(fn->chunk->code.size());
            ser.out.write((char*)fn->chunk->code.data(), fn->chunk->code.size());
            
            ser.writeLong(fn->chunk->constants.size());
            for (const auto& v : fn->chunk->constants) ser.writeValue(v);
            
            ser.writeLong(fn->chunk->lines.size());
            ser.out.write((char*)fn->chunk->lines.data(), fn->chunk->lines.size() * sizeof(int));
            
            ser.writeLong(ser.getEnvId(fn->closure.get()));
        }
    }

    for (Environment* env : sortedEnvs) {
        ser.writeLong(ser.envToId[env]);
        ser.writeLong(ser.getEnvId(env->enclosing.get()));
        ser.writeLong(env->values.size());
        for (const auto& p : env->values) {
            ser.writeString(p.first);
            ser.writeValue(p.second);
        }
    }

    // 4. Roots
    ser.writeLong(vm.globals.size());
    for (const auto& p : vm.globals) {
        ser.writeString(p.first);
        ser.writeValue(p.second);
    }

    // Stack (Adjusted)
    ser.writeLong(stackSize - itemsToRemove + 1);
    for (size_t i = 0; i < stackSize - itemsToRemove; i++) ser.writeValue(vm.stack[i]);
    ser.writeValue(returnValue);

    ser.writeLong(vm.frames.size());
    for (const auto& f : vm.frames) {
        ser.writeLong(ser.getObjectId(f.function));
        ser.writeLong(f.ip - f.function->chunk->code.data());
        ser.writeLong(f.slot_offset);
        ser.writeString(f.fileName);
        ser.writeInt(f.currentLine);
    }
    
    file.close();
    std::cout << "Checkpoint saved to " << path << std::endl;
}

void CheckpointManager::loadCheckpoint(VM& vm, const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open " + path);

    auto readByte = [&]() { uint8_t v; file.read((char*)&v, 1); return v; };
    auto readInt = [&]() { int v; file.read((char*)&v, 4); return v; };
    auto readLong = [&]() { uint64_t v; file.read((char*)&v, 8); return v; };
    auto readDouble = [&]() { double v; file.read((char*)&v, 8); return v; };
    auto readString = [&]() { 
        uint64_t len = readLong(); 
        std::string s(len, 0); 
        file.read(&s[0], len); 
        return s; 
    };

    if (readString() != "NTRN_CKPT") throw std::runtime_error("Invalid checkpoint");
    if (readInt() != 1) throw std::runtime_error("Unsupported version");
    vm.currentFileName = readString();

    vm.stack.clear();
    vm.frames.clear();
    vm.globals.clear();

    std::map<uint64_t, Object*> idToObj;
    std::map<uint64_t, std::shared_ptr<Environment>> idToEnv;

    // 1. Read Definitions
    uint64_t objCount = readLong();
    for (uint64_t i = 0; i < objCount; i++) {
        uint64_t id = readLong();
        uint8_t type = readByte();
        Object* obj = nullptr;
        
        if (type == (uint8_t)ValueType::OBJ_STRING) {
            obj = vm.internString(readString());
        } else if (type == (uint8_t)ValueType::ARRAY) {
            obj = vm.allocate<Array>();
        } else if (type == (uint8_t)ValueType::OBJECT) {
            obj = vm.allocate<JsonObject>(); // JsonObject
        } else if (type == (uint8_t)ValueType::CALLABLE) {
            std::string name = readString();
            int arity = readInt();
            obj = vm.allocate<Function>(name, arity);
        }
        idToObj[id] = obj;
    }

    uint64_t envCount = readLong();
    for (uint64_t i = 0; i < envCount; i++) {
        uint64_t id = readLong();
        idToEnv[id] = std::make_shared<Environment>();
    }

    // Helper to read value
    std::function<Value()> readValue = [&]() {
        uint8_t type = readByte();
        switch ((ValueType)type) {
            case ValueType::NIL: return Value(nullptr);
            case ValueType::BOOLEAN: return Value(readByte() != 0);
            case ValueType::NUMBER: return Value(readDouble());
            case ValueType::OBJ_STRING:
            case ValueType::ARRAY:
            case ValueType::OBJECT:
            case ValueType::CALLABLE:
            case ValueType::CLASS:
            case ValueType::INSTANCE: {
                uint64_t id = readLong();
                Object* obj = idToObj[id];
                
                if (type == (uint8_t)ValueType::OBJ_STRING) return Value((ObjString*)obj);
                if (type == (uint8_t)ValueType::ARRAY) return Value((Array*)obj);
                if (type == (uint8_t)ValueType::OBJECT) return Value(obj); // Object* -> OBJECT
                if (type == (uint8_t)ValueType::CALLABLE) return Value((Callable*)obj);
                if (type == (uint8_t)ValueType::CLASS) return Value((Class*)obj);
                if (type == (uint8_t)ValueType::INSTANCE) return Value((Instance*)obj);
                
                return Value(obj);
            }
            default: return Value(nullptr);
        }
    };

    // 2. Read Data
    for (uint64_t i = 0; i < objCount; i++) {
        uint64_t id = readLong();
        Object* obj = idToObj[id];
        
        if (auto arr = dynamic_cast<Array*>(obj)) {
            uint64_t count = readLong();
            for (uint64_t j = 0; j < count; j++) arr->elements.push_back(readValue());
        } else if (auto map = dynamic_cast<JsonObject*>(obj)) {
            uint64_t count = readLong();
            for (uint64_t j = 0; j < count; j++) {
                std::string k = readString();
                map->properties[vm.internString(k)] = readValue();
            }
        } else if (auto fn = dynamic_cast<Function*>(obj)) {
            uint64_t codeSize = readLong();
            fn->chunk->code.resize(codeSize);
            file.read((char*)fn->chunk->code.data(), codeSize);
            
            uint64_t constCount = readLong();
            for (uint64_t j = 0; j < constCount; j++) fn->chunk->constants.push_back(readValue());
            
            uint64_t lineCount = readLong();
            fn->chunk->lines.resize(lineCount);
            file.read((char*)fn->chunk->lines.data(), lineCount * sizeof(int));
            
            fn->closure = idToEnv[readLong()];
        }
    }

    for (uint64_t i = 0; i < envCount; i++) {
        uint64_t id = readLong();
        auto env = idToEnv[id];
        uint64_t encId = readLong();
        if (encId != 0) env->enclosing = idToEnv[encId];
        
        uint64_t valCount = readLong();
        for (uint64_t j = 0; j < valCount; j++) {
            std::string k = readString();
            env->values[k] = readValue();
        }
    }

    // 3. Roots
    uint64_t globalCount = readLong();
    for (uint64_t i = 0; i < globalCount; i++) {
        std::string k = readString();
        vm.globals[k] = readValue();
    }

    uint64_t stackCount = readLong();
    for (uint64_t i = 0; i < stackCount; i++) vm.stack.push_back(readValue());

    uint64_t frameCount = readLong();
    for (uint64_t i = 0; i < frameCount; i++) {
        CallFrame frame;
        frame.function = (Function*)idToObj[readLong()];
        uint64_t ipOffset = readLong();
        frame.ip = frame.function->chunk->code.data() + ipOffset;
        frame.slot_offset = readLong();
        frame.fileName = readString();
        frame.currentLine = readInt();
        vm.frames.push_back(frame);
    }
    
    // Restore IP of the VM (from top frame)
    if (!vm.frames.empty()) {
        vm.ip = vm.frames.back().ip;
        vm.chunk = vm.frames.back().function->chunk;
    }
}

} // namespace neutron
