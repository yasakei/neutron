#include "aot/qbe_codegen.h"
#include "types/obj_string.h"
#include "types/function.h"
#include "types/class.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace neutron {
namespace aot {

QbeCodegen::QbeCodegen(const Chunk* chunk, bool is_main, const std::string& const_prefix)
    : chunk_(chunk), ip_(0), is_main_(is_main), const_prefix_(const_prefix)
{}

std::string QbeCodegen::T() {
    return "%t" + std::to_string(temp_id_++);
}

std::string QbeCodegen::L() {
    return "@L" + std::to_string(label_id_++);
}

void QbeCodegen::push(const ValuePair& v, int func_idx) {
    stack_.push_back(v);
    stack_func_idx_.push_back(func_idx);
}

ValuePair QbeCodegen::pop() {
    if (stack_.empty()) return {TAG_NIL, "0"};
    ValuePair v = stack_.back();
    stack_.pop_back();
    if (!stack_func_idx_.empty()) stack_func_idx_.pop_back();
    return v;
}

ValuePair QbeCodegen::peek(int depth) const {
    if (stack_.empty()) return {TAG_NIL, "0"};
    size_t idx = stack_.size() - 1 - depth;
    if (idx >= stack_.size()) return {TAG_NIL, "$rt_nil"};
    return stack_[idx];
}

int QbeCodegen::peek_func_idx(int depth) const {
    if (stack_func_idx_.empty()) return -1;
    size_t idx = stack_func_idx_.size() - 1 - depth;
    if (idx >= stack_func_idx_.size()) return -1;
    return stack_func_idx_[idx];
}

ValuePair QbeCodegen::pop_two() {
    // Pop a, b where b is top, a is below — return a
    pop(); // b (top)
    return pop(); // a (deeper)
}

uint8_t QbeCodegen::rb() {
    if (ip_ >= chunk_->code.size()) return 0;
    return chunk_->code[ip_++];
}

uint16_t QbeCodegen::rs() {
    uint8_t b1 = rb();
    uint8_t b2 = rb();
    return (static_cast<uint16_t>(b1) << 8) | b2;
}

std::string QbeCodegen::global_sym(const std::string& name) {
    // Mangle to a safe name (no $ prefix — callers add it)
    std::string sym = "g_";
    for (char c : name) {
        if (isalnum(c) || c == '_') sym += c;
        else sym += '_';
    }
    return sym;
}

std::string QbeCodegen::const_sym(size_t index) {
    return const_prefix_ + "C" + std::to_string(index);
}

std::string QbeCodegen::label_at(size_t ip) {
    auto it = jump_targets_.find(ip);
    return it != jump_targets_.end() ? it->second : "";
}

// ============== First pass: collect info ==============

void QbeCodegen::analyze_bytecode() {
    globals_.clear();
    global_func_idx_.clear();
    const_syms_.clear();
    num_locals_ = 0;
    int max_local = -1;

    uint8_t prev_op = 0;
    int prev_func_const_idx = -1;

    size_t ip = 0;
    while (ip < chunk_->code.size()) {
        uint8_t instr = chunk_->code[ip++];
        OpCode op = static_cast<OpCode>(instr);

        switch (op) {
            case OpCode::OP_GET_LOCAL:
            case OpCode::OP_SET_LOCAL:
            case OpCode::OP_SET_LOCAL_TYPED:
            case OpCode::OP_INCREMENT_LOCAL:
            case OpCode::OP_DECREMENT_LOCAL:
            case OpCode::OP_INC_LOCAL_INT:
            case OpCode::OP_DEC_LOCAL_INT:
            case OpCode::OP_ADD_LOCAL_CONST:
            case OpCode::OP_LOOP_IF_LESS_LOCAL: {
                int slot = chunk_->code[ip];
                if (slot > max_local) max_local = slot;
                ip++;
                if (op == OpCode::OP_ADD_LOCAL_CONST || op == OpCode::OP_LOOP_IF_LESS_LOCAL) {
                    ip++;
                    if (op == OpCode::OP_LOOP_IF_LESS_LOCAL)
                        ip += 2;
                }
                prev_func_const_idx = -1;
                break;
            }
            case OpCode::OP_GET_GLOBAL:
            case OpCode::OP_SET_GLOBAL:
            case OpCode::OP_SET_GLOBAL_TYPED:
            case OpCode::OP_INCREMENT_GLOBAL: {
                uint8_t name_idx = chunk_->code[ip++];
                if (name_idx < chunk_->constants.size() &&
                    chunk_->constants[name_idx].type == ValueType::OBJ_STRING) {
                    std::string name = chunk_->constants[name_idx].as.obj_string->chars;
                    bool found = false;
                    for (const auto& g : globals_) {
                        if (g.name == name) { found = true; break; }
                    }
                    if (!found) globals_.push_back({name, global_sym(name)});
                }
                prev_func_const_idx = -1;
                break;
            }
            case OpCode::OP_DEFINE_GLOBAL:
            case OpCode::OP_DEFINE_TYPED_GLOBAL: {
                uint8_t name_idx = chunk_->code[ip++];
                if (name_idx < chunk_->constants.size() &&
                    chunk_->constants[name_idx].type == ValueType::OBJ_STRING) {
                    std::string name = chunk_->constants[name_idx].as.obj_string->chars;
                    bool found = false;
                    for (size_t si = 0; si < globals_.size(); si++) {
                        if (globals_[si].name == name) { found = true; break; }
                    }
                    if (!found) globals_.push_back({name, global_sym(name)});
                    if (prev_func_const_idx >= 0 &&
                        prev_func_const_idx < (int)chunk_->constants.size() &&
                        chunk_->constants[prev_func_const_idx].type == ValueType::CALLABLE) {
                        for (size_t si = 0; si < globals_.size(); si++) {
                            if (globals_[si].name == name) {
                                if (si >= global_func_idx_.size())
                                    global_func_idx_.resize(globals_.size(), -1);
                                global_func_idx_[si] = prev_func_const_idx;
                                break;
                            }
                        }
                    }
                }
                prev_func_const_idx = -1;
                break;
            }
            case OpCode::OP_GET_GLOBAL_FAST:
            case OpCode::OP_SET_GLOBAL_FAST: {
                ip++;
                prev_func_const_idx = -1;
                break;
            }
            case OpCode::OP_CONSTANT: {
                int const_idx = chunk_->code[ip];
                if (const_idx < (int)chunk_->constants.size() && 
                    chunk_->constants[const_idx].type == ValueType::CALLABLE) {
                    prev_func_const_idx = const_idx;
                } else {
                    prev_func_const_idx = -1;
                }
                ip++;
                break;
            }
            case OpCode::OP_GET_PROPERTY:
            case OpCode::OP_SET_PROPERTY: {
                ip++;
                prev_func_const_idx = -1;
                break;
            }
            case OpCode::OP_INVOKE: {
                ip += 2;
                prev_func_const_idx = -1;
                break;
            }
            case OpCode::OP_CONSTANT_LONG: {
                int const_idx = (chunk_->code[ip] << 8) | chunk_->code[ip + 1];
                if (const_idx < (int)chunk_->constants.size() && 
                    chunk_->constants[const_idx].type == ValueType::CALLABLE) {
                    prev_func_const_idx = const_idx;
                } else {
                    prev_func_const_idx = -1;
                }
                ip += 2;
                break;
            }
            case OpCode::OP_CLOSURE: {
                int const_idx = chunk_->code[ip];
                if (const_idx < (int)chunk_->constants.size() && 
                    chunk_->constants[const_idx].type == ValueType::CALLABLE) {
                    prev_func_const_idx = const_idx;
                } else {
                    prev_func_const_idx = -1;
                }
                ip++;
                if (ip + 1 < chunk_->code.size()) {
                    uint16_t n_up = (chunk_->code[ip] << 8) | chunk_->code[ip + 1];
                    ip += 2 + n_up * 2;
                }
                break;
            }
            case OpCode::OP_JUMP:
            case OpCode::OP_JUMP_IF_FALSE:
            case OpCode::OP_LOOP:
            case OpCode::OP_LOGICAL_AND:
            case OpCode::OP_LOGICAL_OR:
            case OpCode::OP_LESS_JUMP:
            case OpCode::OP_GREATER_JUMP:
            case OpCode::OP_EQUAL_JUMP:
            case OpCode::OP_FOR_IN_INIT:
            case OpCode::OP_FOR_IN_NEXT:
            case OpCode::OP_LOOP_HINT:
            case OpCode::OP_TYPE_GUARD:
                ip += 2;
                prev_func_const_idx = -1;
                break;
            case OpCode::OP_CALL:
            case OpCode::OP_CALL_FAST:
            case OpCode::OP_ARRAY:
            case OpCode::OP_CONST_INT8:
            case OpCode::OP_SPREAD:
                ip++;
                prev_func_const_idx = -1;
                break;
            default:
                prev_func_const_idx = -1;
                break;
        }
    }

    num_locals_ = std::max(max_local + 1, param_count_);
    if (num_locals_ < 0) num_locals_ = 0;

    for (size_t i = 0; i < chunk_->constants.size(); i++) {
        const_syms_.push_back(const_sym(i));
    }
}

// ============== Data section ==============

void QbeCodegen::emit_data_section() {
    // Tag constants must match ValueType exactly, including MODULE before CLASS.
    TAG_NIL = std::to_string(static_cast<uint64_t>(ValueType::NIL));
    TAG_BOOL = std::to_string(static_cast<uint64_t>(ValueType::BOOLEAN));
    TAG_NUM = std::to_string(static_cast<uint64_t>(ValueType::NUMBER));
    TAG_STR = std::to_string(static_cast<uint64_t>(ValueType::OBJ_STRING));
    TAG_ARRAY = std::to_string(static_cast<uint64_t>(ValueType::ARRAY));
    TAG_OBJ = std::to_string(static_cast<uint64_t>(ValueType::OBJECT));
    TAG_CALLABLE = std::to_string(static_cast<uint64_t>(ValueType::CALLABLE));
    TAG_CLASS = std::to_string(static_cast<uint64_t>(ValueType::CLASS));
    TAG_INST = std::to_string(static_cast<uint64_t>(ValueType::INSTANCE));

    // Runtime constants (only emit once for the main function)
    if (is_main_) {
        uint64_t bits_0 = 0, bits_1 = 0;
        double zero = 0.0, one = 1.0;
        memcpy(&bits_0, &zero, sizeof(bits_0));
        memcpy(&bits_1, &one, sizeof(bits_1));
        ir_ << "data $rt_nil = { l " << TAG_NIL << ", l 0 }\n";
        ir_ << "data $rt_true = { l " << TAG_BOOL << ", l 1 }\n";
        ir_ << "data $rt_false = { l " << TAG_BOOL << ", l 0 }\n";
        ir_ << "data $rt_num0 = { l " << TAG_NUM << ", l " << bits_0 << " }\n";
        ir_ << "data $rt_num1 = { l " << TAG_NUM << ", l " << bits_1 << " }\n";
    }

    // Generated constants from the pool
    for (size_t i = 0; i < chunk_->constants.size(); i++) {
        const Value& v = chunk_->constants[i];
        ir_ << "data $" << const_syms_[i] << " = { ";
        switch (v.type) {
            case ValueType::NIL:
                ir_ << "l " << TAG_NIL << ", l 0";
                break;
            case ValueType::BOOLEAN:
                ir_ << "l " << TAG_BOOL << ", l " << (v.as.boolean ? "1" : "0");
                break;
            case ValueType::NUMBER: {
                double d = v.as.number;
                uint64_t bits;
                memcpy(&bits, &d, sizeof(bits));
                ir_ << "l " << TAG_NUM << ", l " << bits;
                break;
            }
            case ValueType::OBJ_STRING: {
                std::string str = v.as.obj_string->chars;
                // Escape special chars for QBE string literal
                // QBE doesn't have string literals in data section — use bytes
                ir_ << "l " << TAG_STR << ", l $" << const_syms_[i] << "_str";
                break;
            }
            case ValueType::CALLABLE: {
                Callable* callable = v.as.callable;
                uint64_t ptr = reinterpret_cast<uint64_t>(callable);
                ir_ << "l " << TAG_CALLABLE << ", l " << ptr;
                break;
            }
            case ValueType::CLASS: {
                std::string name = v.as.klass->name;
                ir_ << "l " << TAG_CLASS << ", l $" << const_syms_[i] << "_cls";
                break;
            }
            default:
                ir_ << "l " << TAG_NIL << ", l 0";
                break;
        }
        ir_ << " }\n";

        // Emit string data separately if needed
        if (v.type == ValueType::OBJ_STRING) {
            std::string str = v.as.obj_string->chars;
            ir_ << "data $" << const_syms_[i] << "_str = { b \"" << str << "\", b 0 }\n";
        }

        // Emit class name string if needed
        if (v.type == ValueType::CLASS) {
            std::string name = v.as.klass->name;
            ir_ << "data $" << const_syms_[i] << "_cls = { b \"" << name << "\", b 0 }\n";
        }
    }

    // Global variables (only emit once for the main function)
    if (is_main_) {
        for (const auto& g : globals_) {
            ir_ << "export data $" << g.symbol << " = { l " << TAG_NIL << ", l 0 }\n";
        }
    }

    ir_ << "\n";
}

bool QbeCodegen::has_class_constants() const {
    for (const Value& v : chunk_->constants) {
        if (v.type == ValueType::CLASS) return true;
    }
    return false;
}

void QbeCodegen::emit_class_methods() {
    bool has_classes = false;
    std::ostringstream class_data;
    for (size_t i = 0; i < chunk_->constants.size(); i++) {
        const Value& v = chunk_->constants[i];
        if (v.type != ValueType::CLASS) continue;
        has_classes = true;
        Class* klass = v.as.klass;
        // Class name length + name
        uint32_t name_len = static_cast<uint32_t>(klass->name.size());
        class_data.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        class_data.write(klass->name.c_str(), name_len);
        // Method count
        uint32_t mcount = static_cast<uint32_t>(klass->methods.size());
        class_data.write(reinterpret_cast<const char*>(&mcount), sizeof(mcount));
        for (const auto& [key, val] : klass->methods) {
            Function* fn = (val.type == ValueType::CALLABLE) ?
                static_cast<Function*>(val.as.callable) : nullptr;
            if (!fn) continue;
            // Method name length + name
            uint32_t mname_len = static_cast<uint32_t>(key->chars.size());
            class_data.write(reinterpret_cast<const char*>(&mname_len), sizeof(mname_len));
            class_data.write(key->chars.c_str(), mname_len);
            // Arity
            uint32_t arity = static_cast<uint32_t>(fn->arity_val);
            class_data.write(reinterpret_cast<const char*>(&arity), sizeof(arity));
            // Bytecode size + bytecodes
            uint32_t bc_size = static_cast<uint32_t>(fn->chunk->code.size());
            class_data.write(reinterpret_cast<const char*>(&bc_size), sizeof(bc_size));
            class_data.write(reinterpret_cast<const char*>(fn->chunk->code.data()), bc_size);
            // Constant count + constants
            uint32_t const_count = static_cast<uint32_t>(fn->chunk->constants.size());
            class_data.write(reinterpret_cast<const char*>(&const_count), sizeof(const_count));
            for (const Value& cv : fn->chunk->constants) {
                uint8_t tag = 0;
                switch (cv.type) {
                    case ValueType::NIL: tag = 0; break;
                    case ValueType::BOOLEAN: tag = 1; break;
                    case ValueType::NUMBER: tag = 2; break;
                    case ValueType::OBJ_STRING: tag = 3; break;
                    default: tag = 0; break;
                }
                class_data.write(reinterpret_cast<const char*>(&tag), 1);
                switch (cv.type) {
                    case ValueType::NIL: break;
                    case ValueType::BOOLEAN: {
                        uint8_t b = cv.as.boolean ? 1 : 0;
                        class_data.write(reinterpret_cast<const char*>(&b), 1);
                        break;
                    }
                    case ValueType::NUMBER: {
                        double d = cv.as.number;
                        class_data.write(reinterpret_cast<const char*>(&d), sizeof(d));
                        break;
                    }
                    case ValueType::OBJ_STRING: {
                        uint32_t slen = static_cast<uint32_t>(cv.as.obj_string->chars.size());
                        class_data.write(reinterpret_cast<const char*>(&slen), sizeof(slen));
                        class_data.write(cv.as.obj_string->chars.c_str(), slen);
                        break;
                    }
                    default: break;
                }
            }
        }
    }
    if (!has_classes) return;
    // Emit terminator (0xFFFFFFFF)
    uint32_t term = 0xFFFFFFFF;
    class_data.write(reinterpret_cast<const char*>(&term), sizeof(term));
    std::string data = class_data.str();
    ir_ << "data $rt_class_data = { b";
    for (size_t i = 0; i < data.size(); i++) {
        if (i % 16 == 0) ir_ << "\n    ";
        ir_ << " " << static_cast<int>(static_cast<unsigned char>(data[i]));
    }
    ir_ << "\n}\n\n";
}

// ============== Function prologue/epilogue ==============

void QbeCodegen::emit_function_start(const std::string& name, int param_count) {
    end_label_ = L();
    ir_ << "export function w $" << name << "(";
    for (int i = 0; i < param_count; i++) {
        if (i > 0) ir_ << ", ";
        ir_ << "w %p" << i << "_t, l %p" << i << "_d";
    }
    ir_ << ") {\n";
    ir_ << "@start\n";
}

void QbeCodegen::emit_function_end() {
    ir_ << end_label_ << "\n";
    ir_ << "    ret 0\n";
    ir_ << "}\n";
}

void QbeCodegen::emit_locals_init() {
    // Pre-allocate QBE temps for each local variable slot
    for (int i = 0; i < num_locals_; i++) {
        locals_.push_back({T(), T()}); // tag and data temps
        // Initialize to nil ONLY if NOT a parameter. 
        // Parameters are initialized via copy from %p_t/d below.
        if (i >= param_count_) {
            ir_ << "    " << locals_[i].tag << " =w copy " << TAG_NIL << "\n";
            ir_ << "    " << locals_[i].data << " =l copy 0\n";
        }
    }
}

// ============== Constant emitter ==============

ValuePair QbeCodegen::emit_constant(size_t index) {
    if (index >= chunk_->constants.size()) {
        ValuePair v = {T(), T()};
        ir_ << "    " << v.tag << " =w copy " << TAG_NIL << "\n";
        ir_ << "    " << v.data << " =l copy 0\n";
        return v;
    }
    ValuePair v = {T(), T()};
    std::string sym = const_syms_[index];
    // Load tag and data from the constant pool using loadld (load 2 words = 16 bytes)
    // QBE: loadl loads a 64-bit value, stored in alternating tag/data slots
    std::string base = global_base(sym);
    std::string off = T();
    ir_ << "    " << off << " =l add " << base << ", 8\n";
    ir_ << "    " << v.tag << " =w loadw " << base << "\n";
    ir_ << "    " << v.data << " =l loadl " << off << "\n";
    return v;
}

std::string QbeCodegen::global_base(const std::string& sym) {
    std::string addr = T();
    ir_ << "    " << addr << " =l copy $" << sym << "\n";
    return addr;
}

void QbeCodegen::emit_global_load(const std::string& sym, const std::string& tag, const std::string& data) {
    std::string base = global_base(sym);
    ir_ << "    " << tag << " =w loadw " << base << "\n";
    std::string off = T();
    ir_ << "    " << off << " =l add " << base << ", 8\n";
    ir_ << "    " << data << " =l loadl " << off << "\n";
}

void QbeCodegen::emit_global_store(const std::string& sym, const std::string& tag, const std::string& data) {
    std::string base = global_base(sym);
    ir_ << "    storew " << tag << ", " << base << "\n";
    std::string off = T();
    ir_ << "    " << off << " =l add " << base << ", 8\n";
    ir_ << "    storel " << data << ", " << off << "\n";
}

// ============== Instruction emitters ==============

void QbeCodegen::emit_return() {
    // Pop the return value and store to $rt_ret
    ValuePair ret = pop();
    emit_global_store("rt_ret", ret.tag, ret.data);
    ir_ << "    jmp " << end_label_ << "\n";
    last_was_term_ = true;
}

void QbeCodegen::emit_const(size_t index) {
    ValuePair v = emit_constant(index);
    int func_idx = -1;
    if (index < chunk_->constants.size() &&
        chunk_->constants[index].type == ValueType::CALLABLE) {
        func_idx = static_cast<int>(index);
    }
    push(v, func_idx);
}

void QbeCodegen::emit_nil() {
    ValuePair v = {T(), T()};
    ir_ << "    " << v.tag << " =w copy " << TAG_NIL << "\n";
    ir_ << "    " << v.data << " =l copy 0\n";
    push(v);
}

void QbeCodegen::emit_bool(bool val) {
    ValuePair v = {T(), T()};
    ir_ << "    " << v.tag << " =w copy " << TAG_BOOL << "\n";
    ir_ << "    " << v.data << " =l copy " << (val ? "1" : "0") << "\n";
    push(v);
}

void QbeCodegen::emit_pop() {
    pop();
}

void QbeCodegen::emit_dup() {
    ValuePair top = peek();
    ValuePair dup = {T(), T()};
    ir_ << "    " << dup.tag << " =w copy " << top.tag << "\n";
    ir_ << "    " << dup.data << " =l copy " << top.data << "\n";
    push(dup);
}

void QbeCodegen::emit_get_local(uint8_t slot) {
    if (slot >= locals_.size()) {
        emit_nil();
        return;
    }
    ValuePair v = {T(), T()};
    ir_ << "    " << v.tag << " =w copy " << locals_[slot].tag << "\n";
    ir_ << "    " << v.data << " =l copy " << locals_[slot].data << "\n";
    push(v);
}

void QbeCodegen::emit_set_local(uint8_t slot) {
    ValuePair val = pop();
    if (slot >= locals_.size()) return;
    ir_ << "    " << locals_[slot].tag << " =w copy " << val.tag << "\n";
    ir_ << "    " << locals_[slot].data << " =l copy " << val.data << "\n";
    // Also push back (SET_LOCAL keeps value on stack)
    push(val);
}

void QbeCodegen::emit_get_global(uint8_t name_idx) {
    if (name_idx >= chunk_->constants.size() ||
        chunk_->constants[name_idx].type != ValueType::OBJ_STRING) {
        emit_nil();
        return;
    }
    std::string name = chunk_->constants[name_idx].as.obj_string->chars;
    std::string sym = global_sym(name);

    ValuePair v = {T(), T()};
    emit_global_load(sym, v.tag, v.data);
    int func_idx = -1;
    for (size_t slot = 0; slot < globals_.size(); slot++) {
        if (globals_[slot].name == name) {
            if (slot < global_func_idx_.size()) func_idx = global_func_idx_[slot];
            break;
        }
    }
    push(v, func_idx);
}

void QbeCodegen::emit_set_global(uint8_t name_idx) {
    if (name_idx >= chunk_->constants.size() ||
        chunk_->constants[name_idx].type != ValueType::OBJ_STRING) {
        pop();
        return;
    }
    std::string name = chunk_->constants[name_idx].as.obj_string->chars;
    std::string sym = global_sym(name);
    int callee_func_idx = -1;
    if (!stack_func_idx_.empty()) callee_func_idx = stack_func_idx_.back();
    ValuePair val = pop();
    // Track func index by name — find slot if exists
    for (size_t slot = 0; slot < globals_.size(); slot++) {
        if (globals_[slot].name == name) {
            if (slot >= global_func_idx_.size()) global_func_idx_.resize(globals_.size(), -1);
            global_func_idx_[slot] = callee_func_idx;
            break;
        }
    }
    emit_global_store(sym, val.tag, val.data);
    push(val, callee_func_idx);
}

void QbeCodegen::emit_define_global(uint8_t name_idx) {
    emit_set_global(name_idx);
    pop(); // define_global pops the value
}

void QbeCodegen::emit_get_global_fast(uint8_t slot) {
    if (slot >= globals_.size()) {
        emit_nil();
        return;
    }
    std::string sym = globals_[slot].symbol;
    ValuePair v = {T(), T()};
    emit_global_load(sym, v.tag, v.data);
    int func_idx = (slot < global_func_idx_.size()) ? global_func_idx_[slot] : -1;
    push(v, func_idx);
}

void QbeCodegen::emit_set_global_fast(uint8_t slot) {
    if (slot >= globals_.size()) {
        pop();
        return;
    }
    std::string sym = globals_[slot].symbol;
    int callee_func_idx = -1;
    if (!stack_func_idx_.empty()) callee_func_idx = stack_func_idx_.back();
    ValuePair val = pop();
    if (slot >= global_func_idx_.size()) global_func_idx_.resize(globals_.size(), -1);
    global_func_idx_[slot] = callee_func_idx;
    emit_global_store(sym, val.tag, val.data);
    push(val, callee_func_idx);
}

void QbeCodegen::emit_arithmetic(OpCode op) {
    ValuePair b = pop(); // top
    ValuePair a = pop(); // below

    ValuePair result = {T(), T()};
    std::string tag_check_a = T();
    std::string tag_check_b = T();
    std::string both_num = T();
    std::string label_num = L();
    std::string label_done = L();
    std::string label_fallback = L();

    // Check if both operands are numbers
    ir_ << "    " << tag_check_a << " =w ceqw " << a.tag << ", " << TAG_NUM << "\n";
    ir_ << "    " << tag_check_b << " =w ceqw " << b.tag << ", " << TAG_NUM << "\n";
    ir_ << "    " << both_num << " =w and " << tag_check_a << ", " << tag_check_b << "\n";
    ir_ << "    jnz " << both_num << ", " << label_num << ", " << label_fallback << "\n";

    // Numeric path: convert data to double, operate, convert back
    ir_ << label_num << "\n";
    std::string da = T();
    std::string db = T();
    std::string dres = T();
    ir_ << "    " << da << " =d cast " << a.data << "\n";
    ir_ << "    " << db << " =d cast " << b.data << "\n";

    bool is_mod = (op == OpCode::OP_MODULO || op == OpCode::OP_MOD_INT);

    if (!is_mod) {
        switch (op) {
            case OpCode::OP_ADD:
            case OpCode::OP_ADD_INT:
                ir_ << "    " << dres << " =d add " << da << ", " << db << "\n";
                break;
            case OpCode::OP_SUBTRACT:
            case OpCode::OP_SUB_INT:
                ir_ << "    " << dres << " =d sub " << da << ", " << db << "\n";
                break;
            case OpCode::OP_MULTIPLY:
            case OpCode::OP_MUL_INT:
                ir_ << "    " << dres << " =d mul " << da << ", " << db << "\n";
                break;
            case OpCode::OP_DIVIDE:
            case OpCode::OP_DIV_INT:
                ir_ << "    " << dres << " =d div " << da << ", " << db << "\n";
                break;
            default:
                ir_ << "    " << dres << " =d copy " << da << "\n";
                break;
        }

        ir_ << "    " << result.tag << " =w copy " << TAG_NUM << "\n";
        ir_ << "    " << result.data << " =l cast " << dres << "\n";
        ir_ << "    jmp " << label_done << "\n";
    } else {
        // Modulo: no numeric path — jump directly to runtime fallback
        ir_ << "    jmp " << label_fallback << "\n";
    }

    // Fallback: call runtime function
    ir_ << label_fallback << "\n";
    std::string rt_fn;
    switch (op) {
        case OpCode::OP_ADD:
        case OpCode::OP_ADD_INT:      rt_fn = "$rt_add"; break;
        case OpCode::OP_SUBTRACT:
        case OpCode::OP_SUB_INT:      rt_fn = "$rt_sub"; break;
        case OpCode::OP_MULTIPLY:
        case OpCode::OP_MUL_INT:      rt_fn = "$rt_mul"; break;
        case OpCode::OP_DIVIDE:
        case OpCode::OP_DIV_INT:      rt_fn = "$rt_div"; break;
        case OpCode::OP_MODULO:
        case OpCode::OP_MOD_INT:      rt_fn = "$rt_mod"; break;
        default:                      rt_fn = "$rt_add"; break;
    }
    // Runtime helpers take (tag_a, data_a, tag_b, data_b) and return (tag, data)
    // Store result into QBE data return slot
    std::string rt_temp = T();
    ir_ << "    call " << rt_fn << "(w " << a.tag << ", l " << a.data
        << ", w " << b.tag << ", l " << b.data << ")\n";
    // Runtime stores result in a global return buffer — load it
    emit_global_load("rt_ret", result.tag, result.data);
    ir_ << "    jmp " << label_done << "\n";

    ir_ << label_done << "\n";
    push(result);
}

void QbeCodegen::emit_compare(OpCode op) {
    ValuePair b = pop();
    ValuePair a = pop();

    ValuePair result = {T(), T()};
    std::string label_num = L();
    std::string label_done = L();
    std::string label_fallback = L();
    std::string check_a = T();
    std::string check_b = T();
    std::string both_num = T();

    ir_ << "    " << check_a << " =w ceqw " << a.tag << ", " << TAG_NUM << "\n";
    ir_ << "    " << check_b << " =w ceqw " << b.tag << ", " << TAG_NUM << "\n";
    ir_ << "    " << both_num << " =w and " << check_a << ", " << check_b << "\n";
    ir_ << "    jnz " << both_num << ", " << label_num << ", " << label_fallback << "\n";

    // Numeric comparison
    ir_ << label_num << "\n";
    std::string da = T();
    std::string db = T();
    std::string dcmp = T();
    ir_ << "    " << da << " =d cast " << a.data << "\n";
    ir_ << "    " << db << " =d cast " << b.data << "\n";

    switch (op) {
        case OpCode::OP_EQUAL:
        case OpCode::OP_EQUAL_INT:
        case OpCode::OP_EQUAL_JUMP:
            ir_ << "    " << dcmp << " =w ceqd " << da << ", " << db << "\n";
            break;
        case OpCode::OP_NOT_EQUAL:
            ir_ << "    " << dcmp << " =w cned " << da << ", " << db << "\n";
            break;
        case OpCode::OP_LESS:
        case OpCode::OP_LESS_INT:
        case OpCode::OP_LESS_JUMP:
            ir_ << "    " << dcmp << " =w cltd " << da << ", " << db << "\n";
            break;
        case OpCode::OP_GREATER:
        case OpCode::OP_GREATER_INT:
        case OpCode::OP_GREATER_JUMP:
            ir_ << "    " << dcmp << " =w cgtd " << da << ", " << db << "\n";
            break;
        default:
            ir_ << "    " << dcmp << " =w ceqd " << da << ", " << db << "\n";
            break;
    }

    // Convert w (0/1) to our boolean Value representation (tag=bool, data=l)
    // Need to extend w to l
    std::string ext = T();
    ir_ << "    " << ext << " =l extsw " << dcmp << "\n";
    ir_ << "    " << result.tag << " =w copy " << TAG_BOOL << "\n";
    ir_ << "    " << result.data << " =l copy " << ext << "\n";
    ir_ << "    jmp " << label_done << "\n";

    // Fallback: runtime comparison
    ir_ << label_fallback << "\n";
    std::string rt_fn;
    switch (op) {
        case OpCode::OP_EQUAL:
        case OpCode::OP_EQUAL_INT:
        case OpCode::OP_EQUAL_JUMP:       rt_fn = "$rt_eq"; break;
        case OpCode::OP_NOT_EQUAL:        rt_fn = "$rt_neq"; break;
        case OpCode::OP_LESS:
        case OpCode::OP_LESS_INT:
        case OpCode::OP_LESS_JUMP:        rt_fn = "$rt_lt"; break;
        case OpCode::OP_GREATER:
        case OpCode::OP_GREATER_INT:
        case OpCode::OP_GREATER_JUMP:     rt_fn = "$rt_gt"; break;
        default:                          rt_fn = "$rt_eq"; break;
    }
    ir_ << "    call " << rt_fn << "(w " << a.tag << ", l " << a.data
        << ", w " << b.tag << ", l " << b.data << ")\n";
    emit_global_load("rt_ret", result.tag, result.data);
    ir_ << "    jmp " << label_done << "\n";

    ir_ << label_done << "\n";
    push(result);
}

void QbeCodegen::emit_not() {
    ValuePair val = pop();
    ValuePair result = {T(), T()};

    // Truthiness check:
    // false and nil are falsy; everything else is truthy
    std::string is_false_tag = T();
    std::string is_nil_tag = T();
    std::string is_falsy = T();
    std::string label_false = L();
    std::string label_done = L();
    std::string label_true = L();

    ir_ << "    " << is_false_tag << " =w ceqw " << val.tag << ", " << TAG_BOOL << "\n";
    ir_ << "    " << is_nil_tag << " =w ceqw " << val.tag << ", " << TAG_NIL << "\n";
    ir_ << "    " << is_falsy << " =w or " << is_false_tag << ", " << is_nil_tag << "\n";
    ir_ << "    jnz " << is_falsy << ", " << label_false << ", " << label_true << "\n";

    // Check if bool value is false too
    ir_ << label_false << "\n";
    std::string is_zero = T();
    ir_ << "    " << is_zero << " =w ceql " << val.data << ", 0\n";
    ir_ << "    " << result.tag << " =w copy " << TAG_BOOL << "\n";
    ir_ << "    " << result.data << " =l extsw " << is_zero << "\n";
    ir_ << "    jmp " << label_done << "\n";

    // Truthy — !truthy = false
    ir_ << label_true << "\n";
    ir_ << "    " << result.tag << " =w copy " << TAG_BOOL << "\n";
    ir_ << "    " << result.data << " =l copy 0\n";
    ir_ << "    jmp " << label_done << "\n";

    ir_ << label_done << "\n";
    push(result);
}

void QbeCodegen::emit_negate(bool is_trusted) {
    ValuePair val = pop();
    ValuePair result = {T(), T()};

    std::string check_num = T();
    std::string label_num = L();
    std::string label_done = L();
    std::string label_fallback = L();

    ir_ << "    " << check_num << " =w ceqw " << val.tag << ", " << TAG_NUM << "\n";
    ir_ << "    jnz " << check_num << ", " << label_num << ", " << label_fallback << "\n";

    ir_ << label_num << "\n";
    std::string dval = T();
    std::string dneg = T();
    ir_ << "    " << dval << " =d cast " << val.data << "\n";
    ir_ << "    " << dneg << " =d neg " << dval << "\n";
    ir_ << "    " << result.tag << " =w copy " << TAG_NUM << "\n";
    ir_ << "    " << result.data << " =l cast " << dneg << "\n";
    ir_ << "    jmp " << label_done << "\n";

    ir_ << label_fallback << "\n";
    ir_ << "    call $rt_neg(w " << val.tag << ", l " << val.data << ")\n";
    emit_global_load("rt_ret", result.tag, result.data);
    ir_ << "    jmp " << label_done << "\n";

    ir_ << label_done << "\n";
    push(result);
}

void QbeCodegen::emit_bitwise(OpCode op) {
    ValuePair b = pop();
    ValuePair a = pop();
    ValuePair result = {T(), T()};

    // Bitwise ops only work on integers — use runtime helpers
    std::string rt_fn;
    switch (op) {
        case OpCode::OP_BITWISE_AND: rt_fn = "$rt_band"; break;
        case OpCode::OP_BITWISE_OR:  rt_fn = "$rt_bor"; break;
        case OpCode::OP_BITWISE_XOR: rt_fn = "$rt_bxor"; break;
        case OpCode::OP_LEFT_SHIFT:  rt_fn = "$rt_shl"; break;
        case OpCode::OP_RIGHT_SHIFT: rt_fn = "$rt_shr"; break;
        default:                     rt_fn = "$rt_band"; break;
    }
    ir_ << "    call " << rt_fn << "(w " << a.tag << ", l " << a.data
        << ", w " << b.tag << ", l " << b.data << ")\n";
    emit_global_load("rt_ret", result.tag, result.data);
    push(result);
}

void QbeCodegen::emit_say() {
    ValuePair val = pop();
    ir_ << "    call $rt_say(w " << val.tag << ", l " << val.data << ")\n";
}

void QbeCodegen::emit_jump(uint16_t offset) {
    size_t target = ip_ + offset;
    std::string lbl = label_at(target);
    if (!lbl.empty()) {
        ir_ << "    jmp " << lbl << "\n";
        last_was_term_ = true;
    }
}

void QbeCodegen::emit_jump_if_false(uint16_t offset) {
    ValuePair cond = pop();
    size_t target = ip_ + offset;
    std::string lbl_taken = label_at(target);
    if (lbl_taken.empty()) return;

    // Check truthiness: false and nil are falsy
    std::string is_nil = T();
    std::string is_false_tag = T();
    std::string check_val = T();
    std::string is_false_val = T();
    std::string is_falsy = T();
    std::string fallthrough = L();

    ir_ << "    " << is_nil << " =w ceqw " << cond.tag << ", " << TAG_NIL << "\n";
    ir_ << "    " << is_false_tag << " =w ceqw " << cond.tag << ", " << TAG_BOOL << "\n";
    ir_ << "    " << check_val << " =w ceql " << cond.data << ", 0\n";
    ir_ << "    " << is_false_val << " =w and " << is_false_tag << ", " << check_val << "\n";
    ir_ << "    " << is_falsy << " =w or " << is_nil << ", " << is_false_val << "\n";

    ir_ << "    jnz " << is_falsy << ", " << lbl_taken << ", " << fallthrough << "\n";
    ir_ << fallthrough << "\n";
}

void QbeCodegen::emit_loop(uint16_t offset) {
    size_t target = (ip_ > offset) ? (ip_ - offset) : 0;
    std::string lbl = label_at(target);
    if (!lbl.empty()) {
        ir_ << "    jmp " << lbl << "\n";
        last_was_term_ = true;
    }
}

void QbeCodegen::emit_loop_if_less_local(uint8_t slot, uint8_t const_idx, uint16_t offset) {
    // Fused: if (locals[slot] < constant[const_idx]) goto loop_start; else goto exit
    size_t exit_target = ip_ + offset;

    std::string lbl_exit = label_at(exit_target);
    if (lbl_exit.empty()) return;

    std::string tag_check = T();
    std::string label_check = L();

    // Check if local is a number
    ir_ << "    " << tag_check << " =w ceqw " << locals_[slot].tag << ", " << TAG_NUM << "\n";
    ir_ << "    jnz " << tag_check << ", " << label_check << ", " << lbl_exit << "\n";

    ir_ << label_check << "\n";
    std::string dlocal = T();
    std::string dconst = T();
    std::string dconst_tmp = T();
    ir_ << "    " << dlocal << " =d cast " << locals_[slot].data << "\n";
    // Load constant value
    if (const_idx < chunk_->constants.size() &&
        chunk_->constants[const_idx].type == ValueType::NUMBER) {
        double cval = chunk_->constants[const_idx].as.number;
        ir_ << "    " << dconst << " =d copy d_" << cval << "\n";
    } else {
        ir_ << "    " << dconst << " =d copy d_0.0\n";
    }

    std::string cmp = T();
    ir_ << "    " << cmp << " =w cltd " << dlocal << ", " << dconst << "\n";
    // If NOT less, jump to exit
    std::string cont_label = L();
    ir_ << "    jnz " << cmp << ", " << cont_label << ", " << lbl_exit << "\n";
    ir_ << cont_label << "\n";
    // Otherwise, fall through to loop body (which follows this instruction)
    // The loop back jump is emitted separately at the end of the loop body
}

void QbeCodegen::emit_logical_and(uint16_t offset) {
    ValuePair a = peek(); // don't pop — keep for short-circuit check
    size_t target = ip_ + offset;
    std::string lbl_skip = label_at(target);
    if (lbl_skip.empty()) return;

    // Check if a is falsy
    std::string is_nil = T();
    std::string is_false_tag = T();
    std::string check_false_val = T();
    std::string is_false_val = T();
    std::string is_falsy = T();

    ir_ << "    " << is_nil << " =w ceqw " << a.tag << ", " << TAG_NIL << "\n";
    ir_ << "    " << is_false_tag << " =w ceqw " << a.tag << ", " << TAG_BOOL << "\n";
    ir_ << "    " << check_false_val << " =w ceql " << a.data << ", 0\n";
    ir_ << "    " << is_false_val << " =w and " << is_false_tag << ", " << check_false_val << "\n";
    ir_ << "    " << is_falsy << " =w or " << is_nil << ", " << is_false_val << "\n";
    std::string cont_label = L();
    ir_ << "    jnz " << is_falsy << ", " << lbl_skip << ", " << cont_label << "\n";
    ir_ << cont_label << "\n";
}

void QbeCodegen::emit_logical_or(uint16_t offset) {
    ValuePair a = peek();
    size_t target = ip_ + offset;
    std::string lbl_skip = label_at(target);
    if (lbl_skip.empty()) return;

    // Check if a is truthy (not nil and not false)
    std::string is_nil = T();
    std::string is_false_tag = T();
    std::string check_false_val = T();
    std::string is_false_val = T();
    std::string is_falsy = T();
    std::string is_truthy = T();

    ir_ << "    " << is_nil << " =w ceqw " << a.tag << ", " << TAG_NIL << "\n";
    ir_ << "    " << is_false_tag << " =w ceqw " << a.tag << ", " << TAG_BOOL << "\n";
    ir_ << "    " << check_false_val << " =w ceql " << a.data << ", 0\n";
    ir_ << "    " << is_false_val << " =w and " << is_false_tag << ", " << check_false_val << "\n";
    ir_ << "    " << is_falsy << " =w or " << is_nil << ", " << is_false_val << "\n";
    // Jump if truthy (skip second evaluation)
    std::string cont_label = L();
    ir_ << "    jnz " << is_falsy << ", " << cont_label << ", " << lbl_skip << "\n";
    ir_ << cont_label << "\n";
}

void QbeCodegen::emit_call(uint8_t arg_count) {
    // Stack: ... callable, arg1, arg2, ..., argN
    // Pop callable and args, push result
    if (stack_.size() < static_cast<size_t>(arg_count + 1)) {
        emit_nil();
        return;
    }

    // Check if callee is a known function BEFORE popping (callee is at depth arg_count)
    int callee_func_idx = peek_func_idx(arg_count);

    std::vector<ValuePair> args;
    for (int i = 0; i < arg_count; i++) {
        args.push_back(pop());
    }
    ValuePair callee = pop();
    // Reverse args to original order
    std::reverse(args.begin(), args.end());

    ValuePair result = {T(), T()};

    // Direct QBE call if callee is a known function constant
    if (callee_func_idx >= 0) {
        std::string func_name = "qbe_func_" + std::to_string(callee_func_idx);
        ir_ << "    call $" << func_name << "(";
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) ir_ << ", ";
            ir_ << "w " << args[i].tag << ", l " << args[i].data;
        }
        ir_ << ")\n";
        emit_global_load("rt_ret", result.tag, result.data);
        push(result);
        return;
    }

    // Fallback: use runtime dispatcher $rt_call
    // Note: '...' tells QBE to emit Oargv, needed for Apple ARM64 va_start
    ir_ << "    call $rt_call(w " << callee.tag << ", l " << callee.data
        << ", w " << static_cast<int>(arg_count) << ", ...";
    for (const auto& arg : args) {
        ir_ << ", w " << arg.tag << ", l " << arg.data;
    }
    ir_ << ")\n";
    emit_global_load("rt_ret", result.tag, result.data);
    push(result);
}

