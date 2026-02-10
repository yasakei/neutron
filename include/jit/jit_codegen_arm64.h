#ifndef NEUTRON_JIT_CODEGEN_ARM64_H
#define NEUTRON_JIT_CODEGEN_ARM64_H

#include <cstdint>
#include <vector>
#include <cstring>

namespace neutron::jit {

/**
 * ARM64 (AArch64) code generation for JIT compiler.
 * 
 * Register mapping:
 *   X0       = first argument / temp
 *   X1       = temp (constant loading)
 *   X9-X15   = scratch registers
 *   X19      = locals pointer (callee-saved)
 *   X20      = ExecutionFrame* (callee-saved)
 *   X21-X24  = global register cache (callee-saved)
 *   D0-D14   = operand stack (D8-D14 are callee-saved, saved in prologue)
 *   D15      = scratch FP register
 */
class AArch64CodeGen {
public:
    AArch64CodeGen() = default;
    ~AArch64CodeGen() = default;

    // Helper to emit a 32-bit instruction (little-endian)
    static void emit32(std::vector<uint8_t>& code, uint32_t instr) {
        code.push_back(instr & 0xFF);
        code.push_back((instr >> 8) & 0xFF);
        code.push_back((instr >> 16) & 0xFF);
        code.push_back((instr >> 24) & 0xFF);
    }

    // ======================== GP Register Operations ========================

    // MOV Xd, Xn  (ORR Xd, XZR, Xn)
    static void emitMovReg64(std::vector<uint8_t>& code, uint8_t dst, uint8_t src) {
        // ORR Xd, XZR, Xm => 0xAA000000 | (Xm << 16) | (XZR << 5) | Xd
        emit32(code, 0xAA0003E0 | ((uint32_t)src << 16) | dst);
    }

    // MOV Xd, #imm64 using MOVZ + up to 3 MOVK
    static void emitMovImm64(std::vector<uint8_t>& code, uint8_t reg, uint64_t imm) {
        // MOVZ Xd, #imm16, LSL #0
        uint16_t imm0 = imm & 0xFFFF;
        emit32(code, 0xD2800000 | ((uint32_t)imm0 << 5) | reg);
        
        uint16_t imm1 = (imm >> 16) & 0xFFFF;
        if (imm1 != 0) {
            // MOVK Xd, #imm16, LSL #16
            emit32(code, 0xF2A00000 | ((uint32_t)imm1 << 5) | reg);
        }
        uint16_t imm2 = (imm >> 32) & 0xFFFF;
        if (imm2 != 0) {
            // MOVK Xd, #imm16, LSL #32
            emit32(code, 0xF2C00000 | ((uint32_t)imm2 << 5) | reg);
        }
        uint16_t imm3 = (imm >> 48) & 0xFFFF;
        if (imm3 != 0) {
            // MOVK Xd, #imm16, LSL #48
            emit32(code, 0xF2E00000 | ((uint32_t)imm3 << 5) | reg);
        }
    }

    // LDR Xd, [Xn, #imm]  (unsigned offset, scaled by 8)
    static void emitLdrReg64(std::vector<uint8_t>& code, uint8_t dst, uint8_t base, int32_t offset) {
        if (offset >= 0 && (offset & 7) == 0 && offset < 32768) {
            // LDR Xd, [Xn, #pimm] — unsigned scaled offset
            uint32_t imm12 = (offset / 8) & 0xFFF;
            emit32(code, 0xF9400000 | (imm12 << 10) | ((uint32_t)base << 5) | dst);
        } else {
            // LDUR Xd, [Xn, #simm9] — unscaled offset
            uint32_t imm9 = offset & 0x1FF;
            emit32(code, 0xF8400000 | (imm9 << 12) | ((uint32_t)base << 5) | dst);
        }
    }

    // STR Xd, [Xn, #imm]  (unsigned offset, scaled by 8)
    static void emitStrReg64(std::vector<uint8_t>& code, uint8_t src, uint8_t base, int32_t offset) {
        if (offset >= 0 && (offset & 7) == 0 && offset < 32768) {
            uint32_t imm12 = (offset / 8) & 0xFFF;
            emit32(code, 0xF9000000 | (imm12 << 10) | ((uint32_t)base << 5) | src);
        } else {
            uint32_t imm9 = offset & 0x1FF;
            emit32(code, 0xF8000000 | (imm9 << 12) | ((uint32_t)base << 5) | src);
        }
    }

