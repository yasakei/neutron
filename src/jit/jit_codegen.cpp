#include "jit/jit_codegen.h"
#include <cstring>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
#endif

namespace neutron::jit {

X86_64CodeGen::X86_64CodeGen() = default;

X86_64CodeGen::~X86_64CodeGen() = default;

void X86_64CodeGen::emitMovReg64Imm64(std::vector<uint8_t>& code, uint8_t reg, uint64_t imm) {
    // MOV r64, imm64
    // Encoding: REX.W + B8 + rd + imm64
    emitRexPrefix(code, true, false, false, (reg & 8) != 0);
    code.push_back(0xB8 + (reg & 7));
    
    // Emit 64-bit immediate in little-endian
    code.push_back(imm & 0xFF);
    code.push_back((imm >> 8) & 0xFF);
    code.push_back((imm >> 16) & 0xFF);
    code.push_back((imm >> 24) & 0xFF);
    code.push_back((imm >> 32) & 0xFF);
    code.push_back((imm >> 40) & 0xFF);
    code.push_back((imm >> 48) & 0xFF);
    code.push_back((imm >> 56) & 0xFF);
}

void X86_64CodeGen::emitMovReg64Reg64(std::vector<uint8_t>& code, uint8_t dst_reg, uint8_t src_reg) {
    // MOV r64, r64
    // Encoding: REX.W + 89 + /r
    emitRexPrefix(code, true, (dst_reg & 8) != 0, false, (src_reg & 8) != 0);
    code.push_back(0x89);
    code.push_back(encodeModRM(3, src_reg & 7, dst_reg & 7));
}

void X86_64CodeGen::emitCallRel32(std::vector<uint8_t>& code, int32_t rel_offset) {
    // CALL rel32
    // Encoding: E8 cd
    code.push_back(0xE8);
    code.push_back(rel_offset & 0xFF);
    code.push_back((rel_offset >> 8) & 0xFF);
    code.push_back((rel_offset >> 16) & 0xFF);
    code.push_back((rel_offset >> 24) & 0xFF);
}

void X86_64CodeGen::emitCallReg(std::vector<uint8_t>& code, uint8_t reg) {
    // CALL r64
    // Encoding: FF /2
    emitRexPrefix(code, false, false, false, (reg & 8) != 0);
    code.push_back(0xFF);
    code.push_back(encodeModRM(3, 2, reg & 7));
}

void X86_64CodeGen::emitJmpRel32(std::vector<uint8_t>& code, int32_t rel_offset) {
    // JMP rel32
    // Encoding: E9 cd
    code.push_back(0xE9);
    code.push_back(rel_offset & 0xFF);
    code.push_back((rel_offset >> 8) & 0xFF);
    code.push_back((rel_offset >> 16) & 0xFF);
    code.push_back((rel_offset >> 24) & 0xFF);
}

void X86_64CodeGen::emitJeRel32(std::vector<uint8_t>& code, int32_t rel_offset) {
    // JE rel32
    // Encoding: 0F 84 cd
    code.push_back(0x0F);
    code.push_back(0x84);
    code.push_back(rel_offset & 0xFF);
    code.push_back((rel_offset >> 8) & 0xFF);
    code.push_back((rel_offset >> 16) & 0xFF);
    code.push_back((rel_offset >> 24) & 0xFF);
}

void X86_64CodeGen::emitCmpReg64Reg64(std::vector<uint8_t>& code, uint8_t reg1, uint8_t reg2) {
    // CMP r64, r64
    // Encoding: REX.W + 39 + /r
    emitRexPrefix(code, true, (reg1 & 8) != 0, false, (reg2 & 8) != 0);
    code.push_back(0x39);
    code.push_back(encodeModRM(3, reg2 & 7, reg1 & 7));
}

void X86_64CodeGen::emitAddReg64Reg64(std::vector<uint8_t>& code, uint8_t dst_reg, uint8_t src_reg) {
    // ADD r64, r64
    // Encoding: REX.W + 01 + /r
    emitRexPrefix(code, true, (dst_reg & 8) != 0, false, (src_reg & 8) != 0);
    code.push_back(0x01);
    code.push_back(encodeModRM(3, src_reg & 7, dst_reg & 7));
}

void X86_64CodeGen::emitRet(std::vector<uint8_t>& code) {
    // RET
    code.push_back(0xC3);
}

void X86_64CodeGen::emitPushReg64(std::vector<uint8_t>& code, uint8_t reg) {
    // PUSH r64
    // Encoding: 50+rd or REX.B + 50+rd
    if (reg >= 8) {
        emitRexPrefix(code, false, false, false, true);
    }
    code.push_back(0x50 + (reg & 7));
}

void X86_64CodeGen::emitPopReg64(std::vector<uint8_t>& code, uint8_t reg) {
    // POP r64
    // Encoding: 58+rd or REX.B + 58+rd
    if (reg >= 8) {
        emitRexPrefix(code, false, false, false, true);
    }
    code.push_back(0x58 + (reg & 7));
}

void X86_64CodeGen::emitAddSdXmmXmm(std::vector<uint8_t>& code, uint8_t dst_xmm, uint8_t src_xmm) {
    // ADDSD xmm, xmm
    // Encoding: F2 0F 58 /r
    code.push_back(0xF2);
    if (dst_xmm >= 8 || src_xmm >= 8) {
        emitRexPrefix(code, false, (dst_xmm & 8) != 0, false, (src_xmm & 8) != 0);
    }
    code.push_back(0x0F);
    code.push_back(0x58);
    code.push_back(encodeModRM(3, dst_xmm & 7, src_xmm & 7));
}

void X86_64CodeGen::emitSubSdXmmXmm(std::vector<uint8_t>& code, uint8_t dst_xmm, uint8_t src_xmm) {
    // SUBSD xmm, xmm
    // Encoding: F2 0F 5C /r
    code.push_back(0xF2);
    if (dst_xmm >= 8 || src_xmm >= 8) {
        emitRexPrefix(code, false, (dst_xmm & 8) != 0, false, (src_xmm & 8) != 0);
    }
    code.push_back(0x0F);
    code.push_back(0x5C);
    code.push_back(encodeModRM(3, dst_xmm & 7, src_xmm & 7));
}

void X86_64CodeGen::emitMulSdXmmXmm(std::vector<uint8_t>& code, uint8_t dst_xmm, uint8_t src_xmm) {
    // MULSD xmm, xmm
    // Encoding: F2 0F 59 /r
    code.push_back(0xF2);
    if (dst_xmm >= 8 || src_xmm >= 8) {
        emitRexPrefix(code, false, (dst_xmm & 8) != 0, false, (src_xmm & 8) != 0);
    }
    code.push_back(0x0F);
    code.push_back(0x59);
    code.push_back(encodeModRM(3, dst_xmm & 7, src_xmm & 7));
}

void X86_64CodeGen::emitDivSdXmmXmm(std::vector<uint8_t>& code, uint8_t dst_xmm, uint8_t src_xmm) {
    // DIVSD xmm, xmm
    // Encoding: F2 0F 5E /r
    code.push_back(0xF2);
    if (dst_xmm >= 8 || src_xmm >= 8) {
        emitRexPrefix(code, false, (dst_xmm & 8) != 0, false, (src_xmm & 8) != 0);
    }
    code.push_back(0x0F);
    code.push_back(0x5E);
    code.push_back(encodeModRM(3, dst_xmm & 7, src_xmm & 7));
}

void X86_64CodeGen::emitMovSdXmmMem(std::vector<uint8_t>& code, uint8_t dst_xmm, uint8_t base_reg, int32_t offset) {
    // MOVSD xmm, [mem]
    // Encoding: F2 0F 10 /r
    code.push_back(0xF2);
    // REX prefix if needed (W=0 usually, R=dst_xmm extension, B=base_reg extension)
    if (dst_xmm >= 8 || base_reg >= 8) {
        emitRexPrefix(code, false, (dst_xmm & 8) != 0, false, (base_reg & 8) != 0);
    }
    code.push_back(0x0F);
    code.push_back(0x10);
    
    // ModRM + Disp logic
    if (offset == 0 && (base_reg & 7) != 5) { // No displacement (except ebp/r13 special case)
        code.push_back(encodeModRM(0, dst_xmm & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.push_back(0x24); // SIB for RSP
    } else if (offset >= -128 && offset <= 127) { // 8-bit displacement
        code.push_back(encodeModRM(1, dst_xmm & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.push_back(0x24); // SIB for RSP
        code.push_back(static_cast<uint8_t>(offset));
    } else { // 32-bit displacement
        code.push_back(encodeModRM(2, dst_xmm & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.push_back(0x24); // SIB for RSP
        code.push_back(offset & 0xFF);
        code.push_back((offset >> 8) & 0xFF);
        code.push_back((offset >> 16) & 0xFF);
        code.push_back((offset >> 24) & 0xFF);
    }
}

void X86_64CodeGen::emitMovSdMemXmm(std::vector<uint8_t>& code, uint8_t base_reg, int32_t offset, uint8_t src_xmm) {
    // MOVSD [mem], xmm
    // Encoding: F2 0F 11 /r
    code.push_back(0xF2);
    if (src_xmm >= 8 || base_reg >= 8) {
        emitRexPrefix(code, false, (src_xmm & 8) != 0, false, (base_reg & 8) != 0);
    }
    code.push_back(0x0F);
    code.push_back(0x11);
    
    // ModRM + Disp logic (reg field is src_xmm, rm field is base_reg)
    if (offset == 0 && (base_reg & 7) != 5) {
        code.push_back(encodeModRM(0, src_xmm & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.push_back(0x24);
    } else if (offset >= -128 && offset <= 127) {
        code.push_back(encodeModRM(1, src_xmm & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.push_back(0x24);
        code.push_back(static_cast<uint8_t>(offset));
    } else {
        code.push_back(encodeModRM(2, src_xmm & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.push_back(0x24);
        code.push_back(offset & 0xFF);
        code.push_back((offset >> 8) & 0xFF);
        code.push_back((offset >> 16) & 0xFF);
        code.push_back((offset >> 24) & 0xFF);
    }
}

void X86_64CodeGen::emitUcomisdXmmXmm(std::vector<uint8_t>& code, uint8_t xmm1, uint8_t xmm2) {
    // UCOMISD xmm, xmm
    // Encoding: 66 0F 2E /r
    code.push_back(0x66);
    if (xmm1 >= 8 || xmm2 >= 8) {
        emitRexPrefix(code, false, (xmm1 & 8) != 0, false, (xmm2 & 8) != 0);
    }
    code.push_back(0x0F);
    code.push_back(0x2E);
    code.push_back(encodeModRM(3, xmm1 & 7, xmm2 & 7));
}

void X86_64CodeGen::emitMovReg64Mem(std::vector<uint8_t>& code, uint8_t dst_reg, uint8_t base_reg, int32_t offset) {
    // MOV r64, [mem]
    // Encoding: REX.W + 8B /r
    emitRexPrefix(code, true, (dst_reg & 8) != 0, false, (base_reg & 8) != 0);
    code.push_back(0x8B);

    if (offset == 0 && (base_reg & 7) != 5) {
        code.push_back(encodeModRM(0, dst_reg & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.push_back(0x24);
    } else if (offset >= -128 && offset <= 127) {
        code.push_back(encodeModRM(1, dst_reg & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.push_back(0x24);
        code.push_back(static_cast<uint8_t>(offset));
    } else {
        code.push_back(encodeModRM(2, dst_reg & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.push_back(0x24);
        code.push_back(offset & 0xFF);
        code.push_back((offset >> 8) & 0xFF);
        code.push_back((offset >> 16) & 0xFF);
        code.push_back((offset >> 24) & 0xFF);
    }
}

void X86_64CodeGen::emitMovMem64Reg(std::vector<uint8_t>& code, uint8_t base_reg, int32_t offset, uint8_t src_reg) {
    // MOV [mem], r64
    // Encoding: REX.W + 89 /r
    emitRexPrefix(code, true, (src_reg & 8) != 0, false, (base_reg & 8) != 0);
    code.push_back(0x89);

    if (offset == 0 && (base_reg & 7) != 5) {
        code.push_back(encodeModRM(0, src_reg & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.push_back(0x24);
    } else if (offset >= -128 && offset <= 127) {
        code.push_back(encodeModRM(1, src_reg & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.push_back(0x24);
        code.push_back(static_cast<uint8_t>(offset));
    } else {
        code.push_back(encodeModRM(2, src_reg & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.push_back(0x24);
        code.push_back(offset & 0xFF);
        code.push_back((offset >> 8) & 0xFF);
        code.push_back((offset >> 16) & 0xFF);
        code.push_back((offset >> 24) & 0xFF);
    }
}

void X86_64CodeGen::emitRexPrefix(std::vector<uint8_t>& code, bool w, bool r, bool x, bool b) {
    // REX prefix: 4X where:
    // bit 3: W (64-bit operand)
    // bit 2: R (extend ModRM.reg)
    // bit 1: X (extend SIB.index)
    // bit 0: B (extend ModRM.rm or SIB.base)
    uint8_t rex = 0x40;
    if (w) rex |= 0x08;
    if (r) rex |= 0x04;
    if (x) rex |= 0x02;
    if (b) rex |= 0x01;
    
    // Only emit if needed
    if (w || r || x || b) {
        code.push_back(rex);
    }
}

uint8_t X86_64CodeGen::encodeModRM(uint8_t mod, uint8_t reg, uint8_t rm) {
    // ModRM byte: mod(2) + reg(3) + rm(3)
    return (mod << 6) | (reg << 3) | rm;
}

bool X86_64CodeGen::makeExecutable(uint8_t* code_ptr, size_t code_size) {
#ifdef _WIN32
    // Windows: VirtualProtect
    DWORD old_protect;
    if (!VirtualProtect(code_ptr, code_size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return false;
    }
    return true;
#else
    // Unix: mprotect
    // Round down to page size
    size_t page_size = sysconf(_SC_PAGESIZE);
    uintptr_t page_start = (reinterpret_cast<uintptr_t>(code_ptr)) & ~(page_size - 1);
    size_t adjusted_size = code_size + (code_ptr - reinterpret_cast<uint8_t*>(page_start));
    
    int result = mprotect(reinterpret_cast<void*>(page_start), adjusted_size,
                          PROT_READ | PROT_WRITE | PROT_EXEC);
    
    if (result != 0) {
        return false;
    }
    
    // Flush instruction cache
    #if defined(__GNUC__)
        __builtin___clear_cache(reinterpret_cast<char*>(code_ptr),
                               reinterpret_cast<char*>(code_ptr + code_size));
    #endif
    
    return true;
#endif
}

} // namespace neutron::jit
