#ifndef NEUTRON_JIT_CODEGEN_H
#define NEUTRON_JIT_CODEGEN_H

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>

namespace neutron::jit {

/**
 * Architecture-specific code generation for x86-64
 * Generates native machine code from intermediate representation
 */
class X86_64CodeGen {
public:
    // X86-64 instruction encoding utilities
    enum class X86Opcode : uint16_t {
        MOV_REG_REG = 0x89,      // mov r64, r64
        MOV_REG_IMM = 0xB8,      // mov r64, imm64
        ADD_REG_REG = 0x01,      // add r64, r64
        SUB_REG_REG = 0x29,      // sub r64, r64
        IMUL_REG_REG = 0x0FAF,   // imul r64, r64
        CMP_REG_REG = 0x39,      // cmp r64, r64
        TEST_REG_REG = 0x85,     // test r64, r64
        JMP_REL32 = 0xE9,        // jmp rel32
        JE_REL32 = 0x0F84,       // je rel32
        JNE_REL32 = 0x0F85,      // jne rel32
        JL_REL32 = 0x0F8C,       // jl rel32
        JG_REL32 = 0x0F8F,       // jg rel32
        CALL_REL32 = 0xE8,       // call rel32
        CALL_REG = 0xFF,          // call r64
        RET = 0xC3,               // ret
        PUSH_REG = 0x50,          // push r64
        POP_REG = 0x58,           // pop r64
        LEA_REG_MEM = 0x8D,       // lea r64, [mem]
        MOV_MEM_REG = 0x89,       // mov [mem], r64
        MOV_REG_MEM = 0x8B,       // mov r64, [mem]
    };

    X86_64CodeGen();
    ~X86_64CodeGen();

    /**
     * Emit instruction: MOV r64, imm64
     */
    void emitMovReg64Imm64(std::vector<uint8_t>& code, uint8_t reg, uint64_t imm);

    /**
     * Emit instruction: MOV r64, r64
     */
    void emitMovReg64Reg64(std::vector<uint8_t>& code, uint8_t dst_reg, uint8_t src_reg);

    /**
     * Emit instruction: CALL rel32
     */
    void emitCallRel32(std::vector<uint8_t>& code, int32_t rel_offset);

    /**
     * Emit instruction: CALL r64
     */
    void emitCallReg(std::vector<uint8_t>& code, uint8_t reg);

    /**
     * Emit instruction: JMP rel32
     */
    void emitJmpRel32(std::vector<uint8_t>& code, int32_t rel_offset);

    /**
     * Emit instruction: JE rel32 (jump if equal)
     */
    void emitJeRel32(std::vector<uint8_t>& code, int32_t rel_offset);

    /**
     * Emit instruction: CMP r64, r64
     */
    void emitCmpReg64Reg64(std::vector<uint8_t>& code, uint8_t reg1, uint8_t reg2);

    /**
     * Emit instruction: ADD r64, r64
     */
    void emitAddReg64Reg64(std::vector<uint8_t>& code, uint8_t dst_reg, uint8_t src_reg);

    /**
     * Emit instruction: MOV r64, [mem]
     */
    void emitMovReg64Mem(std::vector<uint8_t>& code, uint8_t dst_reg, uint8_t base_reg, int32_t offset);

    /**
     * Emit instruction: MOV [mem], r64
     */
    void emitMovMem64Reg(std::vector<uint8_t>& code, uint8_t base_reg, int32_t offset, uint8_t src_reg);

    /**
     * Emit instruction: RET
     */
    void emitRet(std::vector<uint8_t>& code);

    /**
     * Emit instruction: PUSH r64
     */
    void emitPushReg64(std::vector<uint8_t>& code, uint8_t reg);

    /**
     * Emit instruction: POP r64
     */
    void emitPopReg64(std::vector<uint8_t>& code, uint8_t reg);

    // SSE2 Double Precision Instructions
    
    /**
     * Emit instruction: ADDSD xmm, xmm (Add Scalar Double)
     */
    void emitAddSdXmmXmm(std::vector<uint8_t>& code, uint8_t dst_xmm, uint8_t src_xmm);

    /**
     * Emit instruction: SUBSD xmm, xmm (Subtract Scalar Double)
     */
    void emitSubSdXmmXmm(std::vector<uint8_t>& code, uint8_t dst_xmm, uint8_t src_xmm);

    /**
     * Emit instruction: MULSD xmm, xmm (Multiply Scalar Double)
     */
    void emitMulSdXmmXmm(std::vector<uint8_t>& code, uint8_t dst_xmm, uint8_t src_xmm);

    /**
     * Emit instruction: DIVSD xmm, xmm (Divide Scalar Double)
     */
    void emitDivSdXmmXmm(std::vector<uint8_t>& code, uint8_t dst_xmm, uint8_t src_xmm);

    /**
     * Emit instruction: MOVSD xmm, [mem] (Load Scalar Double)
     * base_reg: register holding memory address
     * offset: displacement
     */
    void emitMovSdXmmMem(std::vector<uint8_t>& code, uint8_t dst_xmm, uint8_t base_reg, int32_t offset);

    /**
     * Emit instruction: MOVSD [mem], xmm (Store Scalar Double)
     * base_reg: register holding memory address
     * offset: displacement
     */
    void emitMovSdMemXmm(std::vector<uint8_t>& code, uint8_t base_reg, int32_t offset, uint8_t src_xmm);

    /**
     * Emit instruction: UCOMISD xmm, xmm (Unordered Compare Scalar Double)
     */
    void emitUcomisdXmmXmm(std::vector<uint8_t>& code, uint8_t xmm1, uint8_t xmm2);

    /**
     * Emit REX prefix for 64-bit operations
     */
    static void emitRexPrefix(std::vector<uint8_t>& code, bool w, bool r, bool x, bool b);

    /**
     * Encode ModRM byte
     */
    static uint8_t encodeModRM(uint8_t mod, uint8_t reg, uint8_t rm);

    /**
     * Make code executable on the platform
     */
    bool makeExecutable(uint8_t* code_ptr, size_t code_size);

private:
    // Register numbering for REX encoding
    enum class X86Reg : uint8_t {
        RAX = 0, RCX = 1, RDX = 2, RBX = 3,
        RSP = 4, RBP = 5, RSI = 6, RDI = 7,
        R8 = 8, R9 = 9, R10 = 10, R11 = 11,
        R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    };
};

} // namespace neutron::jit

#endif // NEUTRON_JIT_CODEGEN_H