void QbeCodegen::emit_closure(uint8_t func_idx) {
    uint16_t n_up = rs();

    // Collect upvalue values from locals or enclosing closure
    std::vector<ValuePair> upvalue_values;
    for (uint16_t i = 0; i < n_up; i++) {
        uint8_t is_local = rb();
        uint8_t index = rb();

        ValuePair uv_val = {T(), T()};
        if (is_local) {
            // Upvalue is a local variable in this function
            if (index < locals_.size()) {
                ir_ << "    " << uv_val.tag << " =w copy " << locals_[index].tag << "\n";
                ir_ << "    " << uv_val.data << " =l copy " << locals_[index].data << "\n";
            } else {
                ir_ << "    " << uv_val.tag << " =w copy " << TAG_NIL << "\n";
                ir_ << "    " << uv_val.data << " =l copy 0\n";
            }
        } else {
            // Upvalue is from an enclosing closure — call runtime helper
            ir_ << "    call $rt_get_upvalue(w " << static_cast<int>(index) << ")\n";
            emit_global_load("rt_ret", uv_val.tag, uv_val.data);
        }
        upvalue_values.push_back(uv_val);
    }

    // Load the function constant (contains the Function* for this closure)
    ValuePair func_val = emit_constant(func_idx);

    // Call rt_closure(func_data, n_up, tag_0, data_0, ...)
    ValuePair result = {T(), T()};
    ir_ << "    call $rt_closure(l " << func_val.data
        << ", w " << static_cast<int>(n_up) << ", ...";
    for (const auto& uv : upvalue_values) {
        ir_ << ", w " << uv.tag << ", l " << uv.data;
    }
    ir_ << ")\n";
    emit_global_load("rt_ret", result.tag, result.data);
    push(result);
}