    // STP Xt1, Xt2, [Xn, #imm]!  (pre-index store pair, for prologue)
    static void emitStpPre(std::vector<uint8_t>& code, uint8_t rt1, uint8_t rt2, uint8_t base, int32_t offset) {
        // STP Xt1, Xt2, [Xn, #imm]!  =>  0xA9800000 | imm7<<15 | Rt2<<10 | Rn<<5 | Rt1
        int32_t imm7 = (offset / 8) & 0x7F;
        emit32(code, 0xA9800000 | (imm7 << 15) | ((uint32_t)rt2 << 10) | ((uint32_t)base << 5) | rt1);
    }

    // LDP Xt1, Xt2, [Xn], #imm  (post-index load pair, for epilogue)
    static void emitLdpPost(std::vector<uint8_t>& code, uint8_t rt1, uint8_t rt2, uint8_t base, int32_t offset) {
        int32_t imm7 = (offset / 8) & 0x7F;
        emit32(code, 0xA8C00000 | (imm7 << 15) | ((uint32_t)rt2 << 10) | ((uint32_t)base << 5) | rt1);
    }

    // STP Dt1, Dt2, [Xn, #imm]!  (pre-index FP store pair)
    static void emitStpFpPre(std::vector<uint8_t>& code, uint8_t dt1, uint8_t dt2, uint8_t base, int32_t offset) {
        int32_t imm7 = (offset / 8) & 0x7F;
        emit32(code, 0x6D800000 | (imm7 << 15) | ((uint32_t)dt2 << 10) | ((uint32_t)base << 5) | dt1);
    }

    // LDP Dt1, Dt2, [Xn], #imm  (post-index FP load pair)
    static void emitLdpFpPost(std::vector<uint8_t>& code, uint8_t dt1, uint8_t dt2, uint8_t base, int32_t offset) {
        int32_t imm7 = (offset / 8) & 0x7F;
        emit32(code, 0x6CC00000 | (imm7 << 15) | ((uint32_t)dt2 << 10) | ((uint32_t)base << 5) | dt1);
    }

    // SUB Xd, Xn, #imm12
    static void emitSubImm(std::vector<uint8_t>& code, uint8_t dst, uint8_t src, uint32_t imm) {
        emit32(code, 0xD1000000 | ((imm & 0xFFF) << 10) | ((uint32_t)src << 5) | dst);
    }

    // ADD Xd, Xn, #imm12
    static void emitAddImm(std::vector<uint8_t>& code, uint8_t dst, uint8_t src, uint32_t imm) {
        emit32(code, 0x91000000 | ((imm & 0xFFF) << 10) | ((uint32_t)src << 5) | dst);
    }

    // RET (return via X30/LR)
    static void emitRet(std::vector<uint8_t>& code) {
        emit32(code, 0xD65F03C0);
    }

    // ======================== FP/Double Operations ========================

    // LDR Dd, [Xn, #imm]  (load double, unsigned scaled offset)
    static void emitLdrFp64(std::vector<uint8_t>& code, uint8_t dd, uint8_t base, int32_t offset) {
        if (offset >= 0 && (offset & 7) == 0 && offset < 32768) {
            uint32_t imm12 = (offset / 8) & 0xFFF;
            emit32(code, 0xFD400000 | (imm12 << 10) | ((uint32_t)base << 5) | dd);
        } else {
            // LDUR Dd, [Xn, #simm9]
            uint32_t imm9 = offset & 0x1FF;
            emit32(code, 0xFC400000 | (imm9 << 12) | ((uint32_t)base << 5) | dd);
        }
    }

    // STR Dd, [Xn, #imm]  (store double, unsigned scaled offset)
    static void emitStrFp64(std::vector<uint8_t>& code, uint8_t dd, uint8_t base, int32_t offset) {
        if (offset >= 0 && (offset & 7) == 0 && offset < 32768) {
            uint32_t imm12 = (offset / 8) & 0xFFF;
            emit32(code, 0xFD000000 | (imm12 << 10) | ((uint32_t)base << 5) | dd);
        } else {
            // STUR Dd, [Xn, #simm9]
            uint32_t imm9 = offset & 0x1FF;
            emit32(code, 0xFC000000 | (imm9 << 12) | ((uint32_t)base << 5) | dd);
        }
    }

