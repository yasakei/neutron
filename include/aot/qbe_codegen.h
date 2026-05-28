#ifndef NEUTRON_AOT_QBE_CODEGEN_H
#define NEUTRON_AOT_QBE_CODEGEN_H

#include "compiler/bytecode.h"
#include "types/value.h"
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace neutron {
namespace aot {

// Each Neutron value is represented as a pair of QBE temporaries:
//   tag:   w   (0=nil, 1=bool, 2=number, 3=string, 4=array, 5=object, 6=class, 7=instance, 8=callable)
//   data:  l   (for number: bits of double; for others: pointer)
struct ValuePair {
    std::string tag;
    std::string data;
};

class QbeCodegen {
public:
    QbeCodegen(const Chunk* chunk, bool is_main = true, const std::string& const_prefix = "");

    struct GlobalVar {
        std::string name;
        std::string symbol;
    };

    std::string emit_function(const std::string& name, int param_count = 0,
                              const std::vector<int>* outer_global_func_idx = nullptr,
                              const std::vector<GlobalVar>* outer_globals = nullptr);
    void set_debug(bool debug) { debug_ = true; }
    const std::vector<GlobalVar>& globals() const { return globals_; }
    void set_global_func_idx(const std::vector<int>& outer_global_func_idx,
                             const std::vector<GlobalVar>& outer_globals);

private:
    const Chunk* chunk_;
    std::ostringstream ir_;
    size_t ip_;
    bool is_main_;
    std::string const_prefix_;

    bool debug_ = false;
    bool last_was_term_ = false;  // Last emitted instruction was a terminator (jmp/ret)
    std::string end_label_;  // Function epilogue label for return jumps

    // Temp / label allocators
    int temp_id_ = 0;
    int label_id_ = 0;
    std::string T();
    std::string L();

    // Value stack — each entry is a pair of (tag_temp, data_temp)
    // stack_func_idx_ parallels stack_: -1 = not a function, >= 0 = function constant index
    std::vector<ValuePair> stack_;
    std::vector<int> stack_func_idx_;
    void push(const ValuePair& v, int func_idx = -1);
    ValuePair pop();
    ValuePair peek(int depth = 0) const;
    int peek_func_idx(int depth = 0) const;
    ValuePair pop_two();  // returns first operand (deeper one)

    // Local variable slots — pre-allocated temps
    std::vector<ValuePair> locals_;
    int num_locals_ = 0;

    // Global variables — QBE data symbols
    std::vector<GlobalVar> globals_;
    // Tracks function constant index for each global slot (-1 = not a function)
    std::vector<int> global_func_idx_;
    std::string global_sym(const std::string& name);

    // Constants pool — QBE data symbols
    std::vector<std::string> const_syms_;
    std::string const_sym(size_t index);

    // Jump target resolution
    std::unordered_map<size_t, std::string> jump_targets_;
    void find_jump_targets();
    std::string label_at(size_t ip);

    // First pass: collect globals and count locals
    void analyze_bytecode();

    // Prologue / epilogue
    void emit_data_section();
    void emit_class_methods();
    bool has_class_constants() const;
    void emit_function_start(const std::string& name, int param_count = 0);
    void emit_function_end();
    void emit_locals_init();

    // Helpers
    uint8_t rb();
    uint16_t rs();

    // Emit a constant value from the pool as (tag, data)
    ValuePair emit_constant(size_t index);

    // Get a temp containing the base address of a global data symbol
    std::string global_base(const std::string& sym);

    // Load/store a pair (tag, data) from/to a global data symbol
    void emit_global_load(const std::string& sym, const std::string& tag, const std::string& data);
    void emit_global_store(const std::string& sym, const std::string& tag, const std::string& data);

    // Tag constants
    std::string TAG_NIL;
    std::string TAG_BOOL;
    std::string TAG_NUM;
    std::string TAG_STR;
    std::string TAG_ARRAY;
    std::string TAG_OBJ;
    std::string TAG_CLASS;
    std::string TAG_INST;
    std::string TAG_CALLABLE;

    // Emit instructions — one per opcode
    void emit_return();
    void emit_const(size_t index);
    void emit_nil();
    void emit_bool(bool val);
    void emit_pop();
    void emit_dup();
    void emit_get_local(uint8_t slot);
    void emit_set_local(uint8_t slot);
    void emit_get_global(uint8_t name_idx);
    void emit_set_global(uint8_t name_idx);
    void emit_define_global(uint8_t name_idx);
    void emit_get_global_fast(uint8_t slot);
    void emit_set_global_fast(uint8_t slot);
    void emit_arithmetic(OpCode op);
    void emit_compare(OpCode op);
    void emit_not();
    void emit_negate(bool is_trusted = false);
    void emit_bitwise(OpCode op);
    void emit_bitwise_not(bool is_trusted = false);
    void emit_say();
    void emit_jump(uint16_t offset);
    void emit_jump_if_false(uint16_t offset);
    void emit_loop(uint16_t offset);
    void emit_loop_if_less_local(uint8_t slot, uint8_t const_idx, uint16_t offset);
    void emit_logical_and(uint16_t offset);
    void emit_logical_or(uint16_t offset);
    void emit_call(uint8_t arg_count);
    void emit_closure(uint8_t func_idx);
    void emit_get_upvalue(uint8_t slot);
    void emit_set_upvalue(uint8_t slot);
    void emit_close_upvalue(uint8_t slot);
    void emit_array(uint8_t size);
    void emit_object(uint8_t prop_count);
    void emit_index_get();
    void emit_index_set();
    void emit_get_property(uint8_t name_idx);
    void emit_set_property(uint8_t name_idx);
    void emit_this();
    void emit_invoke(uint8_t name_idx, uint8_t arg_count);
    void emit_try();
    void emit_end_try();
    void emit_throw();
    void emit_break();
    void emit_continue();
    void emit_for_in_init(uint16_t offset);
    void emit_for_in_next(uint16_t offset);
    void emit_optional_chain();
    void emit_spread(uint8_t count);
    void emit_load_local(int slot);
    void emit_const_int8(uint8_t val);
    void emit_inc_local(uint8_t slot, bool is_trusted = false);
    void emit_dec_local(uint8_t slot, bool is_trusted = false);
    void emit_add_local_const(uint8_t slot, uint8_t const_idx);
    void emit_inc_global(uint8_t name_idx);
    void emit_fused_compare_jump(OpCode op, uint16_t offset);
};

} // namespace aot
} // namespace neutron

#endif // NEUTRON_AOT_QBE_CODEGEN_H