void QbeCodegen::emit_get_upvalue(uint8_t slot) {
    ValuePair result = {T(), T()};
    // Read upvalue from the closure - the closure is at a known location
    // For now, call runtime helper
    ir_ << "    call $rt_get_upvalue(w " << static_cast<int>(slot) << ")\n";
    emit_global_load("rt_ret", result.tag, result.data);
    push(result);
}

void QbeCodegen::emit_set_upvalue(uint8_t slot) {
    ValuePair val = pop();
    ir_ << "    call $rt_set_upvalue(w " << static_cast<int>(slot)
        << ", w " << val.tag << ", l " << val.data << ")\n";
}

void QbeCodegen::emit_close_upvalue(uint8_t slot) {
    // Close upvalue at the given stack slot
    ir_ << "    call $rt_close_upvalue(w " << static_cast<int>(slot) << ")\n";
}

void QbeCodegen::emit_array(uint8_t size) {
    if (stack_.size() < size) {
        emit_nil();
        return;
    }

    std::vector<ValuePair> elements;
    for (int i = 0; i < size; i++) {
        elements.push_back(pop());
    }
    std::reverse(elements.begin(), elements.end());

    ValuePair result = {T(), T()};

    // Call runtime $rt_array(size, tag_0, data_0, ...)
    ir_ << "    call $rt_array(w " << static_cast<int>(size) << ", ...";
    for (const auto& elem : elements) {
        ir_ << ", w " << elem.tag << ", l " << elem.data;
    }
    ir_ << ")\n";
    emit_global_load("rt_ret", result.tag, result.data);
    push(result);
}