    // FMOV Dd, Dn
    static void emitFmovDD(std::vector<uint8_t>& code, uint8_t dst, uint8_t src) {
        emit32(code, 0x1E604000 | ((uint32_t)src << 5) | dst);
    }

    // FMOV Dd, Xn (move GP to FP, 64-bit)
    static void emitFmovFromGP(std::vector<uint8_t>& code, uint8_t dd, uint8_t xn) {
        // FMOV Dd, Xn  =>  0x9E670000 | (Xn << 5) | Dd
        emit32(code, 0x9E670000 | ((uint32_t)xn << 5) | dd);
    }

    // FADD Dd, Dn, Dm
    static void emitFaddD(std::vector<uint8_t>& code, uint8_t dst, uint8_t src1, uint8_t src2) {
        emit32(code, 0x1E602800 | ((uint32_t)src2 << 16) | ((uint32_t)src1 << 5) | dst);
    }

    // FSUB Dd, Dn, Dm
    static void emitFsubD(std::vector<uint8_t>& code, uint8_t dst, uint8_t src1, uint8_t src2) {
        emit32(code, 0x1E603800 | ((uint32_t)src2 << 16) | ((uint32_t)src1 << 5) | dst);
    }

    // FMUL Dd, Dn, Dm
    static void emitFmulD(std::vector<uint8_t>& code, uint8_t dst, uint8_t src1, uint8_t src2) {
        emit32(code, 0x1E600800 | ((uint32_t)src2 << 16) | ((uint32_t)src1 << 5) | dst);
    }

    // FDIV Dd, Dn, Dm
    static void emitFdivD(std::vector<uint8_t>& code, uint8_t dst, uint8_t src1, uint8_t src2) {
        emit32(code, 0x1E601800 | ((uint32_t)src2 << 16) | ((uint32_t)src1 << 5) | dst);
    }

    // FNEG Dd, Dn
    static void emitFnegD(std::vector<uint8_t>& code, uint8_t dst, uint8_t src) {
        emit32(code, 0x1E614000 | ((uint32_t)src << 5) | dst);
    }

    // FCMP Dn, Dm (sets NZCV flags)
    static void emitFcmpD(std::vector<uint8_t>& code, uint8_t dn, uint8_t dm) {
        emit32(code, 0x1E602000 | ((uint32_t)dm << 16) | ((uint32_t)dn << 5));
    }

    // ======================== Conversion Operations ========================

    // FCVTZS Xd, Dn (FP double to signed int64, round toward zero)
    static void emitFcvtzsXD(std::vector<uint8_t>& code, uint8_t xd, uint8_t dn) {
        emit32(code, 0x9E780000 | ((uint32_t)dn << 5) | xd);
    }

    // SCVTF Dd, Xn (signed int64 to FP double)
    static void emitScvtfDX(std::vector<uint8_t>& code, uint8_t dd, uint8_t xn) {
        emit32(code, 0x9E620000 | ((uint32_t)xn << 5) | dd);
    }

    // ======================== Bitwise Operations (GP) ========================

    // AND Xd, Xn, Xm
    static void emitAndReg64(std::vector<uint8_t>& code, uint8_t dst, uint8_t src1, uint8_t src2) {
        emit32(code, 0x8A000000 | ((uint32_t)src2 << 16) | ((uint32_t)src1 << 5) | dst);
    }

    // ORR Xd, Xn, Xm
    static void emitOrrReg64(std::vector<uint8_t>& code, uint8_t dst, uint8_t src1, uint8_t src2) {
        emit32(code, 0xAA000000 | ((uint32_t)src2 << 16) | ((uint32_t)src1 << 5) | dst);
    }

    // EOR Xd, Xn, Xm (XOR)
    static void emitEorReg64(std::vector<uint8_t>& code, uint8_t dst, uint8_t src1, uint8_t src2) {
        emit32(code, 0xCA000000 | ((uint32_t)src2 << 16) | ((uint32_t)src1 << 5) | dst);
    }

    // LSL Xd, Xn, Xm (variable shift left)
    static void emitLslReg64(std::vector<uint8_t>& code, uint8_t dst, uint8_t src1, uint8_t src2) {
        // LSLV Xd, Xn, Xm
        emit32(code, 0x9AC02000 | ((uint32_t)src2 << 16) | ((uint32_t)src1 << 5) | dst);
    }

