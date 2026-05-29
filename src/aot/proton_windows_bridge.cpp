#include "aot/proton_windows_bridge.h"
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iterator>

namespace neutron {
namespace aot {

// ---------------------------------------------------------------------------
// GAS (AT&T) → MASM (Intel) syntax converter for QBE/Proton output
// Translates the subset of GAS that the QBE amd64 backend emits so that
// ml64.exe (MASM) can assemble it on Windows.
// ---------------------------------------------------------------------------

static bool is_register(const std::string& tok) {
    if (tok.empty()) return false;
    if (tok[0] != '%') return false;
    std::string reg = tok.substr(1);
    // x86-64 / SSE registers
    return reg == "rax" || reg == "rbx" || reg == "rcx" || reg == "rdx" ||
           reg == "rsi" || reg == "rdi" || reg == "rbp" || reg == "rsp" ||
           reg == "r8"  || reg == "r9"  || reg == "r10" || reg == "r11" ||
           reg == "r12" || reg == "r13" || reg == "r14" || reg == "r15" ||
           reg == "eax" || reg == "ebx" || reg == "ecx" || reg == "edx" ||
           reg == "esi" || reg == "edi" || reg == "ebp" || reg == "esp" ||
           reg == "eip" || reg == "rip" ||
           (reg.size() > 3 && reg.substr(0, 3) == "xmm") ||
           (reg.size() > 3 && reg.substr(0, 3) == "ymm");
}

static std::string strip_register(const std::string& tok) {
    if (!tok.empty() && tok[0] == '%')
        return tok.substr(1);
    return tok;
}

static std::string strip_immediate(const std::string& tok) {
    if (!tok.empty() && tok[0] == '$')
        return tok.substr(1);
    return tok;
}

// Convert an AT&T memory operand like "-8(%rbp)" or "(%rbx,%rcx,4)" to Intel "[base+index*scale+disp]"
static std::string convert_memory_operand(const std::string& gas_mem) {
    // Pattern: [displacement]([base][, [index][, scale]])
    size_t paren_open = gas_mem.find('(');
    if (paren_open == std::string::npos)
        return gas_mem;

    std::string disp = gas_mem.substr(0, paren_open);
    std::string inner = gas_mem.substr(paren_open + 1, gas_mem.size() - paren_open - 2);

    // Parse inner: base, index, scale
    std::string base, index, scale;
    size_t c1 = inner.find(',');
    if (c1 == std::string::npos) {
        base = inner;
    } else {
        base = inner.substr(0, c1);
        size_t c2 = inner.find(',', c1 + 1);
        if (c2 == std::string::npos) {
            index = inner.substr(c1 + 1);
        } else {
            index = inner.substr(c1 + 1, c2 - c1 - 1);
            scale = inner.substr(c2 + 1);
        }
    }

    // Strip % prefix from registers
    if (!base.empty()) base = strip_register(base);
    if (!index.empty()) index = strip_register(index);

    // If this is a RIP-relative addressing like "symbol(%rip)", just use symbol
    if (base == "rip") {
        return disp;
    }

    // Build Intel syntax: [base+index*scale+disp]
    std::string result = "[";
    bool first = true;

    if (!base.empty()) {
        result += base;
        first = false;
    }
    if (!index.empty()) {
        if (!first) result += "+";
        result += index;
        if (!scale.empty() && scale != "1") {
            result += "*" + scale;
        }
        first = false;
    }
    if (!disp.empty() && disp != "0") {
        if (!first && disp[0] != '-') result += "+";
        result += disp;
        first = false;
    }
    if (first) result += "0";
    result += "]";

    return result;
}

// Parse a single GAS operand and convert to MASM
static std::string convert_operand(const std::string& gas_op) {
    std::string trimmed = gas_op;
    // Trim whitespace
    size_t start = trimmed.find_first_not_of(" \t");
    if (start != std::string::npos) trimmed = trimmed.substr(start);
    size_t end = trimmed.find_last_not_of(" \t");
    if (end != std::string::npos) trimmed = trimmed.substr(0, end + 1);

    if (trimmed.empty()) return trimmed;

    if (trimmed[0] == '%') {
        // Register
        return strip_register(trimmed);
    } else if (trimmed[0] == '$') {
        // Immediate
        return strip_immediate(trimmed);
    } else if (trimmed.find('(') != std::string::npos) {
        // Memory operand
        return convert_memory_operand(trimmed);
    }
    // Plain symbol or number
    return trimmed;
}

// Map size suffix → MASM PTR keyword
static const char* size_suffix_to_ptr(const std::string& instr) {
    if (instr.size() < 2) return nullptr;
    char last = instr.back();
    switch (last) {
        case 'b': return "BYTE PTR ";
        case 'w': return "WORD PTR ";
        case 'l': return "DWORD PTR ";
        case 'q': return "QWORD PTR ";
        default:  return nullptr;
    }
}

// Strip size suffix from instruction mnemonic
static std::string strip_suffix(const std::string& instr) {
    if (instr.empty()) return instr;
    char last = instr.back();
    if (last == 'q' || last == 'l' || last == 'w' || last == 'b') {
        // Only strip if preceded by a non-vowel (to avoid stripping "call", "ret", etc.)
        if (instr.size() >= 2) {
            char prev = instr[instr.size() - 2];
            if (prev != 'a' && prev != 'e' && prev != 'i' && prev != 'o' && prev != 'u') {
                // But don't strip "movsb", "movsw", "movslq" - keep as "movsx" equivalents
                if (instr == "movsb" || instr == "movsw" || instr == "movslq" ||
                    instr == "movzb" || instr == "movzw" || instr == "movzl" ||
                    instr == "setb" || instr == "setl" || instr == "setg" ||
                    instr == "seta" || instr == "sete" || instr == "setne" ||
                    instr == "setz" || instr == "setnz" || instr == "setle" ||
                    instr == "setge" || instr == "setbe" || instr == "setae" ||
                    instr == "setp" || instr == "setnp") {
                    return instr;
                }
                return instr.substr(0, instr.size() - 1);
            }
        }
    }
    return instr;
}

// Check if an operand looks like a memory reference (contains '[')
static bool is_memory_ref(const std::string& operand) {
    return operand.find('[') != std::string::npos;
}

// Convert a single GAS assembly line to MASM
static std::string convert_gas_line(const std::string& line) {
    std::string trimmed = line;
    size_t start = trimmed.find_first_not_of(" \t");
    if (start == std::string::npos) return line; // empty line
    trimmed = trimmed.substr(start);

    if (trimmed.empty()) return line;

    // Strip GAS comments (# ...)
    size_t comment_pos = std::string::npos;
    bool in_string = false;
    for (size_t i = 0; i < trimmed.size(); i++) {
        if (trimmed[i] == '"') in_string = !in_string;
        if (trimmed[i] == '#' && !in_string) {
            comment_pos = i;
            break;
        }
    }

    std::string before_comment = trimmed;
    std::string comment;
    if (comment_pos != std::string::npos) {
        before_comment = trimmed.substr(0, comment_pos);
        comment = trimmed.substr(comment_pos);
    }

    // Trim trailing whitespace from before_comment
    size_t end = before_comment.find_last_not_of(" \t\r\n");
    if (end != std::string::npos) before_comment = before_comment.substr(0, end + 1);

    if (before_comment.empty()) {
        if (!comment.empty())
            return ";" + comment.substr(1) + "\n";
        return line;
    }

    // --- Directives ---
    if (before_comment[0] == '.') {
        // .globl sym → PUBLIC sym
        if (before_comment.compare(0, 6, ".globl") == 0 || before_comment.compare(0, 7, ".global") == 0) {
            size_t space = before_comment.find_first_of(" \t");
            if (space != std::string::npos) {
                std::string sym = before_comment.substr(space + 1);
                size_t sym_end = sym.find_last_not_of(" \t");
                if (sym_end != std::string::npos) sym = sym.substr(0, sym_end + 1);
                return "\tPUBLIC " + sym + "\n";
            }
            return "\n";
        }
        // .text → .code
        if (before_comment == ".text")
            return "\t.code\n";
        // .data → .data
        if (before_comment == ".data")
            return "\t.data\n";
        // .align N → ALIGN N
        if (before_comment.compare(0, 6, ".align") == 0) {
            std::string rest = before_comment.substr(6);
            size_t space = rest.find_first_not_of(" \t");
            if (space != std::string::npos) rest = rest.substr(space);
            return "\tALIGN " + rest + "\n";
        }
        // .byte V1, V2, ... → DB V1, V2, ...
        if (before_comment.compare(0, 5, ".byte") == 0) {
            std::string rest = before_comment.substr(5);
            size_t space = rest.find_first_not_of(" \t");
            if (space != std::string::npos) rest = rest.substr(space);
            return "\tDB " + rest + "\n";
        }
        // .long V → DD V
        if (before_comment.compare(0, 5, ".long") == 0) {
            std::string rest = before_comment.substr(5);
            size_t space = rest.find_first_not_of(" \t");
            if (space != std::string::npos) rest = rest.substr(space);
            return "\tDD " + rest + "\n";
        }
        // .quad V → DQ V
        if (before_comment.compare(0, 5, ".quad") == 0) {
            std::string rest = before_comment.substr(5);
            size_t space = rest.find_first_not_of(" \t");
            if (space != std::string::npos) rest = rest.substr(space);
            return "\tDQ " + rest + "\n";
        }
        // .zero N → DB N DUP (0)
        if (before_comment.compare(0, 5, ".zero") == 0) {
            std::string rest = before_comment.substr(5);
            size_t space = rest.find_first_not_of(" \t");
            if (space != std::string::npos) rest = rest.substr(space);
            return "\tDB " + rest + " DUP (0)\n";
        }
        // .section → attempt passthrough
        if (before_comment.compare(0, 8, ".section") == 0) {
            std::string rest = before_comment.substr(8);
            size_t space = rest.find_first_not_of(" \t");
            if (space != std::string::npos) rest = rest.substr(space);
            // Skip .section for now (ml64 uses .code, .data, .const, etc.)
            return "\n";
        }
        // .type, .size, .cfi_* — drop these
        if (before_comment.compare(0, 5, ".type") == 0 ||
            before_comment.compare(0, 5, ".size") == 0 ||
            before_comment.compare(0, 4, ".cfi") == 0 ||
            before_comment.compare(0, 6, ".ident") == 0 ||
            before_comment.compare(0, 5, ".file") == 0) {
            return "\n";
        }
        // Unknown directive — keep as comment for debugging
        return ";" + before_comment + "\n";
    }

    // --- Labels ---
    // Labels end with ':'
    size_t colon = before_comment.find(':');
    if (colon != std::string::npos && colon == before_comment.size() - 1) {
        std::string label = before_comment.substr(0, colon);
        size_t label_end = label.find_last_not_of(" \t");
        if (label_end != std::string::npos) label = label.substr(0, label_end + 1);
        return label + ":\n";
    }

    // --- Instructions (lines starting with tab) ---
    if (before_comment[0] == '\t' || before_comment[0] == ' ') {
        std::string instr_line = before_comment;
        size_t first_nonspace = instr_line.find_first_not_of(" \t");
        if (first_nonspace != std::string::npos)
            instr_line = instr_line.substr(first_nonspace);

        // Split into mnemonic and operands
        size_t space_pos = instr_line.find_first_of(" \t");
        std::string mnemonic;
        std::string operands_str;
        if (space_pos == std::string::npos) {
            mnemonic = instr_line;
        } else {
            mnemonic = instr_line.substr(0, space_pos);
            operands_str = instr_line.substr(space_pos + 1);
        }

        if (mnemonic.empty()) return "\n";

        // Strip instruction size suffix
        std::string intel_mnemonic = strip_suffix(mnemonic);

        // Parse comma-separated operands
        std::vector<std::string> gas_operands;
        if (!operands_str.empty()) {
            std::string ops = operands_str;
            size_t ops_end = ops.find_last_not_of(" \t");
            if (ops_end != std::string::npos) ops = ops.substr(0, ops_end + 1);

            // Split by commas, respecting parentheses nesting
            int paren_depth = 0;
            size_t last_split = 0;
            for (size_t i = 0; i < ops.size(); i++) {
                if (ops[i] == '(') paren_depth++;
                else if (ops[i] == ')') paren_depth--;
                else if (ops[i] == ',' && paren_depth == 0) {
                    gas_operands.push_back(ops.substr(last_split, i - last_split));
                    last_split = i + 1;
                }
            }
            if (last_split < ops.size())
                gas_operands.push_back(ops.substr(last_split));
        }

        // Strip whitespace from each operand
        for (auto& op : gas_operands) {
            size_t s = op.find_first_not_of(" \t");
            size_t e = op.find_last_not_of(" \t");
            if (s != std::string::npos && e != std::string::npos)
                op = op.substr(s, e - s + 1);
        }

        // Convert operands from GAS to Intel
        std::vector<std::string> intel_operands;
        for (const auto& op : gas_operands)
            intel_operands.push_back(convert_operand(op));

        // Determine if we need PTR specifiers for memory operands
        // In MASM, when the destination is memory and the source is immediate,
        // we need a size specifier like QWORD PTR
        std::string ptr_hint;
        if (intel_operands.size() >= 2) {
            bool has_imm = false;
            bool has_mem = false;
            for (const auto& op : intel_operands) {
                if (is_memory_ref(op)) has_mem = true;
                // Check if any operand is a plain number (immediate)
                if (!op.empty() && (op[0] == '-' || (op[0] >= '0' && op[0] <= '9')))
                    has_imm = true;
            }
            if (has_mem && has_imm) {
                const char* ptr = size_suffix_to_ptr(mnemonic);
                if (ptr) ptr_hint = ptr;
            }
        }

        // Build Intel instruction line
        std::string result = "\t" + intel_mnemonic;

        if (!intel_operands.empty()) {
            result += " ";

            // Two-operand AT&T instructions need operand reversal
            // Single-operand instructions (jmp, call, ret, push, pop, etc.) stay
            static const char* single_op_insns[] = {
                "jmp", "call", "ret", "push", "pop", "cqto", "cltd",
                "syscall", "int", "nop", "leave",
                "div", "idiv", "neg", "not",
                "je", "jne", "jz", "jnz", "jg", "jl", "jge", "jle",
                "ja", "jb", "jae", "jbe", "jo", "jno", "js", "jns",
                "jp", "jnp", "jpe", "jpo",
                "sete", "setne", "setz", "setnz",
                "setg", "setl", "setge", "setle",
                "seta", "setb", "setae", "setbe",
                "seto", "setno", "sets", "setns",
                "setp", "setnp",
            };
            bool is_single = false;
            auto it = std::find(std::begin(single_op_insns), std::end(single_op_insns), intel_mnemonic);
            if (it != std::end(single_op_insns))
                is_single = true;

            if (intel_operands.size() == 2 && !is_single) {
                // Reverse operands for AT&T → Intel
                const std::string& src = intel_operands[0];
                const std::string& dst = intel_operands[1];

                // Add PTR hint to the memory operand if needed
                if (!ptr_hint.empty()) {
                    if (is_memory_ref(src))
                        result += ptr_hint;
                    result += src + ", ";
                    if (is_memory_ref(dst))
                        result += ptr_hint;
                    result += dst;
                } else {
                    result += dst + ", " + src;
                }
            } else {
                for (size_t i = 0; i < intel_operands.size(); i++) {
                    if (i > 0) result += ", ";
                    if (!ptr_hint.empty() && is_memory_ref(intel_operands[i]))
                        result += ptr_hint;
                    result += intel_operands[i];
                }
            }
        }

        result += "\n";
        return result;
    }

    // Fallthrough — keep line as-is
    return line;
}

// Convert GAS assembly file to MASM syntax
static bool gas_to_masm(const std::string& gas_path, const std::string& masm_path) {
    std::ifstream gas_file(gas_path);
    if (!gas_file.is_open()) {
        std::cerr << "Error: Could not open GAS assembly file: " << gas_path << std::endl;
        return false;
    }

    std::ofstream masm_file(masm_path);
    if (!masm_file.is_open()) {
        std::cerr << "Error: Could not create MASM assembly file: " << masm_path << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(gas_file, line)) {
        masm_file << convert_gas_line(line);
    }

    // MASM requires an END directive at the end
    masm_file << "\tEND\n";

    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool ProtonWindowsBridge::find_assembler(std::string& out_assembler) {
#ifdef _WIN32
    // On Windows with MSVC, use ml64.exe (MASM)
    std::string check = "where ml64 >nul 2>&1";
    if (system(check.c_str()) == 0) {
        out_assembler = "ml64";
        return true;
    }
    // Fallback: check for cl.exe (could use cl's assembler)
    check = "where cl >nul 2>&1";
    if (system(check.c_str()) == 0) {
        out_assembler = "cl";
        return true;
    }
    return false;
#else
    // On Unix, use GNU assembler
    std::string check = "which as > /dev/null 2>&1";
    if (system(check.c_str()) == 0) {
        out_assembler = "as";
        return true;
    }
    return false;
#endif
}

bool ProtonWindowsBridge::assemble_object(const std::string& asm_path, const std::string& obj_path) {
    std::string assembler;
    if (!find_assembler(assembler)) {
        std::cerr << "Error: No assembler found." << std::endl;
#ifdef _WIN32
        std::cerr << "  MSVC assembler (ml64.exe) not found. Ensure Visual Studio is installed." << std::endl;
#else
        std::cerr << "  Install binutils (as) for your platform." << std::endl;
#endif
        return false;
    }

#ifdef _WIN32
    // On Windows: convert GAS → MASM, then assemble with ml64.exe
    // Generate .asm path from .s path
    std::string masm_path = asm_path;
    size_t dot = masm_path.rfind('.');
    if (dot != std::string::npos)
        masm_path = masm_path.substr(0, dot) + ".asm";
    else
        masm_path += ".asm";

    if (!gas_to_masm(asm_path, masm_path)) {
        std::cerr << "Error: GAS-to-MASM conversion failed" << std::endl;
        return false;
    }

    std::string as_cmd;
    if (assembler == "ml64") {
        as_cmd = "ml64 /nologo /c /Fo\"" + obj_path + "\" \"" + masm_path + "\"";
    } else {
        // cl fallback — not ideal but try
        as_cmd = "cl /nologo /c /Fo\"" + obj_path + "\" \"" + masm_path + "\"";
    }

    int result = system(as_cmd.c_str());
    if (result != 0) {
        std::cerr << "Error: MASM assembly failed for " << masm_path << std::endl;
        return false;
    }

    // Clean up .asm file
    std::remove(masm_path.c_str());
#else
    // On Unix, use as directly
    std::string as_cmd = assembler + " -o \"" + obj_path + "\" \"" + asm_path + "\"";
    int result = system(as_cmd.c_str());
    if (result != 0) {
        std::cerr << "Error: assembler failed for " << asm_path << std::endl;
        return false;
    }
#endif

    return true;
}

std::string ProtonWindowsBridge::proton_lib_name() {
#ifdef _WIN32
    return "proton.lib";
#else
    return "libproton.a";
#endif
}

std::string ProtonWindowsBridge::object_extension() {
#ifdef _WIN32
    return ".obj";
#else
    return ".o";
#endif
}

bool ProtonWindowsBridge::is_windows_msvc() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

} // namespace aot
} // namespace neutron