void QbeCodegen::emit_object(uint8_t prop_count) {
    size_t total = static_cast<size_t>(prop_count) * 2;
    if (stack_.size() < total) {
        emit_nil();
        return;
    }

    std::vector<ValuePair> pairs;
    for (size_t i = 0; i < total; i++) {
        pairs.push_back(pop());
    }
    std::reverse(pairs.begin(), pairs.end());

    ValuePair result = {T(), T()};
    ir_ << "    call $rt_obj(w " << static_cast<int>(prop_count) << ", ...";
    for (const auto& p : pairs) {
        ir_ << ", w " << p.tag << ", l " << p.data;
    }
    ir_ << ")\n";
    emit_global_load("rt_ret", result.tag, result.data);
    push(result);
}

void QbeCodegen::emit_index_get() {
    ValuePair index = pop();
    ValuePair obj = pop();
    ValuePair result = {T(), T()};
    ir_ << "    call $rt_idx_r(w " << obj.tag << ", l " << obj.data
        << ", w " << index.tag << ", l " << index.data << ")\n";
    emit_global_load("rt_ret", result.tag, result.data);
    push(result);
}

void QbeCodegen::emit_index_set() {
    ValuePair val = pop();
    ValuePair index = pop();
    ValuePair obj = pop();
    ir_ << "    call $rt_idx_w(w " << obj.tag << ", l " << obj.data
        << ", w " << index.tag << ", l " << index.data
        << ", w " << val.tag << ", l " << val.data << ")\n";
    push(val);
}

