/*
 * Neutron Programming Language - AOT Compiler
 * Copyright (c) 2026 yasakei
 * 
 * Translates Neutron bytecode to native C++ code at compile time.
 */

#ifndef NEUTRON_AOT_COMPILER_H
#define NEUTRON_AOT_COMPILER_H

#include "compiler/bytecode.h"
#include "types/value.h"
#include <string>
#include <sstream>
#include <vector>

namespace neutron {
namespace aot {

class AotCompiler {
public:
    AotCompiler(const Chunk* chunk);
    
    // Generate native C++ code from bytecode
    std::string generateCode(const std::string& functionName = "main");
    
private:
    const Chunk* chunk;
    std::ostringstream code;
    size_t ip;  // Instruction pointer during code generation
    
    // Code generation helpers
    void generatePrologue(const std::string& functionName);
    void generateEpilogue();
    void generateBytecodeBody();
    
    // Instruction generators
    void emitConstant();
    void emitReturn();
    void emitArithmetic(OpCode op);
    void emitComparison(OpCode op);
    void emitLogic();
    void emitStackOps();
    void emitLocalVars();
    void emitGlobalVars();
    void emitJump();
    void emitCall();
    void emitSay();
    
    // Utility
    std::string constantToCpp(size_t index);
    std::string valueTypeToString(ValueType type);
    uint8_t readByte();
    uint16_t readShort();
};

} // namespace aot
} // namespace neutron

#endif // NEUTRON_AOT_COMPILER_H