    // ASR Xd, Xn, Xm (arithmetic shift right)
    static void emitAsrReg64(std::vector<uint8_t>& code, uint8_t dst, uint8_t src1, uint8_t src2) {
        // ASRV Xd, Xn, Xm
        emit32(code, 0x9AC02800 | ((uint32_t)src2 << 16) | ((uint32_t)src1 << 5) | dst);
    }

    // MVN Xd, Xn (bitwise NOT)
    static void emitMvnReg64(std::vector<uint8_t>& code, uint8_t dst, uint8_t src) {
        // ORN Xd, XZR, Xn
        emit32(code, 0xAA200000 | ((uint32_t)src << 16) | (31 << 5) | dst);
    }

    // ======================== Branch Operations ========================

    // B <offset>  (unconditional branch, offset in bytes, PC-relative)
    // offset must be 4-byte aligned, range ±128MB
    static void emitB(std::vector<uint8_t>& code, int32_t byte_offset) {
        int32_t imm26 = (byte_offset / 4) & 0x3FFFFFF;
        emit32(code, 0x14000000 | imm26);
    }

    // B.cond <offset>  (conditional branch, offset in bytes)
    // Condition codes:
    //   0x0 = EQ, 0x1 = NE, 0x2 = CS/HS, 0x3 = CC/LO
    //   0x8 = HI, 0x9 = LS, 0xA = GE, 0xB = LT
    //   0xC = GT, 0xD = LE
    // For FP comparisons after FCMP:
    //   GE (0xA) = >=,  LT (0xB) = <
    //   GT (0xC) = >,   LE (0xD) = <=
    //   EQ (0x0) = ==,  NE (0x1) = !=
    static void emitBCond(std::vector<uint8_t>& code, uint8_t cond, int32_t byte_offset) {
        int32_t imm19 = ((byte_offset / 4) & 0x7FFFF);
        emit32(code, 0x54000000 | (imm19 << 5) | cond);
    }

    // Condition code constants for FP comparisons
    static constexpr uint8_t COND_EQ = 0x0;   // Equal
    static constexpr uint8_t COND_NE = 0x1;   // Not equal
    static constexpr uint8_t COND_HS = 0x2;   // Unsigned >= (carry set) — DO NOT use for FP comparisons
    static constexpr uint8_t COND_LO = 0x3;   // Unsigned <  (carry clear) — DO NOT use for FP comparisons
    static constexpr uint8_t COND_HI = 0x8;   // Unsigned >  — DO NOT use for FP comparisons
    static constexpr uint8_t COND_LS = 0x9;   // Unsigned <= — DO NOT use for FP comparisons
    static constexpr uint8_t COND_GE = 0xA;   // Signed >= (use for FP: not less)
    static constexpr uint8_t COND_LT = 0xB;   // Signed <  (use for FP: less)
    static constexpr uint8_t COND_GT = 0xC;   // Signed >  (use for FP: greater)
    static constexpr uint8_t COND_LE = 0xD;   // Signed <= (use for FP: not greater)

    // NOP
    static void emitNop(std::vector<uint8_t>& code) {
        emit32(code, 0xD503201F);
    }

    // ======================== Patch Helpers ========================

    // Patch a B instruction at the given code offset
    static void patchB(std::vector<uint8_t>& code, size_t offset, int32_t byte_offset) {
        int32_t imm26 = (byte_offset / 4) & 0x3FFFFFF;
        uint32_t instr = 0x14000000 | imm26;
        code[offset]     = instr & 0xFF;
        code[offset + 1] = (instr >> 8) & 0xFF;
        code[offset + 2] = (instr >> 16) & 0xFF;
        code[offset + 3] = (instr >> 24) & 0xFF;
    }

    // Patch a B.cond instruction at the given code offset
    static void patchBCond(std::vector<uint8_t>& code, size_t offset, uint8_t cond, int32_t byte_offset) {
        int32_t imm19 = ((byte_offset / 4) & 0x7FFFF);
        uint32_t instr = 0x54000000 | (imm19 << 5) | cond;
        code[offset]     = instr & 0xFF;
        code[offset + 1] = (instr >> 8) & 0xFF;
        code[offset + 2] = (instr >> 16) & 0xFF;
        code[offset + 3] = (instr >> 24) & 0xFF;
    }
};

} // namespace neutron::jit

#endif // NEUTRON_JIT_CODEGEN_ARM64_H