void QbeCodegen::emit_get_property(uint8_t name_idx) {
    ValuePair obj = pop();
    ValuePair result = {T(), T()};
    std::string prop_name = "?";
    if (name_idx < chunk_->constants.size() &&
        chunk_->constants[name_idx].type == ValueType::OBJ_STRING) {
        prop_name = chunk_->constants[name_idx].as.obj_string->chars;
    }
    ir_ << "    call $rt_getprop(w " << obj.tag << ", l " << obj.data
        << ", l $" << const_syms_[name_idx] << "_str)\n";
    emit_global_load("rt_ret", result.tag, result.data);
    push(result);
}

void QbeCodegen::emit_set_property(uint8_t name_idx) {
    ValuePair val = pop();
    ValuePair obj = pop();
    ir_ << "    call $rt_setprop(w " << obj.tag << ", l " << obj.data
        << ", l $" << const_syms_[name_idx] << "_str"
        << ", w " << val.tag << ", l " << val.data << ")\n";
    push(val);
}

void QbeCodegen::emit_this() {
    // `this` is passed as the first parameter (%p0) to method QBE functions
    ValuePair v = {T(), T()};
    ir_ << "    " << v.tag << " =w copy %p0_t\n";
    ir_ << "    " << v.data << " =l copy %p0_d\n";
    push(v);
}

void QbeCodegen::emit_invoke(uint8_t name_idx, uint8_t arg_count) {
    // Similar to call but looks up method on object first
    if (stack_.size() < static_cast<size_t>(arg_count + 1)) {
        emit_nil();
        return;
    }

    std::vector<ValuePair> args;
    for (int i = 0; i < arg_count; i++) {
        args.push_back(pop());
    }
    ValuePair obj = pop();
    std::reverse(args.begin(), args.end());

    ValuePair result = {T(), T()};
    ir_ << "    call $rt_invoke(l " << obj.data
        << ", l $" << const_syms_[name_idx] << "_str"
        << ", w " << static_cast<int>(arg_count) << ", ...";
    for (const auto& arg : args) {
        ir_ << ", w " << arg.tag << ", l " << arg.data;
    }
    ir_ << ")\n";
    emit_global_load("rt_ret", result.tag, result.data);
    push(result);
}

void QbeCodegen::emit_try() {
    // Exception handling not implemented in QBE AOT v1 — no-op
}

void QbeCodegen::emit_end_try() {
}

void QbeCodegen::emit_throw() {
    ValuePair val = pop();
    ir_ << "    call $rt_throw(w " << val.tag << ", l " << val.data << ")\n";
    ir_ << "    ret 1\n";
}

void QbeCodegen::emit_break() {
    // Handled by structured control flow in bytecode — already compiled to jumps
}

void QbeCodegen::emit_continue() {
}

void QbeCodegen::emit_for_in_init(uint16_t offset) {
    // Stack: ... obj -> ... obj, keys_array, index
    size_t exit_target = ip_ + offset;
    ValuePair obj = peek();
    ValuePair keys = {T(), T()};
    ValuePair index = {T(), T()};

    ir_ << "    call $rt_forin_init(w " << obj.tag << ", l " << obj.data << ")\n";
    emit_global_load("rt_ret", keys.tag, keys.data);
    ir_ << "    " << index.tag << " =w copy " << TAG_NUM << "\n";
    ir_ << "    " << index.data << " =l copy 0\n";
    push(keys);
    push(index);
}

void QbeCodegen::emit_for_in_next(uint16_t offset) {
    // Stack: ... obj, keys, index -> ... obj, key (or jump to exit)
    size_t exit_target = ip_ + offset;
    std::string lbl_exit = label_at(exit_target);

    ValuePair index = pop();
    ValuePair keys = pop();
    ValuePair obj = peek(); // keep obj on stack

    // Check if index >= keys.length — if so, jump to exit
    ValuePair next_key = {T(), T()};
    ir_ << "    call $rt_forin_next(l " << keys.data
        << ", l " << index.data << ")\n";
    // Result is either the next key or nil if done
    emit_global_load("rt_ret", next_key.tag, next_key.data);

    // Check if done (nil)
    std::string is_done = T();
    std::string tag_check = T();
    ir_ << "    " << tag_check << " =w ceqw " << next_key.tag << ", " << TAG_NIL << "\n";
    if (!lbl_exit.empty()) {
        ir_ << "    jnz " << tag_check << ", " << lbl_exit << ", @L" << label_id_ << "\n";
        ir_ << "@L" << label_id_ << "\n";
    }

    // Increment index via runtime
    ValuePair new_index = {T(), T()};
    ir_ << "    " << new_index.tag << " =w copy " << TAG_NUM << "\n";
    ir_ << "    " << new_index.data << " =l call $rt_inc(l " << index.data << ")\n";
    push(new_index);
    push(next_key);
}

void QbeCodegen::emit_optional_chain() {
    // If top of stack is nil, push nil; otherwise continue
    // For QBE AOT, this is handled at a higher level
    emit_nil();
}

void QbeCodegen::emit_spread(uint8_t count) {
    // Spread array onto stack — handled by runtime
    for (int i = 0; i < count; i++) {
        ValuePair v = pop(); // array to spread
        ir_ << "    call $rt_spread(l " << v.data << ")\n";
    }
}

void QbeCodegen::emit_load_local(int slot) {
    emit_get_local(static_cast<uint8_t>(slot));
}

void QbeCodegen::emit_const_int8(uint8_t val) {
    ValuePair v = {T(), T()};
    std::string dtmp = T();
    ir_ << "    " << v.tag << " =w copy " << TAG_NUM << "\n";
    ir_ << "    " << dtmp << " =d sltof " << static_cast<int>(val) << "\n";
    ir_ << "    " << v.data << " =l cast " << dtmp << "\n";
    push(v);
}

void QbeCodegen::emit_inc_local(uint8_t slot, bool is_trusted) {
    if (slot >= locals_.size()) return;
    std::string check_num = T();
    std::string label_num = L();
    std::string label_done = L();

    ir_ << "    " << check_num << " =w ceqw " << locals_[slot].tag << ", " << TAG_NUM << "\n";
    ir_ << "    jnz " << check_num << ", " << label_num << ", " << label_done << "\n";

    ir_ << label_num << "\n";
    std::string dval = T();
    std::string dinc = T();
    ir_ << "    " << dval << " =d cast " << locals_[slot].data << "\n";
    ir_ << "    " << dinc << " =d add " << dval << ", d_1.0\n";
    ir_ << "    " << locals_[slot].data << " =l cast " << dinc << "\n";
    ir_ << "    jmp " << label_done << "\n";
    ir_ << label_done << "\n";
}

void QbeCodegen::emit_dec_local(uint8_t slot, bool is_trusted) {
    if (slot >= locals_.size()) return;
    std::string check_num = T();
    std::string label_num = L();
    std::string label_done = L();

    ir_ << "    " << check_num << " =w ceqw " << locals_[slot].tag << ", " << TAG_NUM << "\n";
    ir_ << "    jnz " << check_num << ", " << label_num << ", " << label_done << "\n";

    ir_ << label_num << "\n";
    std::string dval = T();
    std::string ddec = T();
    ir_ << "    " << dval << " =d cast " << locals_[slot].data << "\n";
    ir_ << "    " << ddec << " =d sub " << dval << ", d_1.0\n";
    ir_ << "    " << locals_[slot].data << " =l cast " << ddec << "\n";
    ir_ << "    jmp " << label_done << "\n";
    ir_ << label_done << "\n";
}

void QbeCodegen::emit_add_local_const(uint8_t slot, uint8_t const_idx) {
    if (slot >= locals_.size()) return;
    ValuePair result = {T(), T()};

    std::string check_num = T();
    std::string label_num = L();
    std::string label_done = L();
    std::string label_fallback = L();

    ir_ << "    " << check_num << " =w ceqw " << locals_[slot].tag << ", " << TAG_NUM << "\n";
    ir_ << "    jnz " << check_num << ", " << label_num << ", " << label_fallback << "\n";

    ir_ << label_num << "\n";
    std::string dlocal = T();
    std::string dconst = T();
    std::string dsum = T();
    ir_ << "    " << dlocal << " =d cast " << locals_[slot].data << "\n";
    if (const_idx < chunk_->constants.size() &&
        chunk_->constants[const_idx].type == ValueType::NUMBER) {
        double cval = chunk_->constants[const_idx].as.number;
        ir_ << "    " << dconst << " =d copy d_" << cval << "\n";
    } else {
        ir_ << "    " << dconst << " =d copy d_0.0\n";
    }
}

void QbeCodegen::emit_inc_global(uint8_t name_idx) {
    if (name_idx >= chunk_->constants.size() ||
        chunk_->constants[name_idx].type != ValueType::OBJ_STRING) return;
    std::string name = chunk_->constants[name_idx].as.obj_string->chars;
    std::string sym = global_sym(name);

    std::string check_num = T();
    std::string label_num = L();
    std::string label_done = L();

    std::string gb = global_base(sym);
    ir_ << "    " << check_num << " =w loadw " << gb << "\n";
    std::string gb_off = T();
    ir_ << "    " << gb_off << " =l add " << gb << ", 8\n";
    std::string tag_check = T();
    ir_ << "    " << tag_check << " =w ceqw " << check_num << ", " << TAG_NUM << "\n";
    ir_ << "    jnz " << tag_check << ", " << label_num << ", " << label_done << "\n";

    ir_ << label_num << "\n";
    std::string dval = T();
    std::string dinc = T();
    std::string data = T();
    ir_ << "    " << data << " =l loadl " << gb_off << "\n";
    ir_ << "    " << dval << " =d cast " << data << "\n";
    ir_ << "    " << dinc << " =d add " << dval << ", d_1.0\n";
    std::string dinc_int = T();
    ir_ << "    " << dinc_int << " =l cast " << dinc << "\n";
    ir_ << "    storel " << dinc_int << ", " << gb_off << "\n";
    ir_ << "    jmp " << label_done << "\n";
    ir_ << label_done << "\n";
}

void QbeCodegen::emit_fused_compare_jump(OpCode op, uint16_t offset) {
    // Fused comparison + conditional jump
    // Stack: ..., a, b -> pop both, jump if condition is FALSE
    ValuePair b = pop();
    ValuePair a = pop();
    size_t target = ip_ + offset;
    std::string lbl_taken = label_at(target);
    if (lbl_taken.empty()) return;

    std::string check_a = T();
    std::string check_b = T();
    std::string both_num = T();
    std::string label_num = L();
    std::string label_done = L();

    ir_ << "    " << check_a << " =w ceqw " << a.tag << ", " << TAG_NUM << "\n";
    ir_ << "    " << check_b << " =w ceqw " << b.tag << ", " << TAG_NUM << "\n";
    ir_ << "    " << both_num << " =w and " << check_a << ", " << check_b << "\n";
    ir_ << "    jnz " << both_num << ", " << label_num << ", " << label_done << "\n";

    ir_ << label_num << "\n";
    std::string da = T();
    std::string db = T();
    std::string cmp = T();
    ir_ << "    " << da << " =d cast " << a.data << "\n";
    ir_ << "    " << db << " =d cast " << b.data << "\n";

    switch (op) {
        case OpCode::OP_LESS_JUMP:
            ir_ << "    " << cmp << " =w cltd " << da << ", " << db << "\n";
            break;
        case OpCode::OP_GREATER_JUMP:
            ir_ << "    " << cmp << " =w cgtd " << da << ", " << db << "\n";
            break;
        case OpCode::OP_EQUAL_JUMP:
            ir_ << "    " << cmp << " =w ceqd " << da << ", " << db << "\n";
            break;
        default:
            ir_ << "    " << cmp << " =w ceqd " << da << ", " << db << "\n";
            break;
    }

    // If condition is false (0), jump to target
    ir_ << "    jnz " << cmp << ", " << label_done << ", " << lbl_taken << "\n";
    ir_ << "    jmp " << label_done << "\n";
    ir_ << label_done << "\n";
}

// ============== Jump target finding ==============

void QbeCodegen::find_jump_targets() {
    jump_targets_.clear();
    size_t ip = 0;
    while (ip < chunk_->code.size()) {
        uint8_t instr = chunk_->code[ip++];
        OpCode op = static_cast<OpCode>(instr);

        switch (op) {
            case OpCode::OP_JUMP:
            case OpCode::OP_JUMP_IF_FALSE:
            case OpCode::OP_LOOP:
            case OpCode::OP_LOGICAL_AND:
            case OpCode::OP_LOGICAL_OR:
            case OpCode::OP_LESS_JUMP:
            case OpCode::OP_GREATER_JUMP:
            case OpCode::OP_EQUAL_JUMP:
            case OpCode::OP_FOR_IN_INIT:
            case OpCode::OP_FOR_IN_NEXT: {
                if (ip + 1 >= chunk_->code.size()) break;
                uint16_t offset = (chunk_->code[ip] << 8) | chunk_->code[ip + 1];
                ip += 2;
                size_t target;
                if (op == OpCode::OP_LOOP) {
                    target = (ip >= offset) ? (ip - offset) : 0;
                } else {
                    target = ip + offset;
                }
                if (target < chunk_->code.size() && target != ip) {
                    if (jump_targets_.find(target) == jump_targets_.end())
                        jump_targets_[target] = L();
                }
                break;
            }
            case OpCode::OP_LOOP_IF_LESS_LOCAL: {
                ip++; // slot
                ip++; // const index
                if (ip + 1 >= chunk_->code.size()) break;
                uint16_t offset = (chunk_->code[ip] << 8) | chunk_->code[ip + 1];
                ip += 2;
                // The exit target is a forward jump
                size_t exit_target = ip + offset;
                if (exit_target <= chunk_->code.size() && exit_target != ip) {
                    if (jump_targets_.find(exit_target) == jump_targets_.end())
                        jump_targets_[exit_target] = L();
                }
                // Also mark the loop start (backwards) as it's a jump target
                if (ip >= offset) {
                    size_t loop_start = ip - offset;
                    if (jump_targets_.find(loop_start) == jump_targets_.end())
                        jump_targets_[loop_start] = L();
                }
                break;
            }
            default:
                // Skip operands — same pattern as analyze_bytecode
                if (op == OpCode::OP_CONSTANT || op == OpCode::OP_GET_LOCAL ||
                    op == OpCode::OP_SET_LOCAL || op == OpCode::OP_GET_UPVALUE ||
                    op == OpCode::OP_SET_UPVALUE || op == OpCode::OP_CLOSE_UPVALUE ||
                    op == OpCode::OP_GET_PROPERTY || op == OpCode::OP_SET_PROPERTY ||
                    op == OpCode::OP_GET_GLOBAL_FAST || op == OpCode::OP_SET_GLOBAL_FAST ||
                    op == OpCode::OP_GET_GLOBAL || op == OpCode::OP_SET_GLOBAL ||
                    op == OpCode::OP_DEFINE_GLOBAL || op == OpCode::OP_DEFINE_TYPED_GLOBAL ||
                    op == OpCode::OP_SET_GLOBAL_TYPED || op == OpCode::OP_SET_LOCAL_TYPED ||
                    op == OpCode::OP_INCREMENT_LOCAL || op == OpCode::OP_DECREMENT_LOCAL ||
                    op == OpCode::OP_INC_LOCAL_INT || op == OpCode::OP_DEC_LOCAL_INT ||
                    op == OpCode::OP_CONST_INT8 || op == OpCode::OP_CALL ||
                    op == OpCode::OP_CALL_FAST || op == OpCode::OP_ARRAY ||
                    op == OpCode::OP_INCREMENT_GLOBAL ||
                    op == OpCode::OP_SPREAD || op == OpCode::OP_THIS) {
                    ip++;
                } else if (op == OpCode::OP_INVOKE) {
                    ip += 2; // name_index + arg_count
                } else if (op == OpCode::OP_CONSTANT_LONG || op == OpCode::OP_ADD_LOCAL_CONST) {
                    ip += 2;
                } else if (op == OpCode::OP_CLOSURE) {
                    ip++;
                    if (ip + 1 < chunk_->code.size()) {
                        uint16_t n_up = (chunk_->code[ip] << 8) | chunk_->code[ip + 1];
                        ip += 2 + n_up * 2;
                    }
                } else if (op == OpCode::OP_LOOP_HINT || op == OpCode::OP_TYPE_GUARD) {
                    ip += 2;
                }
                break;
        }
    }
    // Mark entry point
    if (jump_targets_.find(0) == jump_targets_.end())
        jump_targets_[0] = "@start";
}

// ============== Main emit function ==============

std::string QbeCodegen::emit_function(const std::string& name, int param_count, const std::vector<int>* outer_global_func_idx, const std::vector<GlobalVar>* outer_globals) {
    ir_.str("");
    ir_.clear();
    temp_id_ = 100; // Start at 100 to avoid any potential low-ID conflicts
    label_id_ = 1;
    ip_ = 0;
    stack_.clear();
    stack_func_idx_.clear();
    locals_.clear();
    param_count_ = param_count;

    // First pass: analyze bytecode to collect globals, locals, constants
    analyze_bytecode();

    // First pass: find jump targets
    find_jump_targets();

    if (!is_main_ && outer_global_func_idx && outer_globals) {
        set_global_func_idx(*outer_global_func_idx, *outer_globals);
    }



    // Recursively compile inner functions (CALLABLE constants) first
    std::string inner_functions;
    if (is_main_) {
        for (size_t i = 0; i < chunk_->constants.size(); i++) {
            const Value& cv = chunk_->constants[i];
            if (cv.type == ValueType::CALLABLE) {
                Callable* callable = cv.as.callable;
                if (callable) {
                    Function* func = dynamic_cast<Function*>(callable);
                    if (func && func->chunk) {
                        std::string fprefix = "qbe_func_" + std::to_string(i) + "_";
                        QbeCodegen inner(func->chunk, false, fprefix);
                        inner_functions += inner.emit_function("qbe_func_" + std::to_string(i), func->arity_val, &global_func_idx_, &globals_);
                        // Propagate inner function's globals back to outer (e.g. module imports like "g_math")
                        for (const auto& g : inner.globals()) {
                            bool found = false;
                            for (const auto& existing : globals_) {
                                if (existing.name == g.name) { found = true; break; }
                            }
                            if (!found) globals_.push_back(g);
                        }
                        inner_functions += "\n";
                    }
                }
            }
        }
    }

    // Emit data section (constants and globals)
    emit_data_section();

    // Emit class method metadata for runtime reconstruction
    emit_class_methods();

    // Emit inner functions' IR before this function
    ir_ << inner_functions;

    // Emit function (with params if this is an inner function)
    emit_function_start(name, param_count);

    // Initialize class definitions at runtime start
    if (is_main_ && has_class_constants()) {
        ir_ << "    call $rt_init_classes(l $rt_class_data)\n";
    }

    // Emit locals init AFTER any init calls

    // Initialize local variables
    emit_locals_init();

    // Map parameters to local slots (for inner functions)
    for (int i = 0; i < param_count && i < num_locals_; i++) {
        ir_ << "    " << locals_[i].tag << " =w copy %p" << i << "_t\n";
        ir_ << "    " << locals_[i].data << " =l copy %p" << i << "_d\n";
    }

    // Emit all instructions
    last_was_term_ = false;
    while (ip_ < chunk_->code.size()) {
        size_t offset = ip_;
        if (debug_) {
            ir_ << "    # offset " << offset << "\n";
        }

        // Emit label if this is a jump target
        std::string lbl = label_at(offset);
        if (!lbl.empty() && lbl != "@start") {
            ir_ << lbl << "\n";
            last_was_term_ = false;  // Label resets terminator state
        }

        // Skip dead code after terminators (return/jump) until next label
        if (last_was_term_) {
            // Determine instruction size and skip it
            uint8_t instr = chunk_->code[ip_];
            OpCode dead_op = static_cast<OpCode>(instr);
            ip_++;
            // Skip operands
            switch (dead_op) {
                case OpCode::OP_CONSTANT: case OpCode::OP_GET_LOCAL:
                case OpCode::OP_SET_LOCAL: case OpCode::OP_GET_UPVALUE:
                case OpCode::OP_SET_UPVALUE: case OpCode::OP_CLOSE_UPVALUE:
                case OpCode::OP_GET_PROPERTY: case OpCode::OP_SET_PROPERTY:
                case OpCode::OP_GET_GLOBAL_FAST: case OpCode::OP_SET_GLOBAL_FAST:
                case OpCode::OP_INCREMENT_LOCAL: case OpCode::OP_DECREMENT_LOCAL:
                case OpCode::OP_INC_LOCAL_INT: case OpCode::OP_DEC_LOCAL_INT:
                case OpCode::OP_CONST_INT8: case OpCode::OP_CALL:
                case OpCode::OP_CALL_FAST: case OpCode::OP_ARRAY:
                case OpCode::OP_INVOKE: case OpCode::OP_GET_GLOBAL:
                case OpCode::OP_SET_GLOBAL: case OpCode::OP_DEFINE_GLOBAL:
                case OpCode::OP_INCREMENT_GLOBAL: case OpCode::OP_SPREAD:
                case OpCode::OP_CONSTANT_LONG: case OpCode::OP_ADD_LOCAL_CONST:
                case OpCode::OP_LOOP_IF_LESS_LOCAL: case OpCode::OP_CLOSURE:
                    // skip operands — will be handled by advancing ip_ correctly below
                    break;
                default: break;
            }
            continue;
        }

        uint8_t instr = rb();
        OpCode op = static_cast<OpCode>(instr);

        if (debug_) {
            ir_ << "    # OP_" << static_cast<int>(op) << "\n";
        }

        switch (op) {
            case OpCode::OP_RETURN:
                emit_return();
                break;
            case OpCode::OP_CONSTANT:
                emit_const(rb());
                break;
            case OpCode::OP_CONSTANT_LONG:
                emit_const(rs());
                break;
            case OpCode::OP_NIL:
                emit_nil();
                break;
            case OpCode::OP_TRUE:
                emit_bool(true);
                break;
            case OpCode::OP_FALSE:
                emit_bool(false);
                break;
            case OpCode::OP_POP:
                emit_pop();
                break;
            case OpCode::OP_DUP:
                emit_dup();
                break;
            case OpCode::OP_GET_LOCAL:
                emit_get_local(rb());
                break;
            case OpCode::OP_SET_LOCAL:
                emit_set_local(rb());
                break;
            case OpCode::OP_GET_GLOBAL:
                emit_get_global(rb());
                break;
            case OpCode::OP_SET_GLOBAL:
                emit_set_global(rb());
                break;
            case OpCode::OP_DEFINE_GLOBAL:
                emit_define_global(rb());
                break;
            case OpCode::OP_DEFINE_TYPED_GLOBAL:
                emit_define_global(rb());
                break;
            case OpCode::OP_SET_GLOBAL_TYPED:
                emit_set_global(rb());
                break;
            case OpCode::OP_SET_LOCAL_TYPED:
                emit_set_local(rb());
                break;
            case OpCode::OP_GET_GLOBAL_FAST:
                emit_get_global_fast(rb());
                break;
            case OpCode::OP_SET_GLOBAL_FAST:
                emit_set_global_fast(rb());
                break;
            case OpCode::OP_ADD:
            case OpCode::OP_ADD_INT:
            case OpCode::OP_SUBTRACT:
            case OpCode::OP_SUB_INT:
            case OpCode::OP_MULTIPLY:
            case OpCode::OP_MUL_INT:
            case OpCode::OP_DIVIDE:
            case OpCode::OP_DIV_INT:
            case OpCode::OP_MODULO:
            case OpCode::OP_MOD_INT:
                emit_arithmetic(op);
                break;
            case OpCode::OP_EQUAL:
            case OpCode::OP_EQUAL_INT:
            case OpCode::OP_NOT_EQUAL:
            case OpCode::OP_LESS:
            case OpCode::OP_LESS_INT:
            case OpCode::OP_GREATER:
            case OpCode::OP_GREATER_INT:
                emit_compare(op);
                break;
            case OpCode::OP_LESS_JUMP:
            case OpCode::OP_GREATER_JUMP:
            case OpCode::OP_EQUAL_JUMP:
                emit_fused_compare_jump(op, rs());
                break;
            case OpCode::OP_NOT:
                emit_not();
                break;
            case OpCode::OP_NEGATE:
            case OpCode::OP_NEGATE_INT:
                emit_negate();
                break;
            case OpCode::OP_BITWISE_AND:
            case OpCode::OP_BITWISE_OR:
            case OpCode::OP_BITWISE_XOR:
            case OpCode::OP_LEFT_SHIFT:
            case OpCode::OP_RIGHT_SHIFT:
                emit_bitwise(op);
                break;
            case OpCode::OP_BITWISE_NOT: {
                // Unary bitwise not
                ValuePair v = pop();
                // ~x = x XOR -1 — use runtime
                ValuePair result = {T(), T()};
                ir_ << "    call $rt_bnot(w " << v.tag << ", l " << v.data << ")\n";
                emit_global_load("rt_ret", result.tag, result.data);
                push(result);
                break;
            }
            case OpCode::OP_SAY:
                emit_say();
                break;
            case OpCode::OP_JUMP:
                emit_jump(rs());
                break;
            case OpCode::OP_JUMP_IF_FALSE:
                emit_jump_if_false(rs());
                break;
            case OpCode::OP_LOOP:
                emit_loop(rs());
                break;
            case OpCode::OP_LOOP_IF_LESS_LOCAL: {
                uint8_t slot = rb();
                uint8_t const_idx = rb();
                uint16_t offset = rs();
                emit_loop_if_less_local(slot, const_idx, offset);
                break;
            }
            case OpCode::OP_LOGICAL_AND:
                emit_logical_and(rs());
                break;
            case OpCode::OP_LOGICAL_OR:
                emit_logical_or(rs());
                break;
            case OpCode::OP_CALL:
            case OpCode::OP_CALL_FAST:
                emit_call(rb());
                break;
            case OpCode::OP_CLOSURE:
                emit_closure(rb());
                break;
            case OpCode::OP_GET_UPVALUE:
                emit_get_upvalue(rb());
                break;
            case OpCode::OP_SET_UPVALUE:
                emit_set_upvalue(rb());
                break;
            case OpCode::OP_CLOSE_UPVALUE:
                emit_close_upvalue(rb());
                break;
            case OpCode::OP_ARRAY:
                emit_array(rb());
                break;
            case OpCode::OP_OBJECT:
                emit_object(rb());
                break;
            case OpCode::OP_INDEX_GET:
                emit_index_get();
                break;
            case OpCode::OP_INDEX_SET:
                emit_index_set();
                break;
            case OpCode::OP_GET_PROPERTY:
                emit_get_property(rb());
                break;
            case OpCode::OP_SET_PROPERTY:
                emit_set_property(rb());
                break;
            case OpCode::OP_THIS:
                emit_this();
                break;
            case OpCode::OP_INVOKE: {
                uint8_t name_idx = rb();
                uint8_t arg_count = rb();
                emit_invoke(name_idx, arg_count);
                break;
            }
            case OpCode::OP_TRY:
                emit_try();
                break;
            case OpCode::OP_END_TRY:
                emit_end_try();
                break;
            case OpCode::OP_THROW:
                emit_throw();
                break;
            case OpCode::OP_BREAK:
                emit_break();
                break;
            case OpCode::OP_CONTINUE:
                emit_continue();
                break;
            case OpCode::OP_FOR_IN_INIT:
                emit_for_in_init(rs());
                break;
            case OpCode::OP_FOR_IN_NEXT:
                emit_for_in_next(rs());
                break;
            case OpCode::OP_OPTIONAL_CHAIN:
                emit_optional_chain();
                break;
            case OpCode::OP_SPREAD:
                emit_spread(rb());
                break;
            case OpCode::OP_CONST_ZERO:
                emit_const_int8(0);
                break;
            case OpCode::OP_CONST_ONE:
                emit_const_int8(1);
                break;
            case OpCode::OP_CONST_INT8:
                emit_const_int8(rb());
                break;
            case OpCode::OP_INC_LOCAL_INT:
            case OpCode::OP_INCREMENT_LOCAL:
                emit_inc_local(rb());
                break;
            case OpCode::OP_DEC_LOCAL_INT:
            case OpCode::OP_DECREMENT_LOCAL:
                emit_dec_local(rb());
                break;
            case OpCode::OP_ADD_LOCAL_CONST:
                emit_add_local_const(rb(), rb());
                break;
            case OpCode::OP_INCREMENT_GLOBAL:
                emit_inc_global(rb());
                break;
            case OpCode::OP_LOAD_LOCAL_0:
            case OpCode::OP_LOAD_LOCAL_1:
            case OpCode::OP_LOAD_LOCAL_2:
            case OpCode::OP_LOAD_LOCAL_3:
                emit_load_local(static_cast<int>(op) - static_cast<int>(OpCode::OP_LOAD_LOCAL_0));
                break;
            case OpCode::OP_TYPE_GUARD:
            case OpCode::OP_LOOP_HINT:
            case OpCode::OP_TAIL_CALL:
                // No-op in AOT
                break;
            case OpCode::OP_VALIDATE_SAFE_FUNCTION:
            case OpCode::OP_VALIDATE_SAFE_VARIABLE:
            case OpCode::OP_VALIDATE_SAFE_FILE_FUNCTION:
            case OpCode::OP_VALIDATE_SAFE_FILE_VARIABLE:
                // No-op in AOT (compile-time validated)
                break;
            case OpCode::OP_COUNT:
                break;
            default:
                if (debug_) {
                    ir_ << "    # UNKNOWN OP " << static_cast<int>(op) << "\n";
                }
                break;
        }
    }

    emit_function_end();
    return ir_.str();
}


void QbeCodegen::set_global_func_idx(const std::vector<int>& outer_global_func_idx,
                                     const std::vector<GlobalVar>& outer_globals) {
    for (size_t slot = 0; slot < globals_.size(); slot++) {
        const std::string& name = globals_[slot].name;
        for (size_t o = 0; o < outer_globals.size(); o++) {
            if (outer_globals[o].name == name && o < outer_global_func_idx.size()) {
                int idx = outer_global_func_idx[o];
                if (idx >= 0) {
                    if (slot >= global_func_idx_.size())
                        global_func_idx_.resize(globals_.size(), -1);
                    global_func_idx_[slot] = idx;
                }
                break;
            }
        }
    }
}

} // namespace aot
} // namespace neutron
